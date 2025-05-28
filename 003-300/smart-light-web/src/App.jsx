import { useState, useEffect, useCallback, useRef } from "react";
import axios from "axios";
import { SketchPicker } from "react-color";
import "./App.css";

function App() {
	const [power, setPower] = useState(false);
	const [brightness, setBrightness] = useState(50);
	const [color, setColor] = useState({ r: 255, g: 255, b: 255 });
	const [esp32Connected, setEsp32Connected] = useState(false);
	const [message, setMessage] = useState("");
	const [loading, setLoading] = useState(false);
	const [showColorPicker, setShowColorPicker] = useState(false);
	const intervalRef = useRef(null);

	const checkEsp32Status = useCallback(async () => {
		try {
			const response = await axios.get("/api/status");
			setEsp32Connected(response.data.esp32Connected);
		} catch (error) {
			console.error("Error fetching ESP32 status:", error);
			setEsp32Connected(false); // Assume disconnected on error
		}
	}, []);

	useEffect(() => {
		checkEsp32Status();
		const intervalId = setInterval(checkEsp32Status, 5000); // Check status every 5 seconds
		return () => clearInterval(intervalId);
	}, [checkEsp32Status]);

	// 在组件卸载时，清理定时器
	useEffect(() => {
		return () => {
			if (intervalRef.current) {
				clearInterval(intervalRef.current);
			}
		};
	}, []);

	const sendControlCommand = async (command) => {
		setLoading(true);
		setMessage("");
		try {
			const payload = { ...command };
			if (command.type === "color") {
				payload.r = color.r;
				payload.g = color.g;
				payload.b = color.b;
			}
			if (command.type === "brightness") {
				payload.value = brightness;
			}
			if (command.type === "power") {
				// 使用传入的 command.state，避免 React 状态异步更新导致出错
				payload.state = command.state;
			}

			console.log("Sending command:", payload);
			const response = await axios.post("/api/led", payload);
			console.log("Response:", response.data);
			if (response.data.result) {
				setMessage("Command sent successfully!");
			} else {
				setMessage(`Error: ${response.data.error || "Unknown error"}`);
			}
		} catch (error) {
			console.error("Error sending command:", error);
			setMessage(`Request failed: ${error.message}`);
			if (error.response) {
				console.error("Error response data:", error.response.data);
				setMessage(
					`Request failed: ${error.response.status} - ${
						error.response.data.error || error.message
					}`
				);
			}
		} finally {
			setLoading(false);
			checkEsp32Status(); // Re-check status after command
		}
	};

	const handlePowerToggle = () => {
		const newState = !power;
		setPower(newState);
		// 立即发送一次
		sendControlCommand({ type: "power", state: newState ? "on" : "off" });
		// 如果打开则启动递归定时发送保持命令
		if (newState && esp32Connected) {
			const sendOnLoop = async () => {
				await sendControlCommand({ type: "power", state: "on" });
				// 如果仍然开启，则继续下一次发送
				if (intervalRef.current) {
					intervalRef.current = setTimeout(sendOnLoop, 10000);
				}
			};
			// 标记定时器存在
			intervalRef.current = true;
			sendOnLoop();
		} else {
			// 关闭时清理定时器
			if (intervalRef.current) {
				clearTimeout(intervalRef.current);
				intervalRef.current = null;
			}
		}
	};

	const handleBrightnessChange = (event) => {
		const newBrightness = parseInt(event.target.value, 10);
		setBrightness(newBrightness);
		sendControlCommand({ type: "brightness", value: newBrightness });
	};

	const handleColorChange = (newColor) => {
		setColor(newColor.rgb);
		sendControlCommand({ type: "color", ...newColor.rgb });
	};

	return (
		<div className="container">
			<header>
				<h1>智能灯光控制</h1>
				<div
					className={`status-indicator ${
						esp32Connected ? "connected" : "disconnected"
					}`}
				>
					ESP32: {esp32Connected ? "已连接" : "未连接"}
				</div>
			</header>

			<div className="controls">
				<div className="control-card">
					<h2>电源</h2>
					<button
						onClick={handlePowerToggle}
						className={`power-button ${power ? "on" : "off"}`}
						disabled={!esp32Connected}
					>
						{power ? "关闭" : "开启"}
					</button>
				</div>

				<div className="control-card">
					<h2>亮度</h2>
					<div className="brightness-slider">
						<input
							type="range"
							min="0"
							max="100"
							value={brightness}
							onChange={handleBrightnessChange}
							disabled={!power || !esp32Connected}
						/>
						<span>{brightness}%</span>
					</div>
				</div>

				<div className="control-card">
					<h2>颜色</h2>
					<SketchPicker
						color={color}
						onChangeComplete={handleColorChange}
						disableAlpha={true}
						presetColors={[]}
						width="250px"
						className={!power || !esp32Connected ? "disabled-picker" : ""}
					/>
					{(!power || !esp32Connected) && (
						<div className="color-picker-disabled-overlay"></div>
					)}
				</div>
			</div>

			{loading && <p className="message loading">正在发送指令...</p>}
			{message && (
				<p
					className={`message ${
						message.startsWith("Error") || message.startsWith("Request failed")
							? "error"
							: "success"
					}`}
				>
					{message.startsWith("Command sent successfully!")
						? "指令发送成功！"
						: message}
				</p>
			)}

			<footer>
				<p>智能灯光系统 &copy; 2025</p>
			</footer>
		</div>
	);
}

export default App;
