#include "webserver.h"
#include "ws2812.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <stdint.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "webserver";

static esp_err_t led_control_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    // 使用 cJSON 解析 JSON
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON *on_item = cJSON_GetObjectItem(root, "on");
    cJSON *r_item = cJSON_GetObjectItem(root, "r");
    cJSON *g_item = cJSON_GetObjectItem(root, "g");
    cJSON *b_item = cJSON_GetObjectItem(root, "b");
    cJSON *brightness_item = cJSON_GetObjectItem(root, "brightness");
    bool on = cJSON_IsBool(on_item) ? cJSON_IsTrue(on_item) : (cJSON_IsNumber(on_item) && on_item->valueint);
    int r = cJSON_IsNumber(r_item) ? r_item->valueint : 255;
    int g = cJSON_IsNumber(g_item) ? g_item->valueint : 255;
    int b = cJSON_IsNumber(b_item) ? b_item->valueint : 255;
    int brightness = cJSON_IsNumber(brightness_item) ? brightness_item->valueint : 255;
    // 参数范围校验
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    if (on) {
        for (int i = 0; i < 30; ++i) ws2812_set_pixel(i, r * brightness / 255, g * brightness / 255, b * brightness / 255);
        ws2812_show();
    } else {
        for (int i = 0; i < 30; ++i) ws2812_set_pixel(i, 0, 0, 0);
        ws2812_show();
    }
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"result\":true}");
    return ESP_OK;
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t led_control_uri = {
            .uri = "/api/led",
            .method = HTTP_POST,
            .handler = led_control_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &led_control_uri);
        ESP_LOGI(TAG, "Webserver started, POST /api/led for LED control");
    }
}
