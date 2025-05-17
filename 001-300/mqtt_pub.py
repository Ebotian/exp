import paho.mqtt.client as mqtt
import time
import json

# 发布端参数
broker = "iot-06z00jbus4sj5ja.mqtt.iothub.aliyuncs.com"
port = 1883
topic = "/ilvjlSRSrTr/esp32cam_device/user/get"

pub_client_id = "ilvjlSRSrTr.esp32cam_app|securemode=2,signmethod=hmacsha256,timestamp=1747494793807|"
pub_username = "esp32cam_app&ilvjlSRSrTr"
pub_password = "f146a9145e3a9deaa477318fc4f85bd07223f13f6219d50b3ae4b6343ebdc038"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[PUB] Connected successfully")
    else:
        print(f"[PUB] Connection failed with code {rc}")

client = mqtt.Client(client_id=pub_client_id, clean_session=True)
client.username_pw_set(pub_username, pub_password)
client.on_connect = on_connect

client.connect(broker, port, 60)
client.loop_start()

try:
    while True:
        msg = json.dumps({"left": 1})
        print("[PUB] Send:", msg)
        rc, mid = client.publish(topic, msg, qos=1)
        if rc == 0:
            print("[PUB] Publish success")
        else:
            print(f"[PUB] Publish failed, rc={rc}")
        time.sleep(5)
        msg = json.dumps({"right": 1})
        print("[PUB] Send:", msg)
        rc, mid = client.publish(topic, msg, qos=1)
        if rc == 0:
            print("[PUB] Publish success")
        else:
            print(f"[PUB] Publish failed, rc={rc}")
        time.sleep(5)
except KeyboardInterrupt:
    client.loop_stop()
    client.disconnect()
