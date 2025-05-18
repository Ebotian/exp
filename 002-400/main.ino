#include <Arduino.h>
#include <ArduinoJson.h>
#include "premain.h" // 确保这个文件包含了 BC260Y_RX_PIN, BC260Y_TX_PIN, MQTT_TOPIC_CONTROL, MQTT_TOPIC_STATUS, 以及 initializeBC260Y, mqttOpen, mqttConnect, mqttSubscribe, mqttPublish, mqttReconnect 函数的声明或定义
#include "tr.h"

// 交通灯参数（可被MQTT动态修改）
volatile int nsGreen = 15; // 总的南北绿灯时间 (常亮+闪烁)
volatile int nsYellow = 5;
volatile int ewGreen = 15; // 总的东西绿灯时间 (常亮+闪烁)
volatile int ewYellow = 5;
volatile int modeIndex = 0;

// 固定参数
const int FLASH_GREEN_TIME = 3; // 绿灯最后3秒闪烁
const int RED_TRANSITION_TIME = 2; // 全红过渡时间

// --- 解析前端下发的set_mode消息 ---
void handleMqttSetMode(const String& payload) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print("JSON解析失败: "); Serial.println(err.c_str());
        return;
    }
    if (!doc.containsKey("cmd")) return;
    JsonObject cmd = doc["cmd"];
    if (!cmd.containsKey("type")) return;
    String type = cmd["type"];
    if (type != "set_mode") return;

    if (cmd.containsKey("modeIndex")) modeIndex = cmd["modeIndex"].as<int>();

    if (cmd.containsKey("nsGreen")) {
        int val = cmd["nsGreen"].as<int>();
        nsGreen = (val >= 0) ? val : 0; // 确保非负
    }
    if (cmd.containsKey("nsYellow")) {
        int val = cmd["nsYellow"].as<int>();
        nsYellow = (val > 0) ? val : 1; // 确保为正，黄灯至少1秒 (或根据需求改为 (val >=0) ? val : 0)
    }
    if (cmd.containsKey("ewGreen")) {
        int val = cmd["ewGreen"].as<int>();
        ewGreen = (val >= 0) ? val : 0; // 确保非负
    }
    if (cmd.containsKey("ewYellow")) {
        int val = cmd["ewYellow"].as<int>();
        ewYellow = (val > 0) ? val : 1; // 确保为正，黄灯至少1秒
    }

    Serial.printf("模式切换: modeIndex=%d nsG=%d nsY=%d ewG=%d ewY=%d\n",
                  modeIndex, nsGreen, nsYellow, ewGreen, ewYellow);
}

// --- MQTT消息处理 ---
void handleMqttCommunication() {
    if (bcSerial.available()) {
        String data = "";
        unsigned long readStartTime = millis();
        while (millis() - readStartTime < 500) {
            if (bcSerial.available()) {
                char c = bcSerial.read();
                data += c;
                readStartTime = millis();
            }
        }

        if (data.length() > 0) {
            Serial.print("串口接收: "); Serial.println(data);
            if (data.indexOf("+QMTRECV:") != -1) {
                int topicStart = data.indexOf(",\"") + 2;
                int topicEnd = data.indexOf("\"", topicStart);
                if (topicStart < 2 || topicEnd == -1) {
                    Serial.println("QMTRECV 主题解析失败 (1)");
                    return;
                }
                String topic = data.substring(topicStart, topicEnd);

                int payloadHeaderPos = data.indexOf("\"", topicEnd + 1);
                if (payloadHeaderPos == -1) {
                    Serial.println("QMTRECV payload 起始引号未找到");
                    return;
                }
                int payloadStart = payloadHeaderPos + 1;
                int payloadEnd = -1;
                for (int i = data.length() - 1; i >= payloadStart; i--) {
                    if (data.charAt(i) == '"') {
                        if (i > payloadStart && data.charAt(i-1) == '\\') {
                            // continue;
                        } else {
                            payloadEnd = i;
                            break;
                        }
                    }
                }

                if (payloadEnd != -1 && payloadEnd > payloadStart) {
                    String payloadStr = data.substring(payloadStart, payloadEnd);
                    payloadStr.replace("\\\"", "\"");

                    Serial.print("解析主题: "); Serial.println(topic);
                    Serial.print("解析Payload: "); Serial.println(payloadStr);

                    if (topic == MQTT_TOPIC_CONTROL) {
                        handleMqttSetMode(payloadStr);
                    }
                } else {
                    Serial.println("QMTRECV payload 解析失败 (2)");
                }
            }
        }
    }
    if (mqttNeedReconnect) {
        mqttReconnect();
    }
}

// --- 交通灯状态机 ---
static int g_phase = 7;
static int g_countdown = 0;
static bool g_greenFlashState = true;

