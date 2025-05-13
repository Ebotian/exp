import React from "react";

const TrafficLightControl = ({ onControl }) => (
	<div className="traffic-light-control">
		<h3>交通灯控制</h3>
		<button onClick={() => onControl("red")}>红灯</button>
		<button onClick={() => onControl("yellow")}>黄灯</button>
		<button onClick={() => onControl("green")}>绿灯</button>
	</div>
);

export default TrafficLightControl;
