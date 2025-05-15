import mqtt from "mqtt";

let client = null;

export function connectMQTT({ host, username, password, onMessage }) {
	console.log("MQTT connect host:", host); // 调试用
	client = mqtt.connect(host, {
		username,
		password,
		protocol: "ws",
	});

	client.on("connect", () => {
		client.subscribe("trafficlight/status");
	});

	client.on("message", (topic, message) => {
		if (topic === "trafficlight/status" && onMessage) {
			onMessage(JSON.parse(message.toString()));
		}
	});
}

export function publishControl(cmd) {
	if (client) {
		client.publish("trafficlight/control", JSON.stringify({ cmd }));
	}
}

// 新增通用 publish 方法
export function publish(topic, payload) {
	if (client) {
		client.publish(topic, payload);
	}
}

export function disconnectMQTT() {
	if (client) {
		client.end();
		client = null;
	}
}
