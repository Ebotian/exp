import React from "react";

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
}) => (
	<div className="dashboard-bg">
		<header className="dashboard-header">
			<span>交通灯控制系统 21010416王俊杰</span>
			<button className="logout-btn" onClick={onLogout}>
				退出登录
			</button>
		</header>
		<div className="dashboard-content">
			<div className="light-settings">
				<div className="direction-card">
					<h3>南北方向</h3>
					<label>红灯时长（秒）</label>
					<input
						type="number"
						value={northSouth.red}
						onChange={(e) => onSaveLight("northSouth", "red", e.target.value)}
					/>
					<button
						className="save-btn"
						onClick={() => onSaveLight("northSouth", "red")}
					>
						保存
					</button>
					<label>绿灯时长（秒）</label>
					<input
						type="number"
						value={northSouth.green}
						onChange={(e) => onSaveLight("northSouth", "green", e.target.value)}
					/>
					<button
						className="save-btn"
						onClick={() => onSaveLight("northSouth", "green")}
					>
						保存
					</button>
					<label>黄灯时长（秒）</label>
					<input
						type="number"
						value={northSouth.yellow}
						onChange={(e) =>
							onSaveLight("northSouth", "yellow", e.target.value)
						}
					/>
					<button
						className="save-btn"
						onClick={() => onSaveLight("northSouth", "yellow")}
					>
						保存
					</button>
				</div>
				<div className="direction-card">
					<h3>东西方向</h3>
					<label>红灯时长（秒）</label>
					<input
						type="number"
						value={eastWest.red}
						onChange={(e) => onSaveLight("eastWest", "red", e.target.value)}
					/>
					<button
						className="save-btn"
						onClick={() => onSaveLight("eastWest", "red")}
					>
						保存
					</button>
					<label>绿灯时长（秒）</label>
					<input
						type="number"
						value={eastWest.green}
						onChange={(e) => onSaveLight("eastWest", "green", e.target.value)}
					/>
					<button
						className="save-btn"
						onClick={() => onSaveLight("eastWest", "green")}
					>
						保存
					</button>
					<label>黄灯时长（秒）</label>
					<input
						type="number"
						value={eastWest.yellow}
						onChange={(e) => onSaveLight("eastWest", "yellow", e.target.value)}
					/>
					<button
						className="save-btn"
						onClick={() => onSaveLight("eastWest", "yellow")}
					>
						保存
					</button>
				</div>
			</div>
			<div className="time-settings">
				<h3>潮汐时间设置</h3>
				<label>早高峰开始时间</label>
				<input
					type="time"
					value={timeSettings.morningStart}
					onChange={(e) => onTimeChange("morningStart", e.target.value)}
				/>
				<label>早高峰结束时间</label>
				<input
					type="time"
					value={timeSettings.morningEnd}
					onChange={(e) => onTimeChange("morningEnd", e.target.value)}
				/>
				<label>晚高峰开始时间</label>
				<input
					type="time"
					value={timeSettings.eveningStart}
					onChange={(e) => onTimeChange("eveningStart", e.target.value)}
				/>
				<label>晚高峰结束时间</label>
				<input
					type="time"
					value={timeSettings.eveningEnd}
					onChange={(e) => onTimeChange("eveningEnd", e.target.value)}
				/>
				<button className="save-btn" onClick={onSaveTime}>
					保存潮汐时间
				</button>
			</div>
			<button className="add-setting-btn" onClick={onAddSetting}>
				添加当前设置到列表
			</button>
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
									<button className="edit-btn" onClick={() => onEdit(item.id)}>
										编辑
									</button>
									<button
										className="delete-btn"
										onClick={() => onDelete(item.id)}
									>
										删除
									</button>
								</td>
								<td>
									<button
										className="activate-btn"
										disabled={activeModeIndex === idx}
										onClick={() => onActivate(idx)}
									>
										{activeModeIndex === idx ? "已激活" : "激活"}
									</button>
								</td>
							</tr>
						))}
					</tbody>
				</table>
			</div>
		</div>
		{editModalOpen && (
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
							onChange={(e) => onEditChange("nsGreen", Number(e.target.value))}
						/>
						<label>南北黄灯时长（秒）</label>
						<input
							type="number"
							value={editItem.nsYellow}
							onChange={(e) => onEditChange("nsYellow", Number(e.target.value))}
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
							onChange={(e) => onEditChange("ewGreen", Number(e.target.value))}
						/>
						<label>东西黄灯时长（秒）</label>
						<input
							type="number"
							value={editItem.ewYellow}
							onChange={(e) => onEditChange("ewYellow", Number(e.target.value))}
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

export default Dashboard;
