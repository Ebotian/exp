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
  // 初始化 WiFi 并连接
  wifi_init();
  while (!wifi_is_connected()) {
    printf("Waiting for WiFi...\n");
    display_show_text("Waiting WiFi...");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  printf("WiFi connected, IP: %s\n", wifi_get_ip());
  display_show_text(wifi_get_ip());
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

  // 初始化 PIR
  pir_init();
  // 初始化板载LED (GPIO2)
  gpio_config_t led_conf = {.pin_bit_mask = 1ULL << GPIO_NUM_2,
                            .mode = GPIO_MODE_OUTPUT,
                            .pull_up_en = GPIO_PULLUP_DISABLE,
                            .pull_down_en = GPIO_PULLDOWN_DISABLE,
                            .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&led_conf);
  // 初始化 LDR
  ldr_init();
  // 初始化 WS2812
  ws2812_init();
  // 启动 HTTP WebServer
  start_webserver();

  // 设置上海时区
  setenv("TZ", "CST-8", 1);
  tzset();

  // 启动语音识别任务
  xTaskCreate(voice_task, "voice_task", 4096, NULL, 5, NULL);

  // 主循环：PIR+LDR 联动自动控制灯带和板载LED
  char time_str[32];
  char light_str[32];
  while (1) {
    bool remote_power_on = false;
    bool remote_commanded_power_this_session = false;
    bool voice_on = false;
    bool voice_valid = false;
    voice_get_light_state(&voice_on, &voice_valid);

    if (tcp_client_is_connected()) {
      remote_power_on = tcp_client_get_power_state_from_remote(
          &remote_commanded_power_this_session);
      if (remote_commanded_power_this_session && remote_power_on) {
        // 远程指令开灯，优先级最高
        tcp_client_apply_current_led_settings();
        vTaskDelay(pdMS_TO_TICKS(500));
        continue;
      }
    }

    // 语音优先级高于自动，低于网页
    if (voice_valid) {
      if (voice_on) {
        for (int i = 0; i < 30; ++i)
          ws2812_set_pixel(i, 255, 255, 255);
        ws2812_show();
        gpio_set_level(GPIO_NUM_2, 1);
      } else {
        for (int i = 0; i < 30; ++i)
          ws2812_set_pixel(i, 0, 0, 0);
        ws2812_show();
        gpio_set_level(GPIO_NUM_2, 0);
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // 获取当前时间
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);
    // 读取光强
    int light = ldr_read();
    snprintf(light_str, sizeof(light_str), "Light: %d", light);
    display_show_2lines(time_str, light_str);

    // 自动模式逻辑
    if (pir_detected()) {
      gpio_set_level(GPIO_NUM_2, 1); // 有人，点亮板载LED
      if (light < 200) {             // 光强较暗
        // 如果远程没有明确关灯，则执行自动开灯
        if (!(tcp_client_is_connected() &&
              remote_commanded_power_this_session && !remote_power_on)) {
          int brightness = 255 - (light * 255 / 400);
          if (brightness < 50)
            brightness = 50;
          if (brightness > 255)
            brightness = 255;
          for (int i = 0; i < 30; ++i)
            ws2812_set_pixel(i, brightness, brightness, brightness); // 自动白光
          ws2812_show();
        } else {
          // 远程明确关灯，即使自动条件满足，灯也保持关闭 (或远程设置的状态)
          tcp_client_apply_current_led_settings(); // 确保应用远程的关闭状态
        }
      } else { // 光强足够
        // 如果远程没有明确关灯，则执行自动调暗
        if (!(tcp_client_is_connected() &&
              remote_commanded_power_this_session && !remote_power_on)) {
          for (int i = 0; i < 30; ++i)
            ws2812_set_pixel(i, 10, 10, 10); // 自动微亮
          ws2812_show();
        } else {
          // 远程明确关灯
          tcp_client_apply_current_led_settings();
        }
      }
    } else {                         // 无人
      gpio_set_level(GPIO_NUM_2, 0); // 无人，熄灭板载LED
      // 如果远程没有明确关灯，则执行自动关灯
      if (!(tcp_client_is_connected() && remote_commanded_power_this_session &&
            !remote_power_on)) {
        for (int i = 0; i < 30; ++i)
          ws2812_set_pixel(i, 0, 0, 0); // 自动关闭灯带
        ws2812_show();
      } else {
        // 远程明确关灯
        tcp_client_apply_current_led_settings();
      }
    }

    // 如果TCP已连接，但没有收到过明确的电源命令，或者收到了明确的关灯命令，
    // 并且自动逻辑也没有操作灯（比如无人或光线强且远程关），
    // 确保应用远程的（可能是关闭）状态。
    if (tcp_client_is_connected() && remote_commanded_power_this_session &&
        !remote_power_on) {
      tcp_client_apply_current_led_settings();
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
