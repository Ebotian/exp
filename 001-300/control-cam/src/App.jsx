import { useState, useRef } from "react";
import "./App.css";

// 配置你的 ESP32-CAM 流地址
const CAMERA_STREAM_URL = "http://192.168.229.184/stream"; // TODO: 替换为实际IP

function App() {
	const [saving, setSaving] = useState(false);
	const [message, setMessage] = useState("");
	const imgRef = useRef(null);

	// 控制指令发送
	const sendControl = async (cmd) => {
		setMessage("");
		try {
			const res = await fetch("/api/control", {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body: JSON.stringify({ command: cmd }),
			});
			const data = await res.json();
			setMessage(data.message || "指令已发送");
		} catch (e) {
			setMessage("指令发送失败");
			setTimeout(() => setMessage(""), 1000);
		}
	};

	// 保存当前帧到本地
	const saveImage = async () => {
		setSaving(true);
		setMessage("");
		console.log("开始保存图片");
		// 断开流
		const oldSrc = imgRef.current.src;
		imgRef.current.src = "";
		try {
			const res = await fetch("http://192.168.229.184/capture", {
				mode: "cors",
			});
			console.log("/capture fetch响应：", res);
			if (!res.ok) throw new Error("请求图片失败: " + res.status);
			const blob = await res.blob();
			console.log("图片blob获取成功");
			const url = URL.createObjectURL(blob);
			const a = document.createElement("a");
			a.href = url;
			a.download = `esp32-cam-${Date.now()}.jpg`;
			document.body.appendChild(a);
			a.click();
			document.body.removeChild(a);
			URL.revokeObjectURL(url);
			setMessage("图片已保存到本地");
		} catch (e) {
			console.error("保存图片异常", e);
			setMessage("保存失败: " + e.message);
			setTimeout(() => setMessage(""), 2000);
		} finally {
			// 恢复流
			imgRef.current.src = oldSrc;
			setSaving(false);
			console.log("保存流程结束");
		}
	};

	return (
		<div className="container">
			<h1>摄像头控制与实时预览</h1>
			<div className="video-section">
				<img
					ref={imgRef}
					src={CAMERA_STREAM_URL}
					alt="Camera Stream"
					className="camera-stream"
				/>
			</div>
			<div className="button-group">
				<button onClick={() => sendControl("LEFT")}>左转</button>
				<button onClick={() => sendControl("RIGHT")}>右转</button>
				<button onClick={() => sendControl("STOP")}>停止</button>
				<button onClick={() => sendControl("AUTO")}>自动</button>
				<button onClick={saveImage} disabled={saving}>
					{saving ? "保存中..." : "保存图片"}
				</button>
			</div>
			{message && <div className="message">{message}</div>}
		</div>
	);
}

export default App;
