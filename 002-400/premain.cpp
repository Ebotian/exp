// premain.cpp
// ESP32 <-> BC260Y MQTT 通信实现文件
#include "premain.h"

HardwareSerial bcSerial(2); // 使用 UART2

const char* MQTT_BROKER_HOST = "39.107.106.220";
const int MQTT_BROKER_PORT = 1883;
const char* MQTT_CLIENT_ID = "esp32-traffic-light-device-01";
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";
const char* MQTT_TOPIC_CONTROL = "trafficlight/control";
const char* MQTT_TOPIC_STATUS = "trafficlight/status";

unsigned long previousMillis = 0;
const unsigned long atCommandInterval = 10000;
bool mqttNeedReconnect = false;

// --- MQTT/AT 通信相关函数实现 ---

void mqttReconnect() {
    Serial.println("[重建] 检测到 MQTT 断开，开始自动重建...");
    sendATCommand("AT+QMTCLOSE=0\r\n", "OK", 3000, false);
    delay(1000);
    if (!mqttOpen()) {
        Serial.println("[重建] QMTOPEN 失败，等待 5 秒后重试...");
        delay(5000);
        return;
    }
    delay(1000);
    if (!mqttConnect()) {
        Serial.println("[重建] QMTCONN 失败，等待 5 秒后重试...");
        delay(5000);
        return;
    }
    delay(1000);
    if (!mqttSubscribe()) {
        Serial.println("[重建] QMTSUB 失败，等待 5 秒后重试...");
        delay(5000);
        return;
    }
    Serial.println("[重建] MQTT 重建完成，恢复正常工作。");
    mqttNeedReconnect = false;
    mqttPublish(MQTT_TOPIC_STATUS, "{\"status\":\"online\",\"esp32\":\"reconnected\"}");
}

bool sendATCommand(String command, String expectedResponse, unsigned long timeout, bool printDebug) {
    String response = "";
    if (printDebug) {
        Serial.print("发送: ");
        Serial.println(command);
    }
    bcSerial.print(command);
    unsigned long startTime = millis();
    while (millis() - startTime < timeout) {
        if (bcSerial.available()) {
            char c = bcSerial.read();
            response += c;
        }
        if (response.indexOf(expectedResponse) != -1) {
            if (printDebug) {
                Serial.print("收到: ");
                Serial.println(response);
            }
            return true;
        }
    }
    if (printDebug) {
        Serial.print("超时. 收到: ");
        Serial.println(response);
    }
    return false;
}

String sendATCommandGetResponse(String command, unsigned long timeout, bool printDebug) {
    String response = "";
    if (printDebug) {
        Serial.print("发送: ");
        Serial.println(command);
    }
    bcSerial.print(command);
    unsigned long startTime = millis();
    while (millis() - startTime < timeout) {
        if (bcSerial.available()) {
            char c = bcSerial.read();
            response += c;
        }
    }
    if (printDebug) {
        Serial.print("收到: ");
        Serial.println(response);
    }
    return response;
}

bool initializeBC260Y() {
    const int maxRetries = 3;
    for(int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("初始化 BC260Y... (尝试 %d/%d)\n", attempt, maxRetries);
        bool success = true;

        // 基本 AT 命令检查
        if (!sendATCommand("AT\r\n", "OK", 2000, true)) {
            success = false;
        }
        // 关闭回显
        else if (!sendATCommand("ATE0\r\n", "OK", 2000, true)) {
            success = false;
        }
        // 设置全功能模式
        else if (!sendATCommand("AT+CFUN=1\r\n", "OK", 5000, true)) {
            success = false;
        }
        delay(1000);
        // 检查 SIM 卡
        if (success && !sendATCommand("AT+CPIN?\r\n", "+CPIN: READY", 5000, true)) {
            success = false;
        }

        if (success) {
            Serial.println("检查网络注册状态...");
            int networkRetries = 10;
            bool registered = false;
            while(networkRetries-- > 0) {
                String resp = sendATCommandGetResponse("AT+CEREG?\r\n", 3000, true);
                if (resp.indexOf("+CEREG: 0,1") != -1 || resp.indexOf("+CEREG: 0,5") != -1 ||
                    resp.indexOf("+CEREG: 1,1") != -1 || resp.indexOf("+CEREG: 1,5") != -1 ||
                    resp.indexOf("+CEREG: 2,1") != -1 || resp.indexOf("+CEREG: 2,5") != -1 ) {
                    Serial.println("网络已注册。");
                    registered = true;
                    break;
                }
                Serial.println("尚未注册，正在重试...");
                delay(3000);
            }
            if (registered) {
                Serial.println("BC260Y 初始化成功！");
                return true;
            }
        }

        if (attempt < maxRetries) {
            Serial.printf("初始化失败，%d 秒后重试...\n", attempt);
            delay(attempt * 1000);  // 递增延迟
        }
    }
    Serial.println("BC260Y 初始化失败，已达最大重试次数。");
    return false;
}

