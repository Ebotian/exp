// ESP32 <-> BC260Y MQTT 通信

// --- BC260Y 串口引脚定义 ---
#define BC260Y_RX_PIN 23 // ESP32 的 RX, 连接到 BC260Y 的 TX
#define BC260Y_TX_PIN 22 // ESP32 的 TX, 连接到 BC260Y 的 RX
HardwareSerial bcSerial(2); // 使用 UART2

// --- MQTT 参数 ---
const char* MQTT_BROKER_HOST = "39.107.106.220"; // 您的 EMQX 服务器 IP
const int MQTT_BROKER_PORT = 1883;             // NB-IoT 标准 MQTT 端口
const char* MQTT_CLIENT_ID = "esp32-traffic-light-device-01"; // 唯一的客户端 ID
// 如果您的 EMQX 需要 NB-IoT 连接认证，请填写这些：
const char* MQTT_USERNAME = ""; // 您的 MQTT 用户名 (如果有)
const char* MQTT_PASSWORD = ""; // 您的 MQTT 密码 (如果有)

const char* MQTT_TOPIC_CONTROL = "trafficlight/control"; // 用于订阅控制指令的主题
const char* MQTT_TOPIC_STATUS = "trafficlight/status";   // 用于发布状态的主题

// --- 辅助变量 ---
unsigned long previousMillis = 0;
const unsigned long atCommandInterval = 10000; // 定期发布状态或检查的间隔时间 (毫秒)

// 函数：发送AT指令并等待特定响应
// 如果找到 expectedResponse，则返回 true，否则返回 false
bool sendATCommand(String command, String expectedResponse, unsigned long timeout, bool printDebug = true) {
    String response = "";
    if (printDebug) {
        Serial.print("发送: "); // 中文：发送
        Serial.println(command);
    }
    bcSerial.print(command);

    unsigned long startTime = millis();
    while (millis() - startTime < timeout) {
        if (bcSerial.available()) {
            char c = bcSerial.read();
            response += c;
        }
        // 频繁检查当前累积的响应中是否包含预期响应
        if (response.indexOf(expectedResponse) != -1) {
            if (printDebug) {
                Serial.print("收到: "); // 中文：收到
                Serial.println(response);
            }
            return true;
        }
    }
    if (printDebug) {
        Serial.print("超时. 收到: "); // 中文：超时. 收到:
        Serial.println(response);
    }
    return false;
}

// 函数：发送AT指令并获取完整的响应字符串
String sendATCommandGetResponse(String command, unsigned long timeout, bool printDebug = true) {
    String response = "";
     if (printDebug) {
        Serial.print("发送: "); // 中文：发送
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
        Serial.print("收到: "); // 中文：收到
        Serial.println(response);
    }
    return response;
}


bool initializeBC260Y() {
    Serial.println("初始化 BC260Y..."); // 中文：初始化 BC260Y...
    if (!sendATCommand("AT\r\n", "OK", 2000)) return false; // 基本检查
    if (!sendATCommand("ATE0\r\n", "OK", 2000)) return false; // 关闭回显，使响应更干净
    if (!sendATCommand("AT+CFUN=1\r\n", "OK", 5000)) return false; // 设置为全功能模式
    delay(1000); // 给模块一些时间
    if (!sendATCommand("AT+CPIN?\r\n", "+CPIN: READY", 5000)) return false; // 检查SIM卡

    Serial.println("检查网络注册状态..."); // 中文：检查网络注册状态...
    int retries = 10;
    bool registered = false;
    while(retries-- > 0) {
        String resp = sendATCommandGetResponse("AT+CEREG?\r\n", 3000, true);
        // 不同模块的第一个参数可能不同
        if (resp.indexOf("+CEREG: 0,1") != -1 || resp.indexOf("+CEREG: 0,5") != -1 ||
            resp.indexOf("+CEREG: 1,1") != -1 || resp.indexOf("+CEREG: 1,5") != -1 ||
            resp.indexOf("+CEREG: 2,1") != -1 || resp.indexOf("+CEREG: 2,5") != -1 ) {
            Serial.println("网络已注册。"); // 中文：网络已注册。
            registered = true;
            break;
        }
        Serial.println("尚未注册，正在重试..."); // 中文：尚未注册，正在重试...
        delay(3000);
    }
    if (!registered) {
        Serial.println("网络注册失败。"); // 中文：网络注册失败。
        return false;
    }
    // 可选：如果需要，配置APN，尽管BC260Y通常会自动处理
    // sendATCommand("AT+CGDCONT=1,\"IP\",\"your_apn_here\"\r\n", "OK", 5000);
    return true;
}

