#include "voice.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define UART_PORT_NUM UART_NUM_2
#define UART_TX_PIN 17
#define UART_RX_PIN 16

static bool voice_light_on = false;
static bool voice_light_valid = false;

void voice_init(void) {
  const uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };
  uart_driver_install(UART_PORT_NUM, 256, 0, 0, NULL, 0);
  uart_param_config(UART_PORT_NUM, &uart_config);
  uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
}

// 提供给 main.c 查询语音指令状态
void voice_get_light_state(bool *on, bool *valid) {
  if (on)
    *on = voice_light_on;
  if (valid)
    *valid = voice_light_valid;
}

void voice_task(void *pvParameter) {
  voice_init();
  printf("[VOICE UART] Voice task started. Listening for commands on UART2 "
         "(TX=17, RX=16)...\n");
  static char cmd_buf[64];
  static int cmd_len = 0;
  uint8_t data;
  while (1) {
    int len = uart_read_bytes(UART_PORT_NUM, &data, 1, 20 / portTICK_PERIOD_MS);
    if (len > 0) {
      printf("[VOICE UART] Received: 0x%02X ('%c')\n", data,
             (data >= 32 && data <= 126) ? data : '.');
      // 滑动窗口方式查找 <G> 前缀命令
      if (cmd_len < (int)sizeof(cmd_buf) - 1) {
        cmd_buf[cmd_len++] = data;
        cmd_buf[cmd_len] = '\0';
      } else {
        // 缓冲区满，前移一位
        memmove(cmd_buf, cmd_buf + 1, sizeof(cmd_buf) - 2);
        cmd_buf[sizeof(cmd_buf) - 2] = data;
        cmd_buf[sizeof(cmd_buf) - 1] = '\0';
      }
      // 检查是否有 <G>lighton 或 <G>lightoff
      char *gptr = strstr(cmd_buf, "<G>");
      if (gptr) {
        if (strstr(gptr, "<G>lighton")) {
          voice_light_on = true;
          voice_light_valid = true;
          printf("[VOICE] 识别到 <G>lighton 指令，语音开灯\n");
          // 清空缓冲区，防止重复触发
          cmd_len = 0;
          cmd_buf[0] = '\0';
        } else if (strstr(gptr, "<G>lightoff")) {
          voice_light_on = false;
          voice_light_valid = true;
          printf("[VOICE] 识别到 <G>lightoff 指令，语音关灯\n");
          cmd_len = 0;
          cmd_buf[0] = '\0';
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
