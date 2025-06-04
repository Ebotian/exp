#include "display.h"
#include "driver/gpio.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ldr.h"
#include "pir.h"
#include "tcp_client.h"
#include "voice.h"
#include "webserver.h"
#include "wifi.h"
#include "ws2812.h"
#include <stdio.h>
#include <time.h>

void app_main(void) {
  display_init();
  // 初始化 WiFi 并连接（不再强制等待连接）
  wifi_init();
  // 直接显示WiFi状态，不等待
  display_show_text("Init WiFi...");
  vTaskDelay(pdMS_TO_TICKS(1000));
  printf("WiFi status: %s\n",
         wifi_is_connected() ? wifi_get_ip() : "Not connected");
  display_show_text(wifi_is_connected() ? wifi_get_ip() : "WiFi N/A");
  vTaskDelay(pdMS_TO_TICKS(2000));

  // 启动TCP客户端，连接服务器 39.107.106.220:9000
  tcp_client_start("39.107.106.220", 9000);

  // --- SNTP 时间同步 ---
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "ntp.aliyun.com");
  esp_sntp_init();
  time_t now = 0;
  struct tm timeinfo = {0};
  int retry = 0;
  const int retry_count = 10;
  while (timeinfo.tm_year < (2025 - 1900) && ++retry < retry_count) {
    printf("Waiting for system time to be set... (%d/%d)\n", retry,
           retry_count);
    vTaskDelay(pdMS_TO_TICKS(1000));
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  pir_init();
  gpio_config_t led_conf = {.pin_bit_mask = 1ULL << GPIO_NUM_2,
                            .mode = GPIO_MODE_OUTPUT,
                            .pull_up_en = GPIO_PULLUP_DISABLE,
                            .pull_down_en = GPIO_PULLDOWN_DISABLE,
                            .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&led_conf);
  ldr_init();
  ws2812_init();
  start_webserver();
  setenv("TZ", "CST-8", 1);
  tzset();
  xTaskCreate(voice_task, "voice_task", 4096, NULL, 5, NULL);

  char time_str[32];
  char light_str[32];
  // 新增：统一灯状态变量
  bool led_on = false;
  int led_brightness = 0;
  int led_r = 0, led_g = 0, led_b = 0;
  bool last_led_on = false;
  int last_led_brightness = -1;
  int last_led_r = -1, last_led_g = -1, last_led_b = -1;

  // 新增：PIR保持时间机制
  const int PIR_HOLD_MS = 5000; // 保持5秒
  int pir_hold_counter = 0;

  while (1) {
    // 1. 读取所有输入
    bool remote_power_on = false;
    bool remote_commanded_power_this_session = false;
    bool voice_on = false;
    bool voice_valid = false;
    bool pir_on = pir_detected();
    // PIR保持机制
    if (pir_on) {
      pir_hold_counter = PIR_HOLD_MS / 500; // 每次循环500ms
    } else if (pir_hold_counter > 0) {
      pir_hold_counter--;
    }
    bool pir_effective = (pir_hold_counter > 0);
    int light = ldr_read();
    voice_get_light_state(&voice_on, &voice_valid);
    bool remote_available = false;
    if (tcp_client_is_connected()) {
      remote_power_on = tcp_client_get_power_state_from_remote(
          &remote_commanded_power_this_session);
      remote_available = remote_commanded_power_this_session;
    }

    // 串口调试输出各输入状态
    printf("[DEBUG] remote_available=%d, remote_power_on=%d, voice_valid=%d, "
           "voice_on=%d, pir_on=%d, pir_effective=%d, light=%d\n",
           remote_available, remote_power_on, voice_valid, voice_on, pir_on,
           pir_effective, light);

    // 2. 处理优先级平等：谁最后有新指令就用谁的
    // 优先顺序：远程>语音>自动 变为 并列，谁有新指令就用谁
    // 检查远程
    if (remote_available) {
      led_on = remote_power_on;
      led_brightness = remote_power_on ? 255 : 0;
      led_r = led_g = led_b = remote_power_on ? 255 : 0;
      printf("[DEBUG] 控制来源: 远程\n");
    } else if (voice_valid) {
      led_on = voice_on;
      led_brightness = voice_on ? 255 : 0;
      led_r = led_g = led_b = voice_on ? 255 : 0;
      printf("[DEBUG] 控制来源: 语音\n");
    } else if (pir_effective) {
      // 自动模式
      if (light < 200) {
        led_on = true;
        led_brightness = 255 - (light * 255 / 400);
        if (led_brightness < 50)
          led_brightness = 50;
        if (led_brightness > 255)
          led_brightness = 255;
        led_r = led_g = led_b = led_brightness;
        printf("[DEBUG] 控制来源: 自动(暗)\n");
      } else {
        led_on = true;
        led_brightness = 10;
        led_r = led_g = led_b = 10;
        printf("[DEBUG] 控制来源: 自动(亮)\n");
      }
    } else {
      // 无人
      led_on = false;
      led_brightness = 0;
      led_r = led_g = led_b = 0;
      printf("[DEBUG] 控制来源: 无人\n");
    }

    // 3. 应用灯状态（只有变化时才操作）
    if (led_on != last_led_on || led_brightness != last_led_brightness ||
        led_r != last_led_r || led_g != last_led_g || led_b != last_led_b) {
      printf("[DEBUG] 灯状态变化: led_on=%d, brightness=%d, rgb=(%d,%d,%d)\n",
             led_on, led_brightness, led_r, led_g, led_b);
      for (int i = 0; i < 30; ++i)
        ws2812_set_pixel(i, led_r, led_g, led_b);
      ws2812_show();
      gpio_set_level(GPIO_NUM_2, led_on ? 1 : 0);
      last_led_on = led_on;
      last_led_brightness = led_brightness;
      last_led_r = led_r;
      last_led_g = led_g;
      last_led_b = led_b;
    }

    // 显示信息
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);
    snprintf(light_str, sizeof(light_str), "Light: %d", light);
    display_show_2lines(time_str, light_str);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
