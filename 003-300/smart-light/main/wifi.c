#include "wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "wifi";
static bool s_connected = false;
static char s_ip[16] = "0.0.0.0";

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id,
                               void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    s_connected = false;
    ESP_LOGI(TAG, "wifi disconnected, retrying...");
    esp_wifi_connect();
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
    s_connected = true;
    unsigned int ip = (unsigned int)evt->ip_info.ip.addr;
    snprintf(s_ip, sizeof(s_ip), "%u.%u.%u.%u", ip & 0xFF, (ip >> 8) & 0xFF,
             (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
    ESP_LOGI(TAG, "got ip: %s", s_ip);
  }
}

void wifi_init(void) {
  // 初始化 NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  // 初始化 TCP/IP、默认事件循环
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();
  // WiFi 配置
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_event_handler_instance_t h1, h2;
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      wifi_event_handler, NULL, &h1);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      wifi_event_handler, NULL, &h2);
  wifi_config_t wc = {
      .sta = {.ssid = "Nicolette86132", .password = "Yibotian123"}};
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wc);
  esp_wifi_start();
  ESP_LOGI(TAG, "wifi_init finished, connecting to %s", wc.sta.ssid);
}

bool wifi_is_connected(void) { return s_connected; }

const char *wifi_get_ip(void) { return s_ip; }
