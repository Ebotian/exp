#include "tcp_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"    // Required for inet_addr, htons
#include "lwip/sockets.h" // Required for socket functions like connect, recv, send
#include "ws2812.h"
#include <errno.h> // Required for errno
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h> // For close()

#define RECV_BUF_SIZE 256
#define RECONNECT_DELAY_MS 5000

static const char *TAG = "tcp_client";
static int sock = -1;
static bool connected = false;
static char server_ip_str[64] = {0};
static uint16_t server_port = 0;

// Placeholder for current color and brightness state
static uint8_t current_r = 255, current_g = 255, current_b = 255;
static uint8_t current_brightness_percent = 100; // 0-100%
static bool current_power_state = false;
static bool remote_power_commanded_this_session = false; // New flag

static void apply_led_state() {
  if (!current_power_state) {
    for (int i = 0; i < 30; ++i)
      ws2812_set_pixel(i, 0, 0, 0);
  } else {
    uint8_t r = current_r * current_brightness_percent / 100;
    uint8_t g = current_g * current_brightness_percent / 100;
    uint8_t b = current_b * current_brightness_percent / 100;
    for (int i = 0; i < 30; ++i)
      ws2812_set_pixel(i, r, g, b);
  }
  ws2812_show();
}

// New function implementation
bool tcp_client_get_power_state_from_remote(bool *was_explicitly_commanded_this_session) {
    if (was_explicitly_commanded_this_session) {
        *was_explicitly_commanded_this_session = remote_power_commanded_this_session;
    }
    return current_power_state;
}

// Renamed and potentially modified function
void tcp_client_apply_current_led_settings(void) {
    apply_led_state(); // Internally calls the static apply_led_state
}


static void tcp_client_task(void *pvParameters) {
  ESP_LOGI(TAG, "tcp_client_task started. Target: %s:%u", server_ip_str,
           server_port);
  char rx_buf[RECV_BUF_SIZE];
  while (1) {
    ESP_LOGI(TAG, "Attempting to create socket...");
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d (%s)", errno,
               strerror(errno));
      vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
      continue;
    }
    ESP_LOGI(TAG, "Socket created.");

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(server_ip_str);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server_port);

    ESP_LOGI(TAG, "Attempting to connect to %s:%d", server_ip_str, server_port);
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to connect: errno %d (%s)", errno,
               strerror(errno));
      close(sock);
      sock = -1;
      vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
      continue;
    }
    ESP_LOGI(TAG, "Successfully connected to %s:%d", server_ip_str,
             server_port);
    connected = true;
    remote_power_commanded_this_session = false; // Reset flag on new connection


    const char *hello_msg = "ESP32 Connected\n";
    if (send(sock, hello_msg, strlen(hello_msg), 0) < 0) {
      ESP_LOGE(TAG, "Failed to send hello message: errno %d (%s)", errno,
               strerror(errno));
    } else {
      ESP_LOGI(TAG, "Sent hello message to server.");
    }

    while (connected) {
      ESP_LOGD(TAG, "Waiting to receive data...");
      int len = recv(sock, rx_buf, RECV_BUF_SIZE - 1, 0);
      if (len < 0) {
        ESP_LOGE(TAG, "recv failed: errno %d (%s)", errno, strerror(errno));
        connected = false;
        break;
      } else if (len == 0) {
        ESP_LOGW(TAG, "Connection closed by server.");
        connected = false;
        break;
      } else {
        rx_buf[len] = 0;
        // 去掉末尾的换行或回车
        while (len > 0 &&
               (rx_buf[len - 1] == '\n' || rx_buf[len - 1] == '\r')) {
          rx_buf[--len] = 0;
        }
        ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buf);

        cJSON *root = cJSON_Parse(rx_buf);
        if (root) {
          cJSON *type_item = cJSON_GetObjectItem(root, "type");
          if (cJSON_IsString(type_item) && (type_item->valuestring != NULL)) {
            ESP_LOGI(TAG, "Command type: %s", type_item->valuestring);
            if (strcmp(type_item->valuestring, "power") == 0) {
              cJSON *state_item = cJSON_GetObjectItem(root, "state");
              if (cJSON_IsString(state_item) &&
                  state_item->valuestring != NULL) {
                if (strcmp(state_item->valuestring, "on") == 0) {
                  ESP_LOGI(TAG, "Turning ON LEDs via remote");
                  current_power_state = true;
                  remote_power_commanded_this_session = true;
                } else if (strcmp(state_item->valuestring, "off") == 0) {
                  ESP_LOGI(TAG, "Turning OFF LEDs via remote");
                  current_power_state = false;
                  remote_power_commanded_this_session = true;
                }
                apply_led_state();
              }
            } else if (strcmp(type_item->valuestring, "color") == 0) {
              cJSON *r_item = cJSON_GetObjectItem(root, "r");
              cJSON *g_item = cJSON_GetObjectItem(root, "g");
              cJSON *b_item = cJSON_GetObjectItem(root, "b");
              if (cJSON_IsNumber(r_item) && cJSON_IsNumber(g_item) &&
                  cJSON_IsNumber(b_item)) {
                current_r = r_item->valueint;
                current_g = g_item->valueint;
                current_b = b_item->valueint;
                ESP_LOGI(TAG, "Setting color to R:%d G:%d B:%d via remote", current_r,
                         current_g, current_b);
                // No longer setting remote_power_commanded_this_session = true here for color/brightness
                if (current_power_state)
                  apply_led_state(); // Apply only if power is on
              }
            } else if (strcmp(type_item->valuestring, "brightness") == 0) {
              cJSON *value_item = cJSON_GetObjectItem(root, "value");
              if (cJSON_IsNumber(value_item)) {
                // 使用 int 临时变量进行范围检查，然后再赋值给 uint8_t
                int val = value_item->valueint;
                if (val < 0) val = 0;
                if (val > 100) val = 100;
                current_brightness_percent = (uint8_t)val;
                ESP_LOGI(TAG, "Setting brightness to %d%% via remote",
                         current_brightness_percent);
                // No longer setting remote_power_commanded_this_session = true here for color/brightness
                if (current_power_state)
                  apply_led_state(); // Apply only if power is on
              }
            }
          } else {
            ESP_LOGW(TAG, "Received JSON does not have a 'type' string field "
                          "or it's null.");
          }
          cJSON_Delete(root);
        } else {
          ESP_LOGW(TAG,
                   "Failed to parse received data as JSON. Error before: [%s]",
                   cJSON_GetErrorPtr());
        }
      }
    }

    if (sock >= 0) {
      ESP_LOGI(TAG, "Closing socket.");
      close(sock);
      sock = -1;
    }
    connected = false;
    ESP_LOGI(TAG, "Disconnected. Reconnecting in %d ms...", RECONNECT_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
  }
  ESP_LOGE(TAG, "tcp_client_task unexpectedly exited its main loop.");
  vTaskDelete(NULL);
}

