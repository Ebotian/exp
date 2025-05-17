import time
import threading
import json
import paho.mqtt.client as mqtt

# 统一 broker，订阅端和发布端都用同一个 broker
broker = "iot-06z00jbus4sj5ja.mqtt.iothub.aliyuncs.com"
port = 1883
topic = "/ilvjlSRSrTr/esp32cam_device/user/get"

# 订阅端参数
sub_client_id = "ilvjlSRSrTr.esp32cam_device|securemode=2,signmethod=hmacsha256,timestamp=1734522707236|"
sub_username = "esp32cam_device&ilvjlSRSrTr"
sub_password = "75157a1e6400a7eb4407654bf04d4ebfeeb2948baafec3f89df6276bd93ae91d"

def on_connect_sub(client, userdata, flags, rc):
    print("[SUB] Connected:", rc)
    client.subscribe(topic)

def on_message(client, userdata, msg):
    print("[SUB] 收到消息:", msg.topic, msg.payload.decode(errors='ignore'))

sub_client = mqtt.Client(client_id=sub_client_id, clean_session=True)
sub_client.username_pw_set(sub_username, sub_password)
sub_client.on_connect = on_connect_sub
sub_client.on_message = on_message

def sub_loop():
    sub_client.connect(broker, port, 60)
    sub_client.loop_forever()

# 发布端参数
pub_client_id = "ilvjlSRSrTr.esp32cam_app|securemode=2,signmethod=hmacsha256,timestamp=1747492118883|"
pub_username = "esp32cam_app&ilvjlSRSrTr"
pub_password = "5a2b003a5004978a7fe5904b036df770abddb25ff3459885f72fc492275d57ee"

def on_connect_pub(client, userdata, flags, rc):
    if rc == 0:
        print("[PUB] Connected successfully")
    else:
        print(f"[PUB] Connection failed with code {rc}")

pub_client = mqtt.Client(client_id=pub_client_id, clean_session=True)
pub_client.username_pw_set(pub_username, pub_password)
pub_client.on_connect = on_connect_pub

def pub_loop():
    pub_client.connect(broker, port, 60)
    pub_client.loop_start()
    try:
        while True:
            msg = json.dumps({"left": 1})
            print("[PUB] Send:", msg)
            rc, mid = pub_client.publish(topic, msg, qos=1)
            if rc == 0:
                print("[PUB] Publish success")
            else:
                print(f"[PUB] Publish failed, rc={rc}")
            time.sleep(5)
            msg = json.dumps({"right": 1})
            print("[PUB] Send:", msg)
            rc, mid = pub_client.publish(topic, msg, qos=1)
            if rc == 0:
                print("[PUB] Publish success")
            else:
                print(f"[PUB] Publish failed, rc={rc}")
            time.sleep(5)
    except KeyboardInterrupt:
        pub_client.loop_stop()
        pub_client.disconnect()

# 启动订阅线程
threading.Thread(target=sub_loop, daemon=True).start()
# 启动发布循环
pub_loop()