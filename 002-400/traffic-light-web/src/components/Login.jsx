import React, { useState } from "react";
import users from "../../users.json";

const Login = ({ onLogin }) => {
	const [username, setUsername] = useState("");
	const [password, setPassword] = useState("");

	const handleSubmit = (e) => {
		e.preventDefault();
		const found = users.find(
			(u) => u.username === username && u.password === password
		);
		if (found) {
			onLogin(username, password);
		} else {
			alert("用户名或密码错误");
		}
	};

	return (
		<div className="login-bg">
			<div className="login-card">
				<h2>欢迎登录</h2>
				<form onSubmit={handleSubmit}>
					<div>
						<input
							type="text"
							placeholder="请输入用户名"
							value={username}
							onChange={(e) => setUsername(e.target.value)}
							required
						/>
					</div>
					<div>
						<input
							type="password"
							placeholder="请输入密码"
							value={password}
							onChange={(e) => setPassword(e.target.value)}
							required
						/>
					</div>
					<button type="submit" className="login-btn">
						登录
					</button>
				</form>
			</div>
		</div>
	);
};

export default Login;
