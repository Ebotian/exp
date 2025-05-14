import { useState } from "react";
import "./App.css";

// 配置你的 ESP32-CAM 流地址
const CAMERA_STREAM_URL = "http://<ESP32-CAM-IP>:81/stream"; // TODO: 替换为实际IP

function App() {
	const [saving, setSaving] = useState(false);
	const [message, setMessage] = useState("");

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

	// 保存当前帧
	const saveImage = async () => {
		setSaving(true);
		setMessage("");
		try {
			const res = await fetch("/api/save_image", { method: "POST" });
			const data = await res.json();
			setMessage(data.message || "图片已保存");
		} catch (e) {
			setMessage("保存失败");
			setTimeout(() => setMessage(""), 1000);
		}
		setSaving(false);
	};

	return (
		<div className="container">
			<h1>摄像头控制与实时预览</h1>
			<div className="video-section">
				<img
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
