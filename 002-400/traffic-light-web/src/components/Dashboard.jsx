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
					<button onClick={() => onSaveLight("northSouth", "red")}>保存</button>
					<label>绿灯时长（秒）</label>
					<input
						type="number"
						value={northSouth.green}
						onChange={(e) => onSaveLight("northSouth", "green", e.target.value)}
					/>
					<button onClick={() => onSaveLight("northSouth", "green")}>
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
					<button onClick={() => onSaveLight("northSouth", "yellow")}>
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
					<button onClick={() => onSaveLight("eastWest", "red")}>保存</button>
					<label>绿灯时长（秒）</label>
					<input
						type="number"
						value={eastWest.green}
						onChange={(e) => onSaveLight("eastWest", "green", e.target.value)}
					/>
					<button onClick={() => onSaveLight("eastWest", "green")}>保存</button>
					<label>黄灯时长（秒）</label>
					<input
						type="number"
						value={eastWest.yellow}
						onChange={(e) => onSaveLight("eastWest", "yellow", e.target.value)}
					/>
					<button onClick={() => onSaveLight("eastWest", "yellow")}>
						保存
					</button>
				</div>
			</div>
			<div className="time-settings">
				<h3>滞沙时间设置</h3>
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
				<button onClick={onSaveTime}>保存滞沙时间</button>
			</div>
			<div className="settings-list">
				<h3>设置列表</h3>
				<table>
					<thead>
						<tr>
							<th>ID</th>
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
						</tr>
					</thead>
					<tbody>
						{settingsList.map((item, idx) => (
							<tr key={item.id}>
								<td>{item.id}</td>
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
									<button onClick={() => onEdit(item.id)}>编辑</button>
									<button onClick={() => onDelete(item.id)}>删除</button>
								</td>
							</tr>
						))}
					</tbody>
				</table>
			</div>
		</div>
	</div>
);

export default Dashboard;
