#include "board_init.h"
#include "tim.h"
#include "adc.h"
#include "dma.h"
#include "usart.h"
#include "oled.h"
#include "key.h"

void board_init(void) {
    // 系统时钟配置
    system_clock_config();
    // GPIO初始化
    gpio_init();
    // TIM定时器初始化
    tim_init();
    // ADC+DMA配置
    adc_init();
    dma_init();
    // USART配置
    usart_init();
    // OLED显示初始化
    oled_init();
    // 按键初始化
    key_init();
}

// 以下为各外设初始化函数的声明（实际应在对应驱动文件实现）
void system_clock_config(void) {
    // 配置系统时钟（具体实现视芯片库而定）
}
void gpio_init(void) {
    // 配置GPIO（具体实现视硬件连接而定）
}
void tim_init(void) {
    // 配置TIM定时器（具体实现视芯片库而定）
}
void adc_init(void) {
    // 配置ADC（具体实现视芯片库而定）
}
void dma_init(void) {
    // 配置DMA（具体实现视芯片库而定）
}
void usart_init(void) {
    // 配置USART（具体实现视芯片库而定）
}
void oled_init(void) {
    // 配置OLED显示（具体实现视硬件连接而定）
}
void key_init(void) {
    // 配置按键（具体实现视硬件连接而定）
}