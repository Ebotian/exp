// tr.cpp
// 交通灯本地控制与数码管显示实现文件
#include "tr.h"

// 交通灯时间参数（如果不由 main.ino 控制，则在此定义）
// int GREEN_LIGHT_TIME = 10; // 这些如果不由 main.ino 的 volatile 变量控制，则在此定义
// int YELLOW_LIGHT_TIME = 3; // 同上

// FLASH_GREEN_TIME 和 RED_TRANSITION_TIME 已在 main.ino 定义为 const,
//并通过 tr.h 的 extern const int 引入，此处不再需要定义。

// 交通灯引脚定义
const int CLK_PIN = 5;
const int DIO_PIN = 4;
const int NS_RED_PIN = 14;
const int NS_YELLOW_PIN = 12;
const int NS_GREEN_PIN = 13;
const int EW_RED_PIN = 16;
const int EW_YELLOW_PIN = 2;
const int EW_GREEN_PIN = 15;

// 数码管对象与位置定义
TM1637Display display(CLK_PIN, DIO_PIN);
const int NORTH_SOUTH_DISPLAY_POS = 0; // 假设数码管位置0是南北
const int EAST_WEST_DISPLAY_POS = 2;   // 假设数码管位置2是东西


// --- 交通灯控制函数 ---
void setNorthSouthRed(bool on) {
    digitalWrite(NS_RED_PIN, on ? LOW : HIGH); // 假设低电平点亮
    // Serial.print("南北红灯: "); Serial.println(on ? "亮" : "灭");
}
void setNorthSouthYellow(bool on) {
    digitalWrite(NS_YELLOW_PIN, on ? LOW : HIGH);
    // Serial.print("南北黄灯: "); Serial.println(on ? "亮" : "灭");
}
void setNorthSouthGreen(bool on) {
    digitalWrite(NS_GREEN_PIN, on ? LOW : HIGH);
    // Serial.print("南北绿灯: "); Serial.println(on ? "亮" : "灭");
}
void setEastWestRed(bool on) {
    digitalWrite(EW_RED_PIN, on ? LOW : HIGH);
    // Serial.print("东西红灯: "); Serial.println(on ? "亮" : "灭");
}
void setEastWestYellow(bool on) {
    digitalWrite(EW_YELLOW_PIN, on ? LOW : HIGH);
    // Serial.print("东西黄灯: "); Serial.println(on ? "亮" : "灭");
}
void setEastWestGreen(bool on) {
    digitalWrite(EW_GREEN_PIN, on ? LOW : HIGH);
    // Serial.print("东西绿灯: "); Serial.println(on ? "亮" : "灭");
}

// --- 数码管显示函数 ---
void showActiveCountdown(int sec, int position) {
    if (sec < 0) sec = 0;
    if (sec > 99) sec = 99;
    uint8_t data[] = {
        display.encodeDigit(sec / 10),
        display.encodeDigit(sec % 10)
    };
    display.setSegments(data, 2, position);
    // Serial.print("数码管位置 "); Serial.print(position); Serial.print(" 显示: "); Serial.println(sec);
}

void blankTwoDigits(int position) {
    uint8_t data[] = {0x00, 0x00}; // 全灭
    display.setSegments(data, 2, position);
    // Serial.print("数码管位置 "); Serial.print(position); Serial.println(" 已清空");
}

void clearAllDisplays() {
    display.clear();
    // Serial.println("所有数码管已清除");
}

void setTrafficLightPhase(const char* phase) {
    Serial.println("----------------------");
    Serial.print("当前阶段: "); Serial.println(phase);
    // Serial.println("----------------------");
}