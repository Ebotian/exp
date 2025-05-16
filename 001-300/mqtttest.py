import time
import threading
import json
import paho.mqtt.client as mqtt

broker = "a14IyRNlnNX.iot-as-mqtt.cn-shanghai.aliyuncs.com"
port = 1883
topic = "/ilvjlSRSrTr/esp32cam_device/user/get"

# 订阅端（用真实设备ID）
sub_client_id = "ilvjlSRSrTr.esp32cam_device|securemode=2,signmethod=hmacsha256,timestamp=1734522707236|"
username = "esp32cam_device&ilvjlSRSrTr"
password = "75157a1e6400a7eb4407654bf04d4ebfeeb2948baafec3f89df6276bd93ae91d"

def on_connect_sub(client, userdata, flags, rc):
    print("[SUB] Connected:", rc)
    client.subscribe(topic)

def on_message(client, userdata, msg):
    print("[SUB] 收到消息:", msg.topic, msg.payload.decode(errors='ignore'))

sub_client = mqtt.Client(client_id=sub_client_id, clean_session=True)
sub_client.username_pw_set(username, password)
sub_client.on_connect = on_connect_sub
sub_client.on_message = on_message

def sub_loop():
    sub_client.connect(broker, port, 60)
    sub_client.loop_forever()

# 发布端（用新ID）
pub_client_id = "test_publisher"
pub_client = mqtt.Client(client_id=pub_client_id, clean_session=True)
pub_client.username_pw_set(username, password)

def pub_loop():
    pub_client.connect(broker, port, 60)
    pub_client.loop_start()
    try:
        while True:
            msg = json.dumps({"left": 1})
            print("[PUB] Send:", msg)
            pub_client.publish(topic, msg)
            time.sleep(5)
            msg = json.dumps({"right": 1})
            print("[PUB] Send:", msg)
            pub_client.publish(topic, msg)
            time.sleep(5)
    except KeyboardInterrupt:
        pub_client.loop_stop()
        pub_client.disconnect()

# 启动订阅线程
threading.Thread(target=sub_loop, daemon=True).start()
# 启动发布循环
pub_loop()