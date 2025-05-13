import mqtt from "mqtt";

let client = null;

export function connectMQTT({ host, username, password, onMessage }) {
	client = mqtt.connect(host, {
		username,
		password,
		protocol: "wss",
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

export function disconnectMQTT() {
	if (client) {
		client.end();
		client = null;
	}
}