void runTrafficLightCycle() {
    static unsigned long lastFlashToggleMillis = 0;
    unsigned long currentMillis = millis();

    if (g_countdown <= 0) {
        g_phase = (g_phase + 1) % 8;
        lastFlashToggleMillis = currentMillis;
        g_greenFlashState = true;

        // --- 修正后的绿灯常亮和闪烁时间计算 ---
        int nsGreenSteadyTimeCalculated;
        int currentNsFlashTimeCalculated;
        if (nsGreen <= 0) { // 如果总绿灯时间为0或负
            nsGreenSteadyTimeCalculated = 0;
            currentNsFlashTimeCalculated = 0;
        } else if (nsGreen <= FLASH_GREEN_TIME) { // 如果总绿灯时间小于等于固定闪烁时间
            nsGreenSteadyTimeCalculated = 0; // 没有常亮时间
            currentNsFlashTimeCalculated = nsGreen; // 全部为闪烁时间
        } else { // 正常情况
            nsGreenSteadyTimeCalculated = nsGreen - FLASH_GREEN_TIME;
            currentNsFlashTimeCalculated = FLASH_GREEN_TIME;
        }

        int ewGreenSteadyTimeCalculated;
        int currentEwFlashTimeCalculated;
        if (ewGreen <= 0) {
            ewGreenSteadyTimeCalculated = 0;
            currentEwFlashTimeCalculated = 0;
        } else if (ewGreen <= FLASH_GREEN_TIME) {
            ewGreenSteadyTimeCalculated = 0;
            currentEwFlashTimeCalculated = ewGreen;
        } else {
            ewGreenSteadyTimeCalculated = ewGreen - FLASH_GREEN_TIME;
            currentEwFlashTimeCalculated = FLASH_GREEN_TIME;
        }
        // --- 计算结束 ---

        switch (g_phase) {
            case 0: // 南北绿灯 (常亮)
                setTrafficLightPhase("南北绿灯");
                setNorthSouthRed(false); setNorthSouthYellow(false); setNorthSouthGreen(true);
                setEastWestRed(true); setEastWestYellow(false); setEastWestGreen(false);
                g_countdown = nsGreenSteadyTimeCalculated;
                break;
            case 1: // 南北绿灯闪烁
                setTrafficLightPhase("南北绿灯闪烁");
                setNorthSouthRed(false); setNorthSouthYellow(false); setNorthSouthGreen(g_greenFlashState);
                setEastWestRed(true); setEastWestYellow(false); setEastWestGreen(false);
                g_countdown = currentNsFlashTimeCalculated;
                break;
            case 2: // 南北黄灯
                setTrafficLightPhase("南北黄灯");
                setNorthSouthRed(false); setNorthSouthYellow(true); setNorthSouthGreen(false);
                setEastWestRed(true); setEastWestYellow(false); setEastWestGreen(false);
                g_countdown = nsYellow; // nsYellow 已经过校验
                break;
            case 3: // 全红过渡 (南北转东西)
                setTrafficLightPhase("全红过渡 (NS->EW)");
                setNorthSouthRed(true); setNorthSouthYellow(false); setNorthSouthGreen(false);
                setEastWestRed(true); setEastWestYellow(false); setEastWestGreen(false);
                g_countdown = RED_TRANSITION_TIME;
                break;
            case 4: // 东西绿灯 (常亮)
                setTrafficLightPhase("东西绿灯");
                setNorthSouthRed(true); setNorthSouthYellow(false); setNorthSouthGreen(false);
                setEastWestRed(false); setEastWestYellow(false); setEastWestGreen(true);
                g_countdown = ewGreenSteadyTimeCalculated;
                break;
            case 5: // 东西绿灯闪烁
                setTrafficLightPhase("东西绿灯闪烁");
                setNorthSouthRed(true); setNorthSouthYellow(false); setNorthSouthGreen(false);
                setEastWestRed(false); setEastWestYellow(false); setEastWestGreen(g_greenFlashState);
                g_countdown = currentEwFlashTimeCalculated;
                break;
            case 6: // 东西黄灯
                setTrafficLightPhase("东西黄灯");
                setNorthSouthRed(true); setNorthSouthYellow(false); setNorthSouthGreen(false);
                setEastWestRed(false); setEastWestYellow(true); setEastWestGreen(false);
                g_countdown = ewYellow; // ewYellow 已经过校验
                break;
            case 7: // 全红过渡 (东西转南北)
                setTrafficLightPhase("全红过渡 (EW->NS)");
                setNorthSouthRed(true); setNorthSouthYellow(false); setNorthSouthGreen(false);
                setEastWestRed(true); setEastWestYellow(false); setEastWestGreen(false);
                g_countdown = RED_TRANSITION_TIME;
                break;
        }
        if (g_countdown < 0) g_countdown = 0; // 安全校验，尽管前面逻辑应避免负数
    }

    // 绿灯闪烁处理
    if (g_phase == 1) {
        if (currentMillis - lastFlashToggleMillis >= 500) {
            g_greenFlashState = !g_greenFlashState;
            setNorthSouthGreen(g_greenFlashState);
            lastFlashToggleMillis = currentMillis;
        }
    } else if (g_phase == 5) {
        if (currentMillis - lastFlashToggleMillis >= 500) {
            g_greenFlashState = !g_greenFlashState;
            setEastWestGreen(g_greenFlashState);
            lastFlashToggleMillis = currentMillis;
        }
    }

    // --- 数码管显示逻辑 ---
    // 为了显示一致性，这里也使用与上面相位切换时相同的计算逻辑获取闪烁时间
    int currentNsFlashTimeForDisplay;
    if (nsGreen <= 0) currentNsFlashTimeForDisplay = 0;
    else if (nsGreen <= FLASH_GREEN_TIME) currentNsFlashTimeForDisplay = nsGreen;
    else currentNsFlashTimeForDisplay = FLASH_GREEN_TIME;

    int currentEwFlashTimeForDisplay;
    if (ewGreen <= 0) currentEwFlashTimeForDisplay = 0;
    else if (ewGreen <= FLASH_GREEN_TIME) currentEwFlashTimeForDisplay = ewGreen;
    else currentEwFlashTimeForDisplay = FLASH_GREEN_TIME;

    int nsDisplayValue = 0;
    int ewDisplayValue = 0;

    switch (g_phase) {
        case 0:
            nsDisplayValue = g_countdown + currentNsFlashTimeForDisplay;
            showActiveCountdown(nsDisplayValue > 0 ? nsDisplayValue : 0, NORTH_SOUTH_DISPLAY_POS);
            blankTwoDigits(EAST_WEST_DISPLAY_POS);
            break;
        case 1:
            nsDisplayValue = g_countdown;
            showActiveCountdown(nsDisplayValue > 0 ? nsDisplayValue : 0, NORTH_SOUTH_DISPLAY_POS);
            blankTwoDigits(EAST_WEST_DISPLAY_POS);
            break;
        case 2:
            nsDisplayValue = g_countdown;
            showActiveCountdown(nsDisplayValue > 0 ? nsDisplayValue : 0, NORTH_SOUTH_DISPLAY_POS);
            blankTwoDigits(EAST_WEST_DISPLAY_POS);
            break;
        case 4:
            ewDisplayValue = g_countdown + currentEwFlashTimeForDisplay;
            showActiveCountdown(ewDisplayValue > 0 ? ewDisplayValue : 0, EAST_WEST_DISPLAY_POS);
            blankTwoDigits(NORTH_SOUTH_DISPLAY_POS);
            break;
        case 5:
            ewDisplayValue = g_countdown;
            showActiveCountdown(ewDisplayValue > 0 ? ewDisplayValue : 0, EAST_WEST_DISPLAY_POS);
            blankTwoDigits(NORTH_SOUTH_DISPLAY_POS);
            break;
        case 6:
            ewDisplayValue = g_countdown;
            showActiveCountdown(ewDisplayValue > 0 ? ewDisplayValue : 0, EAST_WEST_DISPLAY_POS);
            blankTwoDigits(NORTH_SOUTH_DISPLAY_POS);
            break;
        case 3:
        case 7:
            nsDisplayValue = g_countdown;
            ewDisplayValue = g_countdown;
            showActiveCountdown(nsDisplayValue > 0 ? nsDisplayValue : 0, NORTH_SOUTH_DISPLAY_POS);
            showActiveCountdown(ewDisplayValue > 0 ? ewDisplayValue : 0, EAST_WEST_DISPLAY_POS);
            break;
    }

    // 1秒递减倒计时
    static unsigned long lastTickMillis = 0;
    if (currentMillis - lastTickMillis >= 1000) {
        if (g_countdown > 0) {
            g_countdown--;
        }
        lastTickMillis = currentMillis;
    }
}

