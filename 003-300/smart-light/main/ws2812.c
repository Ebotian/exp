#include "ws2812.h"
#include <string.h>
#include <stdint.h>
#include "led_strip.h"
#include "led_strip_types.h"
#include "led_strip_rmt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WS2812_LED_NUM 30
#define WS2812_GPIO    18

static led_strip_handle_t led_strip = NULL;

void ws2812_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO,
        .max_leds = WS2812_LED_NUM,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = 0}
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 0,
        .flags = {.with_dma = 0}
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    led_strip_clear(led_strip);
}

void ws2812_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if (led_strip && idx < WS2812_LED_NUM) {
        led_strip_set_pixel(led_strip, idx, r, g, b);
    }
}

void ws2812_show(void) {
    if (led_strip) {
        led_strip_refresh(led_strip);
    }
}

void ws2812_running_light_task(void *pvParameter) {
    ws2812_init();
    int pos = 0;
    while (1) {
        for (int i = 0; i < WS2812_LED_NUM; ++i) {
            ws2812_set_pixel(i, 0, 0, 0);
        }
        ws2812_set_pixel(pos, 255, 0, 0);
        ws2812_show();
        pos = (pos + 1) % WS2812_LED_NUM;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
