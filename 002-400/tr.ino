#include <TM1637Display.h> // 包含TM1637数码管库

// 定义交通信号灯时间常量（单位：秒）
const int GREEN_LIGHT_TIME = 10;     // 绿灯时间
const int YELLOW_LIGHT_TIME = 3;     // 黄灯时间
const int RED_TRANSITION_TIME = 2;    // 全红过渡时间
const int FLASH_GREEN_TIME = 5;      // 绿灯闪烁预警时间

// 定义7段数码管的引脚
const int CLK_PIN = 5;  // D5 (GPIO5) 连接到数码管 CLK
const int DIO_PIN = 4;  // D4 (GPIO4) 连接到数码管 DIO

// 定义南北方向交通灯的引脚
const int NS_RED_PIN = 14;    // D14 (GPIO14) 控制南北红灯
const int NS_YELLOW_PIN = 12; // D12 (GPIO12) 控制南北黄灯
const int NS_GREEN_PIN = 13;  // D13 (GPIO13) 控制南北绿灯

// 定义东西方向交通灯的引脚
const int EW_RED_PIN = 16;    // D16 (GPIO16) 控制东西红灯
const int EW_YELLOW_PIN = 2;  // D2 (GPIO2)   控制东西黄灯
const int EW_GREEN_PIN = 15;  // D15 (GPIO15) 控制东西绿灯

// 初始化7段数码管对象
TM1637Display display(CLK_PIN, DIO_PIN);

// 定义数码管的逻辑位置
// !!! 注意：您可能需要根据实际模块的接线调换这两个值 !!!
// 假设 "南北计时" 是第一组 (位置0), "东西计时" 是第二组 (位置2)
const int NORTH_SOUTH_DISPLAY_POS = 0; // 南北数码管的起始位置 (通常是0)
const int EAST_WEST_DISPLAY_POS = 2;   // 东西数码管的起始位置 (通常是2, 如果是4位数码管模块)

void setup() {
  Serial.begin(115200); // 初始化串口通信，用于调试

  // 初始化交通灯引脚为输出模式
  pinMode(NS_RED_PIN, OUTPUT);
  pinMode(NS_YELLOW_PIN, OUTPUT);
  pinMode(NS_GREEN_PIN, OUTPUT);
  pinMode(EW_RED_PIN, OUTPUT);
  pinMode(EW_YELLOW_PIN, OUTPUT);
  pinMode(EW_GREEN_PIN, OUTPUT);

  // 初始化所有灯为熄灭状态
  digitalWrite(NS_RED_PIN, HIGH);
  digitalWrite(NS_YELLOW_PIN, HIGH);
  digitalWrite(NS_GREEN_PIN, HIGH);
  digitalWrite(EW_RED_PIN, HIGH);
  digitalWrite(EW_YELLOW_PIN, HIGH);
  digitalWrite(EW_GREEN_PIN, HIGH);

  // 设置7段数码管的亮度
  display.setBrightness(0x0f); // 设置为最大亮度
  display.clear(); // 清除数码管显示
  Serial.println("交通灯控制系统已初始化");

  // 短暂测试数码管位置
  Serial.println("测试数码管位置...");
  Serial.print("在位置 "); Serial.print(NORTH_SOUTH_DISPLAY_POS); Serial.println(" 显示 11");
  display.showNumberDec(11, false, 2, NORTH_SOUTH_DISPLAY_POS);
  Serial.print("在位置 "); Serial.print(EAST_WEST_DISPLAY_POS); Serial.println(" 显示 22");
  display.showNumberDec(22, false, 2, EAST_WEST_DISPLAY_POS);
  delay(4000); // 显示4秒钟，方便观察
  display.clear();
  Serial.println("数码管位置测试结束。");
}

// --- 灯光控制函数 ---
void setNorthSouthRed(bool on) {
  digitalWrite(NS_RED_PIN, on ? LOW : HIGH);
  Serial.print("南北红灯: "); Serial.println(on ? "亮" : "灭");
}
void setNorthSouthYellow(bool on) {
  digitalWrite(NS_YELLOW_PIN, on ? LOW : HIGH);
  Serial.print("南北黄灯: "); Serial.println(on ? "亮" : "灭");
}
void setNorthSouthGreen(bool on) {
  digitalWrite(NS_GREEN_PIN, on ? LOW : HIGH);
  Serial.print("南北绿灯: "); Serial.println(on ? "亮" : "灭");
}
void setEastWestRed(bool on) {
  digitalWrite(EW_RED_PIN, on ? LOW : HIGH);
  Serial.print("东西红灯: "); Serial.println(on ? "亮" : "灭");
}
void setEastWestYellow(bool on) {
  digitalWrite(EW_YELLOW_PIN, on ? LOW : HIGH);
  Serial.print("东西黄灯: "); Serial.println(on ? "亮" : "灭");
}
void setEastWestGreen(bool on) {
  digitalWrite(EW_GREEN_PIN, on ? LOW : HIGH);
  Serial.print("东西绿灯: "); Serial.println(on ? "亮" : "灭");
}