// --- 交通灯硬件初始化 ---
void setupTrafficLights() {
    pinMode(NS_RED_PIN, OUTPUT);
    pinMode(NS_YELLOW_PIN, OUTPUT);
    pinMode(NS_GREEN_PIN, OUTPUT);
    pinMode(EW_RED_PIN, OUTPUT);
    pinMode(EW_YELLOW_PIN, OUTPUT);
    pinMode(EW_GREEN_PIN, OUTPUT);

    digitalWrite(NS_RED_PIN, HIGH);
    digitalWrite(NS_YELLOW_PIN, HIGH);
    digitalWrite(NS_GREEN_PIN, HIGH);
    digitalWrite(EW_RED_PIN, HIGH);
    digitalWrite(EW_YELLOW_PIN, HIGH);
    digitalWrite(EW_GREEN_PIN, HIGH);

    display.setBrightness(0x0f);
    clearAllDisplays();
}

void setup() {
    Serial.begin(115200);
    bcSerial.begin(9600, SERIAL_8N1, BC260Y_RX_PIN, BC260Y_TX_PIN);

    setupTrafficLights();

    Serial.println("系统初始化...");
    delay(2000);

    if (!initializeBC260Y()) {
        Serial.println("BC260Y 初始化失败，系统挂起。");
        while(1) delay(1000);
    }
    if (!mqttOpen()) {
        Serial.println("MQTT 打开失败，系统挂起。");
        while(1) delay(1000);
    }
    if (!mqttConnect()) {
        Serial.println("MQTT 连接失败，系统挂起。");
        while(1) delay(1000);
    }
    if (!mqttSubscribe()) {
        Serial.println("MQTT 订阅失败，系统挂起。");
        while(1) delay(1000);
    }

    mqttPublish(MQTT_TOPIC_STATUS, "{\"status\":\"online\",\"message\":\"ESP32 Traffic Light Controller Connected\"}");
    Serial.println("系统准备就绪。");
    g_countdown = 0;
}

