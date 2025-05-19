#include "board_init.h"
#include "app_motor.h"
#include "app_sensor.h"
#include "app_comm.h"
#include "app_ui.h"
#include "key.h"

int main(void) {
    // 系统初始化
    board_init();
    motor_init();
    sensor_init();
    comm_init();
    ui_init();

    // 外设初始化（GPIO/DMA/ADC/TIM/USART/I2C）
    // 已在 board_init() 内部完成

    while (1) {
        // 按键扫描
        key_scan();
        // 步进电机运行
        motor_run();
        // 传感器采集
        sensor_read();
        // 通信数据发送（如有新数据）
        comm_send();
        // UI刷新
        ui_update();
    }
    // 系统待机/关闭（实际嵌入式系统一般不会到达此处）
    return 0;
}