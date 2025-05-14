import React, { useState } from "react";
import Login from "./components/Login";
import Dashboard from "./components/Dashboard";
import {
	connectMQTT,
	publishControl,
	disconnectMQTT,
} from "./services/mqttService";

const MQTT_HOST = "ws://39.107.106.220:8083/mqtt"; // 使用你的EMQX服务器公网IP和端口

function App() {
	const [loggedIn, setLoggedIn] = useState(false);
	const [status, setStatus] = useState("");
	const [user, setUser] = useState({ username: "" });

	const isAdmin = user.username === "admin";

	// 表单相关状态
	const [northSouth, setNorthSouth] = useState({
		red: 30,
		green: 25,
		yellow: 5,
	});
	const [eastWest, setEastWest] = useState({ red: 30, green: 25, yellow: 5 });
	const [timeSettings, setTimeSettings] = useState({
		morningStart: "07:00",
		morningEnd: "09:00",
		eveningStart: "17:00",
		eveningEnd: "19:00",
	});
	const [settingsList, setSettingsList] = useState([]);
	const [editModalOpen, setEditModalOpen] = useState(false);
	const [editItem, setEditItem] = useState(null);
	const [activeModeIndex, setActiveModeIndex] = useState(null); // 当前激活模式下标

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

	// 表单相关逻辑
	const handleSaveLight = (direction, color, value) => {
		const v = Number(value);
		if (direction === "northSouth") {
			setNorthSouth((prev) => ({ ...prev, [color]: v }));
		} else {
			setEastWest((prev) => ({ ...prev, [color]: v }));
		}
	};

	const handleTimeChange = (key, value) => {
		setTimeSettings((prev) => ({ ...prev, [key]: value }));
	};

	const handleSaveTime = () => {
		// 可扩展保存逻辑
	};

	const handleAddSetting = () => {
		const newSetting = {
			id: Date.now(),
			nsRed: northSouth.red,
			nsGreen: northSouth.green,
			nsYellow: northSouth.yellow,
			ewRed: eastWest.red,
			ewGreen: eastWest.green,
			ewYellow: eastWest.yellow,
			morningStart: timeSettings.morningStart,
			morningEnd: timeSettings.morningEnd,
			eveningStart: timeSettings.eveningStart,
			eveningEnd: timeSettings.eveningEnd,
		};
		setSettingsList((prev) => [...prev, newSetting]);
	};

	const handleEdit = (id) => {
		const item = settingsList.find((s) => s.id === id);
		if (item) {
			setEditItem(item);
			setEditModalOpen(true);
		}
	};

	const handleEditChange = (key, value) => {
		setEditItem((prev) => ({ ...prev, [key]: value }));
	};

	const handleEditSave = () => {
		setSettingsList((prev) =>
			prev.map((s) => (s.id === editItem.id ? editItem : s))
		);
		setEditModalOpen(false);
		setEditItem(null);
	};

	const handleEditCancel = () => {
		setEditModalOpen(false);
		setEditItem(null);
	};

	const handleDelete = (id) => {
		setSettingsList((prev) => prev.filter((s) => s.id !== id));
	};

	const handleActivate = (idx) => {
		setActiveModeIndex(idx);
		// 通过MQTT通知单片机端切换模式
		const modeSetting = settingsList[idx];
		if (modeSetting) {
			publishControl({ type: "set_mode", modeIndex: idx, ...modeSetting });
		}
	};

	if (!loggedIn) {
		return <Login onLogin={handleLogin} />;
	}

	return (
		<div>
			<Dashboard
				user={user}
				onLogout={handleLogout}
				northSouth={northSouth}
				eastWest={eastWest}
				onSaveLight={handleSaveLight}
				timeSettings={timeSettings}
				onTimeChange={handleTimeChange}
				onSaveTime={handleSaveTime}
				settingsList={settingsList}
				onEdit={handleEdit}
				onDelete={handleDelete}
				onAddSetting={handleAddSetting}
				editModalOpen={editModalOpen}
				editItem={editItem}
				onEditChange={handleEditChange}
				onEditSave={handleEditSave}
				onEditCancel={handleEditCancel}
				onActivate={handleActivate}
				activeModeIndex={activeModeIndex}
				isAdmin={isAdmin} // 新增
			/>
		</div>
	);
}

export default App;
