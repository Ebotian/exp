const express = require("express");
const fs = require("fs");
const path = require("path");
const app = express();
app.use(express.json());

const DATA_FILE = path.join(__dirname, "../users.json");

// 保存用户设置
app.post("/api/user/settings", (req, res) => {
	const { username, settings } = req.body;
	if (!username || !settings) return res.status(400).json({ success: false });
	let all = {};
	if (fs.existsSync(DATA_FILE)) {
		all = JSON.parse(fs.readFileSync(DATA_FILE, "utf-8"));
	}
	all[username] = settings;
	fs.writeFileSync(DATA_FILE, JSON.stringify(all, null, 2));
	res.json({ success: true });
});

// 获取用户设置
app.get("/api/user/settings", (req, res) => {
	const { username } = req.query;
	if (!username) return res.status(400).json({ success: false });
	let all = {};
	if (fs.existsSync(DATA_FILE)) {
		all = JSON.parse(fs.readFileSync(DATA_FILE, "utf-8"));
	}
	res.json({ success: true, data: all[username] || {} });
});

app.listen(5002, () => console.log("JSON API running on 5002"));
