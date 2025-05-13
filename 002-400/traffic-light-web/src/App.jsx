import React, { useState } from "react";
import Login from "./components/Login";
import Dashboard from "./components/Dashboard";
import StatusDisplay from "./components/StatusDisplay";
import TrafficLightControl from "./components/TrafficLightControl";
import {
	connectMQTT,
	publishControl,
	disconnectMQTT,
} from "./services/mqttService";

const MQTT_HOST = "wss://your-emqx-server:8084/mqtt"; // 替换为你的EMQX服务器地址

function App() {
	const [loggedIn, setLoggedIn] = useState(false);
	const [status, setStatus] = useState("");
	const [user, setUser] = useState({ username: "" });

	const handleLogin = (username, password) => {
		connectMQTT({
			host: MQTT_HOST,
			username,
			password,
			onMessage: (msg) => setStatus(msg.status),
		});
		setUser({ username });
		setLoggedIn(true);
	};

	const handleLogout = () => {
		disconnectMQTT();
		setLoggedIn(false);
		setUser({ username: "" });
		setStatus("");
	};

	const handleControl = (cmd) => {
		publishControl(cmd);
	};

	if (!loggedIn) {
		return <Login onLogin={handleLogin} />;
	}

	return (
		<div>
			<button onClick={handleLogout}>退出登录</button>
			<TrafficLightControl onControl={handleControl} />
			<StatusDisplay status={status} />
      {/* 你可以根据需要添加 Dashboard 或其他功能 */}
      <Dashboard user={user} />
		</div>
	);
}

export default App;
