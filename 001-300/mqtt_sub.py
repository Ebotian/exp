import paho.mqtt.client as mqtt
import time
import json

# 订阅端参数
broker = "iot-06z00jbus4sj5ja.mqtt.iothub.aliyuncs.com"
port = 1883
topic = "/ilvjlSRSrTr/esp32cam_device/user/get"

sub_client_id = "ilvjlSRSrTr.esp32cam_device|securemode=2,signmethod=hmacsha256,timestamp=1747494842511|"
sub_username = "esp32cam_device&ilvjlSRSrTr"
sub_password = "55209e87709feb53f6ca87bcae34f63a8d7310aa05710972be0930ff90e372eb"

def on_connect(client, userdata, flags, rc):
    print("[SUB] Connected:", rc)
    client.subscribe(topic)

def on_message(client, userdata, msg):
    print("[SUB] 收到消息:", msg.topic, msg.payload.decode(errors='ignore'))

client = mqtt.Client(client_id=sub_client_id, clean_session=True)
client.username_pw_set(sub_username, sub_password)
client.on_connect = on_connect
client.on_message = on_message

client.connect(broker, port, 60)
client.loop_forever()
