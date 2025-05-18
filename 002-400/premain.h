// premain.h
// ESP32 <-> BC260Y MQTT 通信头文件
#ifndef PREMAIN_H
#define PREMAIN_H

#include <Arduino.h>

// --- BC260Y 串口引脚定义 ---
#define BC260Y_RX_PIN 23 // ESP32 的 RX, 连接到 BC260Y 的 TX
#define BC260Y_TX_PIN 22 // ESP32 的 TX, 连接到 BC260Y 的 RX
extern HardwareSerial bcSerial;

// --- MQTT 参数 ---
extern const char* MQTT_BROKER_HOST;
extern const int MQTT_BROKER_PORT;
extern const char* MQTT_CLIENT_ID;
extern const char* MQTT_USERNAME;
extern const char* MQTT_PASSWORD;
extern const char* MQTT_TOPIC_CONTROL;
extern const char* MQTT_TOPIC_STATUS;

// --- 辅助变量 ---
extern unsigned long previousMillis;
extern const unsigned long atCommandInterval;
extern bool mqttNeedReconnect;

// --- MQTT/AT 通信相关函数声明 ---
bool initializeBC260Y();
bool mqttOpen();
bool mqttConnect();
bool mqttSubscribe();
bool mqttPublish(String topic, String payload);
void mqttReconnect();
bool sendATCommand(String command, String expectedResponse, unsigned long timeout, bool printDebug);
String sendATCommandGetResponse(String command, unsigned long timeout, bool printDebug);

#endif // PREMAIN_H
