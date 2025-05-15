import { publish } from "../services/mqttService";
import { useState } from "react";

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

	// 保存按钮点击处理
	const handleSaveLight = (direction, color, value) => {
		try {
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
			publish(
				"traffic-light/active-setting",
				JSON.stringify(updatedList[activeModeIndex])
			);
			setSaveMsg("保存成功");
		} catch (e) {
			setSaveMsg("保存失败: " + e.message);
		} finally {
			setTimeout(() => setSaveMsg(""), 1500);
		}
	};

	// 保存潮汐时间按钮点击处理
	const handleSaveTime = () => {
		try {
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
			publish(
				"traffic-light/active-setting",
				JSON.stringify(updatedList[activeModeIndex])
			);
			setSaveMsg("潮汐时间保存成功");
		} catch (e) {
			setSaveMsg("潮汐时间保存失败: " + e.message);
		} finally {
			setTimeout(() => setSaveMsg(""), 1500);
		}
	};

	// 工具函数
	function capitalize(str) {
		return str.charAt(0).toUpperCase() + str.slice(1);
	}

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
				<div className="light-settings">
					<div className="direction-card">
						<h3>南北方向</h3>
						<label>红灯时长（秒）</label>
						<input
							type="number"
							value={northSouth.red}
							onChange={(e) =>
								isAdmin && handleSaveLight("northSouth", "red", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className="save-btn"
								onClick={() =>
									handleSaveLight("northSouth", "red", northSouth.red)
								}
							>
								保存
							</button>
						)}
						<label>绿灯时长（秒）</label>
						<input
							type="number"
							value={northSouth.green}
							onChange={(e) =>
								isAdmin &&
								handleSaveLight("northSouth", "green", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className="save-btn"
								onClick={() =>
									handleSaveLight("northSouth", "green", northSouth.green)
								}
							>
								保存
							</button>
						)}
						<label>黄灯时长（秒）</label>
						<input
							type="number"
							value={northSouth.yellow}
							onChange={(e) =>
								isAdmin &&
								handleSaveLight("northSouth", "yellow", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className="save-btn"
								onClick={() =>
									handleSaveLight("northSouth", "yellow", northSouth.yellow)
								}
							>
								保存
							</button>
						)}
					</div>
					<div className="direction-card">
						<h3>东西方向</h3>
						<label>红灯时长（秒）</label>
						<input
							type="number"
							value={eastWest.red}
							onChange={(e) =>
								isAdmin && handleSaveLight("eastWest", "red", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className="save-btn"
								onClick={() => handleSaveLight("eastWest", "red", eastWest.red)}
							>
								保存
							</button>
						)}
						<label>绿灯时长（秒）</label>
						<input
							type="number"
							value={eastWest.green}
							onChange={(e) =>
								isAdmin && handleSaveLight("eastWest", "green", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className="save-btn"
								onClick={() =>
									handleSaveLight("eastWest", "green", eastWest.green)
								}
							>
								保存
							</button>
						)}
						<label>黄灯时长（秒）</label>
						<input
							type="number"
							value={eastWest.yellow}
							onChange={(e) =>
								isAdmin && handleSaveLight("eastWest", "yellow", e.target.value)
							}
							readOnly={!isAdmin}
						/>
						{isAdmin && (
							<button
								className="save-btn"
								onClick={() =>
									handleSaveLight("eastWest", "yellow", eastWest.yellow)
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
						<button className="save-btn" onClick={handleSaveTime}>
							保存潮汐时间
						</button>
					)}
				</div>
				{isAdmin && (
					<button className="add-setting-btn" onClick={onAddSetting}>
						添加当前设置到列表
					</button>
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
							{settingsList.map((item, idx) => (
								<tr
									key={item.id}
									className={activeModeIndex === idx ? "active-mode-row" : ""}
								>
									<td>{`模式${idx + 1}`}</td>
									<td>{item.nsRed}</td>
									<td>{item.nsGreen}</td>
									<td>{item.nsYellow}</td>
									<td>{item.ewRed}</td>
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
													onClick={() => onEdit(item.id)}
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
												disabled={activeModeIndex === idx}
												onClick={() => onActivate(idx)}
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
							))}
						</tbody>
					</table>
				</div>
			</div>
			{editModalOpen && isAdmin && (
				<div className="modal-mask">
					<div className="modal">
						<div className="modal-header">
							<span>编辑设置</span>
							<button className="modal-close" onClick={onEditCancel}>
								×
							</button>
						</div>
						<div className="modal-body">
							<label>南北红灯时长（秒）</label>
							<input
								type="number"
								value={editItem.nsRed}
								onChange={(e) => onEditChange("nsRed", Number(e.target.value))}
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
							<label>早高峰开始时间</label>
							<input
								type="time"
								value={editItem.morningStart}
								onChange={(e) => onEditChange("morningStart", e.target.value)}
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
								onChange={(e) => onEditChange("eveningStart", e.target.value)}
							/>
							<label>晚高峰结束时间</label>
							<input
								type="time"
								value={editItem.eveningEnd}
								onChange={(e) => onEditChange("eveningEnd", e.target.value)}
							/>
						</div>
						<div className="modal-footer">
							<button className="save-btn" onClick={onEditSave}>
								保存修改
							</button>
							<button className="delete-btn" onClick={onEditCancel}>
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
