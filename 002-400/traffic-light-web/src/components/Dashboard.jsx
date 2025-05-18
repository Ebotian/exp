import { publish, publishControl } from "../services/mqttService";
import { useState, useEffect, useRef } from "react";

const RED_TRANSITION_TIME = 2; // 与main.ino保持一致
const DEFAULT_PARAMS_INITIAL = {
	nsGreen: 5,
	nsYellow: 5,
	ewGreen: 5,
	ewYellow: 5,
	// 其余字段可补充默认值或留空
};

const Dashboard = ({
	user,
	onLogout,
	northSouth,
	eastWest,
	onSaveLight,
	timeSettings,
	onTimeChange,
	onSaveTime,
	settingsList,
	onEdit,
	onDelete,
	onAddSetting,
	editModalOpen,
	editItem,
	onEditChange,
	onEditSave,
	onEditCancel,
	onActivate, // 新增：激活模式的回调
	activeModeIndex, // 新增：当前激活模式的下标
	isAdmin, // 新增：是否为管理员
	status, // 新增：交通灯状态
	setSettingsList, // 新增：用于更新设置列表
	setActiveModeIndex, // 新增：用于更新激活模式
}) => {
	const [saveMsg, setSaveMsg] = useState("");
	const lastSubmitRef = useRef({}); // 记录上次提交的时间点，避免重复提交
	const [defaultParams, setDefaultParams] = useState(DEFAULT_PARAMS_INITIAL);
	const [isEditingDefault, setIsEditingDefault] = useState(false); // 新增：是否在编辑默认参数

	// 新增：处理默认参数修改
	const handleDefaultParamChange = (direction, color, value) => {
		const numValue = Number(value);
		if (isNaN(numValue) || numValue < 0) {
			setSaveMsg("请输入有效的正数");
			return;
		}

		const key = direction === "northSouth" ? `ns${capitalize(color)}` : `ew${capitalize(color)}`;
		setDefaultParams(prev => ({
			...prev,
			[key]: numValue
		}));
	};

	// 计算红灯时长（只读展示）
	const nsRed = eastWest.green + eastWest.yellow + RED_TRANSITION_TIME;
	const ewRed = northSouth.green + northSouth.yellow + RED_TRANSITION_TIME;

	// 保存按钮点击处理
	const handleSaveLight = (direction, color, value) => {
		try {
			const numValue = Number(value);
			if (isNaN(numValue) || numValue < 0) {
				setSaveMsg("请输入有效的正数");
				return;
			}

			if (isEditingDefault) {
				// 更新默认参数
				const key = direction === "northSouth" ? `ns${capitalize(color)}` : `ew${capitalize(color)}`;
				const newParams = {
					...defaultParams,
					[key]: numValue
				};
				setDefaultParams(newParams);

				// 如果当前不在潮汐时间段，立即应用新的默认参数
				const cmd = {
					type: "set_mode",
					modeIndex: 0, // 默认模式为0
					nsGreen: newParams.nsGreen,
					nsYellow: newParams.nsYellow,
					ewGreen: newParams.ewGreen,
					ewYellow: newParams.ewYellow,
				};
				publishControl(cmd);
				setSaveMsg("默认参数已更新并生效");
			} else {
				onSaveLight(direction, color, value);
				// 实时更新激活项
				const updatedList = settingsList.map((item, idx) =>
					idx === activeModeIndex
						? {
								...item,
								...(direction === "northSouth"
									? { [`ns${capitalize(color)}`]: value }
									: { [`ew${capitalize(color)}`]: value }),
						}
						: item
				);
				setSettingsList(updatedList);
			// 推送到mqtt
				const setting = updatedList[activeModeIndex];
				const cmd = {
					type: "set_mode",
					modeIndex: activeModeIndex,
					nsGreen: setting.nsGreen,
					nsYellow: setting.nsYellow,
					ewGreen: setting.ewGreen,
					ewYellow: setting.ewYellow,
				};
				publishControl(cmd);
				setSaveMsg("保存成功");
			}
		} catch (e) {
			setSaveMsg("保存失败: " + e.message);
		} finally {
			setTimeout(() => setSaveMsg(""), 1500);
		}
	};

	// 工具函数
	function capitalize(str) {
		return str.charAt(0).toUpperCase() + str.slice(1);
	}

	// 检查时间并发送相应配置
	const checkTimeAndSendConfig = async () => {
		const { hh, mm } = await getBeijingTime();
		const hhmm = `${hh}:${mm}`;
		const { morningStart, morningEnd, eveningStart, eveningEnd } = timeSettings;

		function pad2(n) {
			return n.toString().padStart(2, "0");
		}
		function norm(t) {
			if (!t) return "";
			const [h, m] = t.split(":");
			return pad2(h) + ":" + pad2(m);
		}
		const ms = norm(morningStart);
		const me = norm(morningEnd);
		const es = norm(eveningStart);
		const ee = norm(eveningEnd);

		// 判断当前是否在潮汐时间段内
		function inPeriod(start, end, now) {
			if (!start || !end) return false;
			function toMinutes(t) {
				const [h, m] = t.split(":");
				return parseInt(h, 10) * 60 + parseInt(m, 10);
			}
			const s = toMinutes(start);
			const e = toMinutes(end);
			const n = toMinutes(now);
			if (e > s) {
				return s <= n && n < e;
			} else if (e < s) {
				// 跨午夜的时间段
				return n >= s || n < e;
			} else {
				// start==end，视为无效区间
				return false;
			}
		}
		const inMorning = inPeriod(ms, me, hhmm);
		const inEvening = inPeriod(es, ee, hhmm);

		if (inMorning || inEvening) {
			if (settingsList[activeModeIndex]) {
				const tidalSetting = settingsList[activeModeIndex];
				const cmd = {
					type: "set_mode",
					modeIndex: activeModeIndex,
					nsGreen: tidalSetting.nsGreen,
					nsYellow: tidalSetting.nsYellow,
					ewGreen: tidalSetting.ewGreen,
					ewYellow: tidalSetting.ewYellow,
				};
				console.log("MQTT publish: 潮汐激活配置", hhmm, cmd);
				publishControl(cmd);
			}
		} else {
			// 非潮汐时间段，发送与"已激活"按钮完全一致的默认参数格式
			const cmd = {
				type: "set_mode",
				modeIndex: 0, // 默认模式为0
				nsGreen: defaultParams.nsGreen,
				nsYellow: defaultParams.nsYellow,
				ewGreen: defaultParams.ewGreen,
				ewYellow: defaultParams.ewYellow,
			};
			console.log("MQTT publish: 默认参数", hhmm, cmd);
			publishControl(cmd);
		}
	};

	// 从网络获取北京时间
	const getBeijingTime = async () => {
		// 定义多个时间服务API
		const timeApis = [
			"https://worldtimeapi.org/api/timezone/Asia/Shanghai",
			"https://time.is/zh/Unix_time_now", // 备用服务器1
			"http://quan.suning.com/getSysTime.do", // 备用服务器2
		];

		for (const api of timeApis) {
			try {
				const response = await fetch(api);
				const data = await response.json();
				let beijingDate;

				if (api.includes("worldtimeapi")) {
					beijingDate = new Date(data.datetime);
				} else if (api.includes("time.is")) {
					beijingDate = new Date(data.timestamp * 1000);
				} else if (api.includes("suning")) {
					beijingDate = new Date(data.sysTime2.replace(/-/g, "/"));
				}

				return {
					hh: beijingDate.getHours().toString().padStart(2, "0"),
					mm: beijingDate.getMinutes().toString().padStart(2, "0"),
				};
			} catch (error) {
				console.error(`获取时间失败(${api}):`, error);
				// 继续尝试下一个API
				continue;
			}
		}

		// 所有API都失败时，使用本地时间作为后备
		console.warn("所有网络时间API均失败，使用本地时间作为后备");
		const now = new Date();
		return {
			hh: now.toLocaleString("zh-CN", {
				hour: "2-digit",
				hour12: false,
				timeZone: "Asia/Shanghai",
			}),
			mm: now.toLocaleString("zh-CN", {
				minute: "2-digit",
				hour12: false,
				timeZone: "Asia/Shanghai",
			}),
		};
	};

	// 修改保存潮汐时间处理函数
	const handleSaveTime = () => {
		try {
			if (isEditingDefault) {
				// 在默认参数模式下，保存并立即应用当前表单的值
				const cmd = {
					type: "set_mode",
					modeIndex: 0, // 默认模式为0
					nsGreen: defaultParams.nsGreen,
					nsYellow: defaultParams.nsYellow,
					ewGreen: defaultParams.ewGreen,
					ewYellow: defaultParams.ewYellow,
				};
				publishControl(cmd);
				setSaveMsg("默认参数已更新并生效");
			} else {
				// 原有的潮汐时间保存逻辑
				onSaveTime();
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
				setSaveMsg("潮汐时间保存成功");
				// 保存后立即检查并发送配置
				checkTimeAndSendConfig();
			}
		} catch (e) {
			setSaveMsg(isEditingDefault ? "默认参数更新失败: " : "潮汐时间保存失败: " + e.message);
		} finally {
			setTimeout(() => setSaveMsg(""), 1500);
		}
	};

	const handleTimeSettingsChange = (newSettings) => {
		setTimeSettings(newSettings);
		checkTimeAndSendConfig(); // 保存设置后立即检查时间并发送配置
	};

	// 新增：打开编辑默认参数的模态框
	const handleEditDefault = () => {
		setIsEditingDefault(true);
		setEditItem({
			nsGreen: defaultParams.nsGreen,
			nsYellow: defaultParams.nsYellow,
			ewGreen: defaultParams.ewGreen,
			ewYellow: defaultParams.ewYellow,
			nsRed:
				defaultParams.ewGreen + defaultParams.ewYellow + RED_TRANSITION_TIME,
			ewRed:
				defaultParams.nsGreen + defaultParams.nsYellow + RED_TRANSITION_TIME,
		});
		setEditModalOpen(true);
	};

	// 修改原有的编辑函数
	const handleEdit = (id) => {
		setIsEditingDefault(false);
		onEdit(id);
	};

	// 修改保存函数
	const handleEditSave = () => {
		if (isEditingDefault) {
			// 保存默认参数
			const newDefaultParams = {
				nsGreen: editItem.nsGreen,
				nsYellow: editItem.nsYellow,
				ewGreen: editItem.ewGreen,
				ewYellow: editItem.ewYellow,
			};
			setDefaultParams(newDefaultParams);
			setEditModalOpen(false);
			setSaveMsg("默认参数已更新");
			setTimeout(() => setSaveMsg(""), 1500);

			// 如果当前不在潮汐时间段，立即应用新的默认参数
			checkTimeAndSendConfig();
		} else {
			// 原有的保存逻辑
			onEditSave();
		}
	};

	// 修改取消函数
	const handleEditCancel = () => {
		setIsEditingDefault(false);
		onEditCancel();
	};

	useEffect(() => {
		const timer = setInterval(checkTimeAndSendConfig, 10000); // 每10秒检测并发送一次
		return () => clearInterval(timer);
	}, [settingsList, activeModeIndex, timeSettings]);

	// 允许"已激活"按钮反复点击
	const handleActivateClick = (idx) => {
		onActivate(idx);
		checkTimeAndSendConfig(); // 激活后立即检查时间并发送配置
	};

	return (
		<div className="dashboard-bg">
			<header className="dashboard-header">
				<span>交通灯控制系统 21010416王俊杰</span>
				<button className="logout-btn" onClick={onLogout}>
					退出登录
				</button>
			</header>
			{saveMsg && <div className="save-msg-tip">{saveMsg}</div>}
			<div className="dashboard-content">
				<div className="traffic-status">
					<div>
						南北向：
						<span style={{ color: status.nsColor }}>{status.nsColor}</span>{" "}
						{status.nsSeconds} 秒
					</div>
					<div>
						东西向：
						<span style={{ color: status.ewColor }}>{status.ewColor}</span>{" "}
						{status.ewSeconds} 秒
					</div>
				</div>
				{isAdmin && (
					<>
						<div className="mode-switch-container">
							<span>当前编辑：</span>
							<div className="mode-switch">
								<button
									className={!isEditingDefault ? "active" : ""}
									onClick={() => {
										if (isEditingDefault) {
											setIsEditingDefault(false);
											// 清除编辑状态
											handleEditCancel();
										}
									}}
								>
									潮汐模式
								</button>
								<button
									className={isEditingDefault ? "active" : ""}
									onClick={() => {
										if (!isEditingDefault) {
											handleEditDefault();
										}
									}}
								>
									默认参数
								</button>
							</div>
						</div>
					</>
				)}
				<div className="light-settings">
					<div className="direction-card">
						<h3>南北方向</h3>
						<label>红灯时长（秒）</label>
						<input type="number" value={nsRed} readOnly />
						<label>绿灯时长（秒）</label>
						<input
							type="number"
							value={
								isEditingDefault ? defaultParams.nsGreen : northSouth.green
							}
							onChange={(e) =>
								isAdmin &&
								handleSaveLight("northSouth", "green", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className={`save-btn ${
									isEditingDefault ? "save-default-btn" : ""
								}`}
								onClick={() =>
									handleSaveLight("northSouth", "green", isEditingDefault ? defaultParams.nsGreen : northSouth.green)
								}
							>
								保存
							</button>
						)}
						<label>黄灯时长（秒）</label>
						<input
							type="number"
							value={
								isEditingDefault ? defaultParams.nsYellow : northSouth.yellow
							}
							onChange={(e) =>
								isAdmin &&
								handleSaveLight("northSouth", "yellow", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className={`save-btn ${
									isEditingDefault ? "save-default-btn" : ""
								}`}
								onClick={() =>
									handleSaveLight("northSouth", "yellow", isEditingDefault ? defaultParams.nsYellow : northSouth.yellow)
								}
							>
								保存
							</button>
						)}
					</div>
					<div className="direction-card">
						<h3>东西方向</h3>
						<label>红灯时长（秒）</label>
						<input type="number" value={ewRed} readOnly />
						<label>绿灯时长（秒）</label>
						<input
							type="number"
							value={isEditingDefault ? defaultParams.ewGreen : eastWest.green}
							onChange={(e) =>
								isAdmin && handleSaveLight("eastWest", "green", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className={`save-btn ${
									isEditingDefault ? "save-default-btn" : ""
								}`}
								onClick={() =>
									handleSaveLight("eastWest", "green", isEditingDefault ? defaultParams.ewGreen : eastWest.green)
								}
							>
								保存
							</button>
						)}
						<label>黄灯时长（秒）</label>
						<input
							type="number"
							value={
								isEditingDefault ? defaultParams.ewYellow : eastWest.yellow
							}
							onChange={(e) =>
								isAdmin && handleSaveLight("eastWest", "yellow", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className={`save-btn ${
									isEditingDefault ? "save-default-btn" : ""
								}`}
								onClick={() =>
									handleSaveLight("eastWest", "yellow", isEditingDefault ? defaultParams.ewYellow : eastWest.yellow)
								}
							>
								保存
							</button>
						)}
					</div>
				</div>
				<div className="time-settings">
					<h3>潮汐时间设置</h3>
					<label>早高峰开始时间</label>
					<input
						type="time"
						value={timeSettings.morningStart}
						onChange={(e) =>
							isAdmin && onTimeChange("morningStart", e.target.value)
						}
						readOnly={!isAdmin}
					/>
					<label>早高峰结束时间</label>
					<input
						type="time"
						value={timeSettings.morningEnd}
						onChange={(e) =>
							isAdmin && onTimeChange("morningEnd", e.target.value)
						}
						readOnly={!isAdmin}
					/>
					<label>晚高峰开始时间</label>
					<input
						type="time"
						value={timeSettings.eveningStart}
						onChange={(e) =>
							isAdmin && onTimeChange("eveningStart", e.target.value)
						}
						readOnly={!isAdmin}
					/>
					<label>晚高峰结束时间</label>
					<input
						type="time"
						value={timeSettings.eveningEnd}
						onChange={(e) =>
							isAdmin && onTimeChange("eveningEnd", e.target.value)
						}
						readOnly={!isAdmin}
					/>
					{isAdmin && (
						<button
							className={`save-btn ${
								isEditingDefault ? "save-default-btn" : ""
							}`}
							onClick={handleSaveTime}
						>
							{isEditingDefault ? "保存默认参数时间" : "保存潮汐时间"}
						</button>
					)}
				</div>
				<button className="add-setting-btn" onClick={onAddSetting}>
					添加当前设置到列表
				</button>
				{isEditingDefault && (
					<div className="default-params-display">
						<h3>当前默认参数</h3>
						<table>
							<thead>
								<tr>
									<th>参数类型</th>
									<th>南北方向</th>
									<th>东西方向</th>
								</tr>
							</thead>
							<tbody>
								<tr>
									<td>红灯时长（秒）</td>
									<td>
										{defaultParams.ewGreen +
											defaultParams.ewYellow +
											RED_TRANSITION_TIME}
									</td>
									<td>
										{defaultParams.nsGreen +
											defaultParams.nsYellow +
											RED_TRANSITION_TIME}
									</td>
								</tr>
								<tr>
									<td>绿灯时长（秒）</td>
									<td>{defaultParams.nsGreen}</td>
									<td>{defaultParams.ewGreen}</td>
								</tr>
								<tr>
									<td>黄灯时长（秒）</td>
									<td>{defaultParams.nsYellow}</td>
									<td>{defaultParams.ewYellow}</td>
								</tr>
							</tbody>
						</table>
					</div>
				)}
				<div className="settings-list">
					<h3>设置列表</h3>
					<table>
						<thead>
							<tr>
								<th>模式名称</th>
								<th>南北红灯</th>
								<th>南北绿灯</th>
								<th>南北黄灯</th>
								<th>东西红灯</th>
								<th>东西绿灯</th>
								<th>东西黄灯</th>
								<th>早高峰开始</th>
								<th>早高峰结束</th>
								<th>晚高峰开始</th>
								<th>晚高峰结束</th>
								<th>操作</th>
								<th>激活</th>
							</tr>
						</thead>
						<tbody>
							{settingsList.map((item, idx) => {
								const nsRedRow =
									item.ewGreen + item.ewYellow + RED_TRANSITION_TIME;
								const ewRedRow =
									item.nsGreen + item.nsYellow + RED_TRANSITION_TIME;
								return (
									<tr
										key={item.id}
										className={activeModeIndex === idx ? "active-mode-row" : ""}
									>
										<td>{`模式${idx + 1}`}</td>
										<td>{nsRedRow}</td>
										<td>{item.nsGreen}</td>
										<td>{item.nsYellow}</td>
										<td>{ewRedRow}</td>
										<td>{item.ewGreen}</td>
										<td>{item.ewYellow}</td>
										<td>{item.morningStart}</td>
										<td>{item.morningEnd}</td>
										<td>{item.eveningStart}</td>
										<td>{item.eveningEnd}</td>
										<td>
											{isAdmin && (
												<>
													<button
														className="edit-btn"
														onClick={() => handleEdit(item.id)}
													>
														编辑
													</button>
													<button
														className="delete-btn"
														onClick={() => onDelete(item.id)}
													>
														删除
													</button>
												</>
											)}
										</td>
										<td>
											{isAdmin ? (
												<button
													disabled={false}
													onClick={() => handleActivateClick(idx)}
													className="activate-btn"
												>
													{activeModeIndex === idx ? "已激活" : "激活"}
												</button>
											) : activeModeIndex === idx ? (
												"已激活"
											) : (
												"-"
											)}
										</td>
									</tr>
								);
							})}
						</tbody>
					</table>
				</div>
			</div>
			{editModalOpen && isAdmin && (
				<div className="modal-mask">
					<div className="modal">
						<div className="modal-header">
							<span>{isEditingDefault ? "编辑默认参数" : "编辑设置"}</span>
							<button className="modal-close" onClick={handleEditCancel}>
								×
							</button>
						</div>
						<div className="modal-body">
							<label>南北红灯时长（秒）</label>
							<input
								type="number"
								value={editItem.nsRed}
								onChange={(e) => onEditChange("nsRed", Number(e.target.value))}
								readOnly
							/>
							<label>南北绿灯时长（秒）</label>
							<input
								type="number"
								value={editItem.nsGreen}
								onChange={(e) =>
									onEditChange("nsGreen", Number(e.target.value))
								}
							/>
							<label>南北黄灯时长（秒）</label>
							<input
								type="number"
								value={editItem.nsYellow}
								onChange={(e) =>
									onEditChange("nsYellow", Number(e.target.value))
								}
							/>
							<label>东西红灯时长（秒）</label>
							<input
								type="number"
								value={editItem.ewRed}
								onChange={(e) => onEditChange("ewRed", Number(e.target.value))}
								readOnly
							/>
							<label>东西绿灯时长（秒）</label>
							<input
								type="number"
								value={editItem.ewGreen}
								onChange={(e) =>
									onEditChange("ewGreen", Number(e.target.value))
								}
							/>
							<label>东西黄灯时长（秒）</label>
							<input
								type="number"
								value={editItem.ewYellow}
								onChange={(e) =>
									onEditChange("ewYellow", Number(e.target.value))
								}
							/>
							{!isEditingDefault && (
								<>
									<label>早高峰开始时间</label>
									<input
										type="time"
										value={editItem.morningStart}
										onChange={(e) =>
											onEditChange("morningStart", e.target.value)
										}
									/>
									<label>早高峰结束时间</label>
									<input
										type="time"
										value={editItem.morningEnd}
										onChange={(e) => onEditChange("morningEnd", e.target.value)}
									/>
									<label>晚高峰开始时间</label>
									<input
										type="time"
										value={editItem.eveningStart}
										onChange={(e) =>
											onEditChange("eveningStart", e.target.value)
										}
									/>
									<label>晚高峰结束时间</label>
									<input
										type="time"
										value={editItem.eveningEnd}
										onChange={(e) => onEditChange("eveningEnd", e.target.value)}
									/>
								</>
							)}
						</div>
						<div className="modal-footer">
							<button
								className={`save-btn ${
									isEditingDefault ? "save-default-btn" : ""
								}`}
								onClick={handleEditSave}
							>
								{isEditingDefault ? "保存默认参数" : "保存修改"}
							</button>
							<button className="delete-btn" onClick={handleEditCancel}>
								取消
							</button>
						</div>
					</div>
				</div>
			)}
		</div>
	);
};

export default Dashboard;