void loop() {
    handleMqttCommunication();
    runTrafficLightCycle();

    static unsigned long lastReportMillis = 0;
    unsigned long currentMillis = millis();

    if (currentMillis - lastReportMillis >= 1000) {
        lastReportMillis = currentMillis;
        StaticJsonDocument<256> doc;

        String nsColorStr = "red";
        int nsSecondsVal = 0;
        String ewColorStr = "red";
        int ewSecondsVal = 0;

        // 为了MQTT上报，再次获取当前相位计算出的闪烁时间
        int currentNsFlashTimeForReport;
        if (nsGreen <= 0) currentNsFlashTimeForReport = 0;
        else if (nsGreen <= FLASH_GREEN_TIME) currentNsFlashTimeForReport = nsGreen;
        else currentNsFlashTimeForReport = FLASH_GREEN_TIME;

        int currentEwFlashTimeForReport;
        if (ewGreen <= 0) currentEwFlashTimeForReport = 0;
        else if (ewGreen <= FLASH_GREEN_TIME) currentEwFlashTimeForReport = ewGreen;
        else currentEwFlashTimeForReport = FLASH_GREEN_TIME;


        switch (g_phase) {
            case 0:
                nsColorStr = "green";
                nsSecondsVal = g_countdown + currentNsFlashTimeForReport;
                ewColorStr = "red";
                ewSecondsVal = g_countdown + currentNsFlashTimeForReport + nsYellow + RED_TRANSITION_TIME;
                break;
            case 1:
                nsColorStr = "green";
                nsSecondsVal = g_countdown;
                ewColorStr = "red";
                ewSecondsVal = g_countdown + nsYellow + RED_TRANSITION_TIME;
                break;
            case 2:
                nsColorStr = "yellow";
                nsSecondsVal = g_countdown;
                ewColorStr = "red";
                ewSecondsVal = g_countdown + RED_TRANSITION_TIME;
                break;
            case 3:
                nsColorStr = "red";
                nsSecondsVal = g_countdown;
                ewColorStr = "red";
                ewSecondsVal = g_countdown;
                break;
            case 4:
                ewColorStr = "green";
                ewSecondsVal = g_countdown + currentEwFlashTimeForReport;
                nsColorStr = "red";
                nsSecondsVal = g_countdown + currentEwFlashTimeForReport + ewYellow + RED_TRANSITION_TIME;
                break;
            case 5:
                ewColorStr = "green";
                ewSecondsVal = g_countdown;
                nsColorStr = "red";
                nsSecondsVal = g_countdown + ewYellow + RED_TRANSITION_TIME;
                break;
            case 6:
                ewColorStr = "yellow";
                ewSecondsVal = g_countdown;
                nsColorStr = "red";
                nsSecondsVal = g_countdown + RED_TRANSITION_TIME;
                break;
            case 7:
                nsColorStr = "red";
                nsSecondsVal = g_countdown;
                ewColorStr = "red";
                ewSecondsVal = g_countdown;
                break;
        }

        doc["nsColor"] = nsColorStr;
        doc["nsSeconds"] = nsSecondsVal > 0 ? nsSecondsVal : 0;
        doc["ewColor"] = ewColorStr;
        doc["ewSeconds"] = ewSecondsVal > 0 ? ewSecondsVal : 0;

        char jsonBuffer[256];
        size_t n = serializeJson(doc, jsonBuffer);
        if (n > 0 && n < sizeof(jsonBuffer)) {
            mqttPublish(MQTT_TOPIC_STATUS, String(jsonBuffer));
        } else {
            Serial.println("JSON序列化失败或缓冲区不足");
        }
    }
    delay(10);
}