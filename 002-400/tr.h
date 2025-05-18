// tr.h
// 交通灯本地控制与数码管显示头文件
#ifndef TR_H
#define TR_H
#include <Arduino.h>
#include <TM1637Display.h>

// 交通灯时间参数
// extern int GREEN_LIGHT_TIME; // 这些如果不由 main.ino 控制，则保留在 tr.cpp 定义
// extern int YELLOW_LIGHT_TIME; // 同上

// 从 main.ino 引入的常量
extern const int FLASH_GREEN_TIME;
extern const int RED_TRANSITION_TIME;

// 交通灯引脚 (这些通常在 tr.cpp 中定义并初始化，或者如果固定不变也可以设为 const)
// 如果引脚号是固定的，也可以在 tr.h 中声明为 extern const int，并在 tr.cpp 中定义
// 但通常外设相关的引脚定义在对应的 .cpp 文件中更常见
extern const int CLK_PIN; // 假设这些是固定的
extern const int DIO_PIN;
extern const int NS_RED_PIN;
extern const int NS_YELLOW_PIN;
extern const int NS_GREEN_PIN;
extern const int EW_RED_PIN;
extern const int EW_YELLOW_PIN;
extern const int EW_GREEN_PIN; // 确保这个引脚也被 extern

// 数码管对象声明
extern TM1637Display display;

// 数码管逻辑位置 (这些也可以是 const)
extern const int NORTH_SOUTH_DISPLAY_POS;
extern const int EAST_WEST_DISPLAY_POS;


// 函数声明
void setNorthSouthRed(bool on);
void setNorthSouthYellow(bool on);
void setNorthSouthGreen(bool on);
void setEastWestRed(bool on);
void setEastWestYellow(bool on);
void setEastWestGreen(bool on);

void showActiveCountdown(int sec, int position);
void blankTwoDigits(int position);
void clearAllDisplays();
void setTrafficLightPhase(const char* phase);
// bool toggleGreenLightState(bool current); // 如果这个函数不再需要，可以移除

#endif