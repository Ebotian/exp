import React, { useState, useEffect } from "react";
import Login from "./components/Login";
import Dashboard from "./components/Dashboard";
import {
	connectMQTT,
	publishControl,
	disconnectMQTT,
	publish,
} from "./services/mqttService";

const MQTT_HOST = "ws://39.107.106.220:8083/mqtt"; // 使用你的EMQX服务器公网IP和端口

function App() {
	const [loggedIn, setLoggedIn] = useState(false);
	const [status, setStatus] = useState({
		nsColor: "red",
		nsSeconds: 0,
		ewColor: "green",
		ewSeconds: 0,
	});
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

	// 读取本地存储
	useEffect(() => {
		const savedSettingsList = localStorage.getItem("settingsList");
		const savedTimeSettings = localStorage.getItem("timeSettings");
		const savedNorthSouth = localStorage.getItem("northSouth");
		const savedEastWest = localStorage.getItem("eastWest");
		const savedActiveModeIndex = localStorage.getItem("activeModeIndex");

		if (savedSettingsList) setSettingsList(JSON.parse(savedSettingsList));
		if (savedTimeSettings) setTimeSettings(JSON.parse(savedTimeSettings));
		if (savedNorthSouth) setNorthSouth(JSON.parse(savedNorthSouth));
		if (savedEastWest) setEastWest(JSON.parse(savedEastWest));
		if (savedActiveModeIndex) setActiveModeIndex(Number(savedActiveModeIndex));
	}, []);

	// 读取云端数据
	useEffect(() => {
		if (user.username) {
			fetch(`/api/user/settings?username=${user.username}`)
				.then((res) => res.json())
				.then((res) => {
					if (res.success && res.data) {
						if (res.data.settingsList) setSettingsList(res.data.settingsList);
						if (res.data.timeSettings) setTimeSettings(res.data.timeSettings);
						if (res.data.northSouth) setNorthSouth(res.data.northSouth);
						if (res.data.eastWest) setEastWest(res.data.eastWest);
						if (res.data.activeModeIndex !== undefined)
							setActiveModeIndex(res.data.activeModeIndex);
					}
				});
		}
		// eslint-disable-next-line
	}, [user.username]);

	// 保存到本地存储
	useEffect(() => {
		localStorage.setItem("settingsList", JSON.stringify(settingsList));
	}, [settingsList]);
	useEffect(() => {
		localStorage.setItem("timeSettings", JSON.stringify(timeSettings));
	}, [timeSettings]);
	useEffect(() => {
		localStorage.setItem("northSouth", JSON.stringify(northSouth));
	}, [northSouth]);
	useEffect(() => {
		localStorage.setItem("eastWest", JSON.stringify(eastWest));
	}, [eastWest]);
	useEffect(() => {
		localStorage.setItem(
			"activeModeIndex",
			activeModeIndex !== null ? String(activeModeIndex) : ""
		);
	}, [activeModeIndex]);

	// 保存到云端
	useEffect(() => {
		if (user.username) {
			fetch("/api/user/settings", {
				method: "POST",
				headers: { "Content-Type": "application/json" },
				body: JSON.stringify({
					username: user.username,
					settings: {
						settingsList,
						timeSettings,
						northSouth,
						eastWest,
						activeModeIndex,
					},
				}),
			});
		}
		// eslint-disable-next-line
	}, [
		settingsList,
		timeSettings,
		northSouth,
		eastWest,
		activeModeIndex,
		user.username,
	]);

	const handleLogin = (username, password) => {
		connectMQTT({
			host: MQTT_HOST,
			username,
			password,
			onMessage: (msg) => setStatus(msg),
		});
		setUser({ username });
		setLoggedIn(true);
	};

	const handleLogout = () => {
		disconnectMQTT();
		setLoggedIn(false);
		setUser({ username: "" });
		setStatus({
			nsColor: "red",
			nsSeconds: 0,
			ewColor: "green",
			ewSeconds: 0,
		});
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
		// 实时更新激活项并推送到mqtt
		const updatedList = settingsList.map((item, idx) =>
			idx === activeModeIndex
				? {
						...item,
						...(direction === "northSouth"
							? { [`ns${capitalize(color)}`]: v }
							: { [`ew${capitalize(color)}`]: v }),
				  }
				: item
		);
		setSettingsList(updatedList);
		if (activeModeIndex !== null && updatedList[activeModeIndex]) {
			publish(
				"traffic-light/active-setting",
				JSON.stringify(updatedList[activeModeIndex])
			);
		}
	};

	const handleTimeChange = (key, value) => {
		setTimeSettings((prev) => ({ ...prev, [key]: value }));
	};

	const handleSaveTime = () => {
		// 实时更新激活项并推送到mqtt
		const updatedList = settingsList.map((item, idx) =>
			idx === activeModeIndex
				? {
						...item,
						morningStart: timeSettings.morningStart,
						morningEnd: timeSettings.morningEnd,
						eveningStart: timeSettings.eveningStart,
						eveningEnd: timeSettings.eveningEnd,
				  }
				: item
		);
		setSettingsList(updatedList);
		if (activeModeIndex !== null && updatedList[activeModeIndex]) {
			publish(
				"traffic-light/active-setting",
				JSON.stringify(updatedList[activeModeIndex])
			);
		}
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

	function capitalize(str) {
		return str.charAt(0).toUpperCase() + str.slice(1);
	}

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
				isAdmin={isAdmin}
				status={status}
				setSettingsList={setSettingsList}
				setActiveModeIndex={setActiveModeIndex}
			/>
		</div>
	);
}

export default App;