// --- 数码管控制函数 ---
// 在指定位置显示两位倒计时数字
void showActiveCountdown(int number, int position) {
  if (number < 0) number = 0;
  if (number > 99) number = 99;
  display.showNumberDec(number, false, 2, position);
  Serial.print("数码管位置 "); Serial.print(position); Serial.print(" 显示: "); Serial.println(number);
}

// 清空指定的两位数码管 (通过显示空白段实现)
void blankTwoDigits(int position) {
  uint8_t blankSegments[] = {0x00, 0x00}; // 两个全灭的段码
  display.setSegments(blankSegments, 2, position);
  Serial.print("数码管位置 "); Serial.print(position); Serial.println(" 已清空");
}

void clearAllDisplays() {
  display.clear();
  Serial.println("所有数码管已清除");
}

// --- 交通灯控制函数 ---
void setTrafficLightPhase(const char* phase) {
  Serial.println("\n======================");
  Serial.print("当前阶段："); Serial.println(phase);
  Serial.println("======================");
}

// 绿灯闪烁函数（预警即将变黄）
void flashGreenLight(bool isNorthSouth) {
  for (int i = FLASH_GREEN_TIME; i >= 1; i--) {
    if (isNorthSouth) {
      setNorthSouthGreen(i % 2 == 0);
      showActiveCountdown(i, NORTH_SOUTH_DISPLAY_POS);
    } else {
      setEastWestGreen(i % 2 == 0);
      showActiveCountdown(i, EAST_WEST_DISPLAY_POS);
    }
    delay(1000);
  }
}

// --- 交通灯序列 ---
void loop() {
  // 阶段 1: 南北方向通行（绿灯）
  setTrafficLightPhase("南北方向通行");
  setNorthSouthGreen(true); setNorthSouthYellow(false); setNorthSouthRed(false);
  setEastWestRed(true); setEastWestYellow(false); setEastWestGreen(false);
  blankTwoDigits(EAST_WEST_DISPLAY_POS); // 清空东西方向数码管

  // 绿灯主要时间
  for (int i = GREEN_LIGHT_TIME; i > FLASH_GREEN_TIME; i--) {
    showActiveCountdown(i, NORTH_SOUTH_DISPLAY_POS);
    delay(1000);
  }

  // 绿灯闪烁预警
  flashGreenLight(true);

  // 阶段 2: 南北方向黄灯
  setTrafficLightPhase("南北方向请注意，黄灯倒计时");
  setNorthSouthGreen(false); setNorthSouthYellow(true);
  for (int i = YELLOW_LIGHT_TIME; i >= 1; i--) {
    showActiveCountdown(i, NORTH_SOUTH_DISPLAY_POS); // 南北方向显示黄灯倒计时
    showActiveCountdown(i, EAST_WEST_DISPLAY_POS);   // 东西方向也显示倒计时（准备通行）
    delay(1000);
  }

  // 阶段 3: 全红过渡
  setTrafficLightPhase("全红过渡 - 等待清空路口");
  setNorthSouthYellow(false); setNorthSouthRed(true);
  for (int i = RED_TRANSITION_TIME; i >= 1; i--) {
    showActiveCountdown(i, NORTH_SOUTH_DISPLAY_POS);
    showActiveCountdown(i, EAST_WEST_DISPLAY_POS);
    delay(1000);
  }

  // 阶段 4: 东西方向通行（绿灯）
  setTrafficLightPhase("东西方向通行");
  setEastWestGreen(true); setEastWestYellow(false); setEastWestRed(false);
  setNorthSouthRed(true); setNorthSouthYellow(false); setNorthSouthGreen(false);
  blankTwoDigits(NORTH_SOUTH_DISPLAY_POS); // 清空南北方向数码管

  // 绿灯主要时间
  for (int i = GREEN_LIGHT_TIME; i > FLASH_GREEN_TIME; i--) {
    showActiveCountdown(i, EAST_WEST_DISPLAY_POS);
    delay(1000);
  }

  // 绿灯闪烁预警
  flashGreenLight(false);

  // 阶段 5: 东西方向黄灯
  setTrafficLightPhase("东西方向请注意，黄灯倒计时");
  setEastWestGreen(false); setEastWestYellow(true);
  for (int i = YELLOW_LIGHT_TIME; i >= 1; i--) {
    showActiveCountdown(i, EAST_WEST_DISPLAY_POS);   // 东西方向显示黄灯倒计时
    showActiveCountdown(i, NORTH_SOUTH_DISPLAY_POS); // 南北方向也显示倒计时（准备通行）
    delay(1000);
  }

  // 阶段 6: 全红过渡
  setTrafficLightPhase("全红过渡 - 等待清空路口");
  setEastWestYellow(false); setEastWestRed(true);
  for (int i = RED_TRANSITION_TIME; i >= 1; i--) {
    showActiveCountdown(i, EAST_WEST_DISPLAY_POS);
    showActiveCountdown(i, NORTH_SOUTH_DISPLAY_POS);
    delay(1000);
  }

  // 循环结束，清理显示
  clearAllDisplays();
}