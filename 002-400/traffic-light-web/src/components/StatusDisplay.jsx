import React from "react";

const StatusDisplay = ({ status }) => (
	<div className="status-display">
		<h3>当前交通灯状态</h3>
		<div className={`light light-${status}`}>
			{status ? status.toUpperCase() : "未知"}
		</div>
	</div>
);

export default StatusDisplay;