bool mqttOpen() {
    Serial.println("打开 MQTT 客户端实例..."); // 中文：打开 MQTT 客户端实例...
    String cmd = "AT+QMTOPEN=0,\"" + String(MQTT_BROKER_HOST) + "\"," + String(MQTT_BROKER_PORT) + "\r\n";
    String response = sendATCommandGetResponse(cmd, 10000, true); // 打开操作可能需要时间
    // 成功打开的响应是 "+QMTOPEN: 0,0"。如果已打开，可能是 "+QMTOPEN: 0,2"
    if (response.indexOf("+QMTOPEN: 0,0") != -1 || response.indexOf("+QMTOPEN: 0,2") != -1) {
        Serial.println("MQTT 客户端实例打开成功 (或已打开)。"); // 中文：MQTT 客户端实例打开成功 (或已打开)。
        return true;
    }
    Serial.println("打开 MQTT 客户端实例失败。"); // 中文：打开 MQTT 客户端实例失败。
    return false;
}

bool mqttConnect() {
    Serial.println("连接到 MQTT 代理..."); // 中文：连接到 MQTT 代理...
    String cmd = "AT+QMTCONN=0,\"" + String(MQTT_CLIENT_ID) + "\"";
    if (strlen(MQTT_USERNAME) > 0) {
        cmd += ",\"" + String(MQTT_USERNAME) + "\"";
        if (strlen(MQTT_PASSWORD) > 0) {
            cmd += ",\"" + String(MQTT_PASSWORD) + "\"";
        }
    }
    cmd += "\r\n";
    String response = sendATCommandGetResponse(cmd, 15000, true); // 连接可能需要较长时间
    if (response.indexOf("+QMTCONN: 0,0,0") != -1) {
        Serial.println("成功连接到 MQTT 代理！"); // 中文：成功连接到 MQTT 代理！
        return true;
    }
    Serial.println("连接到 MQTT 代理失败。"); // 中文：连接到 MQTT 代理失败。
    // 如果连接失败，最好在重试前关闭 QMTOPEN 实例
    // sendATCommand("AT+QMTCLOSE=0\r\n", "OK", 5000);
    return false;
}

bool mqttSubscribe() {
    Serial.println("订阅控制主题..."); // 中文：订阅控制主题...
    String cmd = "AT+QMTSUB=0,1,\"" + String(MQTT_TOPIC_CONTROL) + "\",1\r\n"; // QoS 1
    String response = sendATCommandGetResponse(cmd, 5000, true);
    // 成功订阅的响应: "+QMTSUB: 0,1,0,1" (最后一个1表示QoS 1)
    if (response.indexOf("+QMTSUB: 0,1,0") != -1) { // 检查主要部分
        Serial.println("成功订阅主题 " + String(MQTT_TOPIC_CONTROL)); // 中文：成功订阅主题
        return true;
    }
    Serial.println("订阅主题 " + String(MQTT_TOPIC_CONTROL) + " 失败"); // 中文：订阅主题 ... 失败
    return false;
}