void tcp_client_start(const char *server_ip, uint16_t port) {
  if (!server_ip) {
    ESP_LOGE(TAG, "Server IP is NULL, cannot start TCP client.");
    return;
  }
  strncpy(server_ip_str, server_ip, sizeof(server_ip_str) - 1);
  server_ip_str[sizeof(server_ip_str) - 1] =
      '\0'; // Corrected: Use single quotes for char
  server_port = port;

  ESP_LOGI(TAG,
           "Attempting to create and start tcp_client_task to connect to %s:%u",
           server_ip_str, server_port);
  BaseType_t task_created =
      xTaskCreate(tcp_client_task, "tcp_client", 4096 * 2, NULL, 5, NULL);
  if (task_created == pdPASS) {
    ESP_LOGI(TAG, "tcp_client_task created successfully.");
  } else {
    ESP_LOGE(TAG, "Failed to create tcp_client_task. Error code: %ld",
             (long)task_created);
  }
}

bool tcp_client_send_data(const char *data, int len) {
  if (!connected || sock < 0) {
    ESP_LOGE(TAG, "Not connected or socket invalid, cannot send data.");
    return false;
  }
  if (!data || len <= 0) {
    ESP_LOGE(TAG, "Invalid data or length for sending.");
    return false;
  }

  ESP_LOGI(TAG, "Sending %d bytes: %s", len, data);
  int sent = send(sock, data, len, 0);
  if (sent < 0) {
    ESP_LOGE(TAG, "send failed: errno %d (%s)", errno, strerror(errno));
    connected = false;
    return false;
  }
  if (sent < len) {
    ESP_LOGW(TAG, "Sent only %d bytes out of %d", sent, len);
  }
  return sent == len;
}

bool tcp_client_is_connected(void) { return connected && (sock >= 0); }

// 外部接口：根据最近接收到的 remote 命令，应用当前 LED 状态
void tcp_client_apply_led_state(void) { apply_led_state(); }