bool mqttOpen() {
    const int maxRetries = 3;
    for(int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("打开 MQTT 客户端实例... (尝试 %d/%d)\n", attempt, maxRetries);
        String cmd = "AT+QMTOPEN=0,\"" + String(MQTT_BROKER_HOST) + "\"," + String(MQTT_BROKER_PORT) + "\r\n";
        String response = sendATCommandGetResponse(cmd, 10000, true);
        if (response.indexOf("+QMTOPEN: 0,0") != -1 || response.indexOf("+QMTOPEN: 0,2") != -1) {
            Serial.println("MQTT 客户端实例打开成功 (或已打开)。");
            return true;
        }
        if (attempt < maxRetries) {
            Serial.printf("打开失败，%d 秒后重试...\n", attempt);
            sendATCommand("AT+QMTCLOSE=0\r\n", "OK", 3000, true);  // 先关闭再重试
            delay(attempt * 1000);
        }
    }
    Serial.println("打开 MQTT 客户端实例失败，已达最大重试次数。");
    return false;
}

bool mqttConnect() {
    const int maxRetries = 3;
    for(int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("连接到 MQTT 代理... (尝试 %d/%d)\n", attempt, maxRetries);
        String cmd = "AT+QMTCONN=0,\"" + String(MQTT_CLIENT_ID) + "\"";
        if (strlen(MQTT_USERNAME) > 0) {
            cmd += ",\"" + String(MQTT_USERNAME) + "\"";
            if (strlen(MQTT_PASSWORD) > 0) {
                cmd += ",\"" + String(MQTT_PASSWORD) + "\"";
            }
        }
        cmd += "\r\n";
        String response = sendATCommandGetResponse(cmd, 15000, true);
        if (response.indexOf("+QMTCONN: 0,0,0") != -1) {
            Serial.println("成功连接到 MQTT 代理！");
            return true;
        }
        if (attempt < maxRetries) {
            Serial.printf("连接失败，%d 秒后重试...\n", attempt);
            delay(attempt * 1000);
        }
    }
    Serial.println("连接到 MQTT 代理失败，已达最大重试次数。");
    return false;
}

bool mqttSubscribe() {
    const int maxRetries = 3;
    for(int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("订阅控制主题... (尝试 %d/%d)\n", attempt, maxRetries);
        String cmd = "AT+QMTSUB=0,1,\"" + String(MQTT_TOPIC_CONTROL) + "\",1\r\n";
        String response = sendATCommandGetResponse(cmd, 5000, true);
        if (response.indexOf("+QMTSUB: 0,1,0") != -1) {
            Serial.println("成功订阅主题 " + String(MQTT_TOPIC_CONTROL));
            return true;
        }
        if (attempt < maxRetries) {
            Serial.printf("订阅失败，%d 秒后重试...\n", attempt);
            delay(attempt * 1000);
        }
    }
    Serial.println("订阅主题失败，已达最大重试次数。");
    return false;
}

bool mqttPublish(String topic, String payload) {
    const int maxRetries = 3;
    int attempt = 0;
    while (attempt < maxRetries) {
        Serial.println("发布消息...");
        String cmd_header = "AT+QMTPUB=0,1,1,0,\"" + topic + "\"\r\n";
        Serial.print("发送 PUBLISH 头: ");
        Serial.print(cmd_header);
        bcSerial.print(cmd_header);
        unsigned long startTime = millis();
        String response_prompt = "";
        bool prompt_received = false;
        while (millis() - startTime < 2000) {
            if (bcSerial.available()) {
                char c = bcSerial.read();
                response_prompt += c;
                if (response_prompt.endsWith(">")) {
                    prompt_received = true;
                    Serial.println("收到 '>' 提示符.");
                    break;
                }
            }
        }
        if (!prompt_received) {
            Serial.println("[错误] 未收到 '>' 提示符。响应: " + response_prompt);
            mqttNeedReconnect = true;
            attempt++;
            Serial.printf("[重试] QMTPUB prompt 未收到, 第 %d 次重试\n", attempt);
            delay(500);
            continue;
        }
        Serial.print("发送 Payload: ");
        Serial.println(payload);
        bcSerial.print(payload);
        delay(10);
        Serial.println("发送 CTRL+Z");
        bcSerial.write(26);
        String final_response = "";
        startTime = millis();
        unsigned long publishTimeout = 7000;
        bool success = false;
        while (millis() - startTime < publishTimeout) {
            if (bcSerial.available()) {
                char c = bcSerial.read();
                final_response += c;
                if (final_response.indexOf("+QMTPUB: 0,1,0") != -1 || final_response.indexOf("\nOK\r") != -1) {
                    Serial.print("发布结果响应: ");
                    Serial.println(final_response);
                    Serial.println("消息成功发布到主题 " + topic);
                    return true;
                }
                if (final_response.indexOf("+QMTPUB: 0,1,2") != -1) {
                    Serial.print("发布结果响应: ");
                    Serial.println(final_response);
                    Serial.println("[错误] QMTPUB 明确报告发送失败 (result=2)。");
                    mqttNeedReconnect = true;
                    break;
                }
                if (final_response.indexOf("ERROR") != -1 || final_response.indexOf("+CME ERROR") != -1) {
                    Serial.print("发布结果响应: ");
                    Serial.println(final_response);
                    Serial.println("[错误] QMTPUB 过程中收到 ERROR。响应: " + final_response);
                    mqttNeedReconnect = true;
                    break;
                }
            }
        }
        if (final_response.indexOf("+QMTPUB: 0,1,0") != -1 || final_response.indexOf("\nOK\r") != -1) {
            // 虽然上面已 return，但保险起见
            return true;
        }
        Serial.println("[错误] QMTPUB 等待最终结果超时。当前响应: " + final_response);
        mqttNeedReconnect = true;
        attempt++;
        Serial.printf("[重试] QMTPUB 失败, 第 %d 次重试\n", attempt);
        delay(500);
    }
    return false;
}