bool mqttPublish(String topic, String payload) {
    Serial.println("发布消息..."); // 中文：发布消息...
    // AT+QMTPUB 的最大负载长度通常有限制 (例如 512 或 1024 字节)。
    // 对于更长的负载，可能需要 AT+QMTPUBEX，或者如果模块支持，则分块发送。
    // 这里假设负载足够短。
    String cmd = "AT+QMTPUB=0,1,1,0,\"" + topic + "\",\"" + payload + "\"\r\n"; // QoS 1, No Retain
    String response = sendATCommandGetResponse(cmd, 5000, true);
    // 成功发布的响应: "+QMTPUB: 0,1,0" (msgID 1, result 0)
    if (response.indexOf("+QMTPUB: 0,1,0") != -1 || response.indexOf("OK") != -1) { // 有些模块在 +QMTPUB URC 后只返回 OK
        Serial.println("消息成功发布到主题 " + topic); // 中文：消息成功发布到主题
        return true;
    }
    Serial.println("发布消息到主题 " + topic + " 失败"); // 中文：发布消息到主题 ... 失败
    return false;
}


void setup() {
    Serial.begin(115200); // 用于 ESP32 调试
    bcSerial.begin(9600, SERIAL_8N1, BC260Y_RX_PIN, BC260Y_TX_PIN); // 用于 BC260Y
    Serial.println("\nESP32 MQTT 客户端 (BC260Y) - Setup 启动"); // 中文：ESP32 MQTT 客户端 (BC260Y) - Setup 启动
    delay(2000); // 上电后等待模块准备就绪

    if (!initializeBC260Y()) {
        Serial.println("BC260Y 初始化失败。停止运行。"); // 中文：BC260Y 初始化失败。停止运行。
        while (1); // 停止
    }

    if (!mqttOpen()) {
        Serial.println("MQTT 打开失败。重试一次..."); // 中文：MQTT 打开失败。重试一次...
        sendATCommand("AT+QMTCLOSE=0\r\n", "OK", 3000, false); // 如果处于异常状态，尝试关闭
        delay(1000);
        if(!mqttOpen()){
            Serial.println("MQTT 再次打开失败。停止运行。"); // 中文：MQTT 再次打开失败。停止运行。
            while (1); // 停止
        }
    }

    if (!mqttConnect()) {
        Serial.println("MQTT 连接失败。停止运行。"); // 中文：MQTT 连接失败。停止运行。
        while (1); // 停止
    }

    if (!mqttSubscribe()) {
        Serial.println("MQTT 订阅失败。无订阅继续运行可能会有问题。"); // 中文：MQTT 订阅失败。无订阅继续运行可能会有问题。
        // 决定是停止还是继续
    }

    Serial.println("Setup 完成。ESP32 已连接到 MQTT 并已订阅。"); // 中文：Setup 完成。ESP32 已连接到 MQTT 并已订阅。
    // 示例：发布一个初始状态
    mqttPublish(MQTT_TOPIC_STATUS, "{\"status\":\"online\",\"esp32\":\"ready\"}"); // 中文：在线，就绪
}

void loop() {
    // 检查来自 BC260Y 的传入数据 (例如 MQTT 消息)
    if (bcSerial.available()) {
        String data = bcSerial.readString(); // 读取所有可用数据
        Serial.print("来自 BC260Y 的数据: "); // 中文：来自 BC260Y 的数据:
        Serial.println(data);

        // 对接收到的 MQTT 消息进行基本解析
        // URC 格式: +QMTRECV: 0,<msgId>,"<topic>","<payload>"
        if (data.indexOf("+QMTRECV:") != -1) {
            // 更健壮的解析器会正确提取主题和负载
            Serial.println("收到 MQTT 消息！"); // 中文：收到 MQTT 消息！
            // 示例: if (data.indexOf(MQTT_TOPIC_CONTROL) != -1) { ... }
            // 然后解析负载: data.substring(...)
            // 目前只是打印它。
            // 您需要实现从 'data' 字符串中解析主题和负载的逻辑，
            // 然后根据指令执行操作 (例如控制交通灯)。
        }
    }

    // 示例：定期发布状态更新
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= atCommandInterval) {
        previousMillis = currentMillis;
        String statusPayload = "{\"uptime\":" + String(millis()/1000) + ",\"heap\":" + String(ESP.getFreeHeap()) + "}"; // 中文：运行时间，堆内存
        mqttPublish(MQTT_TOPIC_STATUS, statusPayload);
    }
}