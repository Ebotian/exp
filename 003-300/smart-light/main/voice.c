#include "voice.h"
#include "ws2812.h"
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/uart.h"
#include <stdio.h>
#include <math.h>

#define I2S_WS_PIN 25
#define I2S_SCK_PIN 26
#define I2S_SD_PIN 35
#define I2S_PORT_NUMBER 0
#define I2S_SAMPLE_RATE (16000)
#define I2S_BITS_PER_SAMPLE I2S_DATA_BIT_WIDTH_32BIT
#define I2S_READ_BUFFER_SAMPLE_COUNT 64

static bool ws2812_on = false;
static i2s_chan_handle_t rx_handle = NULL;

void voice_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_SCK_PIN, I2S_PORT_NUMBER);
    chan_cfg.id = I2S_PORT_NUMBER;
    chan_cfg.role = I2S_ROLE_MASTER;
    i2s_chan_handle_t tx_handle = NULL;
    i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle); // 正确创建RX通道

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = I2S_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_BITS_PER_SAMPLE,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_SD_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);
    // UART0 init for serial command input
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void ws2812_all_on(void) {
    for (int i = 0; i < 30; ++i) ws2812_set_pixel(i, 255, 255, 255);
    ws2812_show();
    ws2812_on = true;
}
void ws2812_all_off(void) {
    for (int i = 0; i < 30; ++i) ws2812_set_pixel(i, 0, 0, 0);
    ws2812_show();
    ws2812_on = false;
}

static bool contains_keyword(const char *text, const char *keyword) {
    return strstr(text, keyword) != NULL;
}

void voice_task(void *pvParameter) {
    voice_init();
    int32_t samples[I2S_READ_BUFFER_SAMPLE_COUNT];
    size_t bytes_read = 0;
    char recognized[64];
    uint8_t data;
    while (1) {
        esp_err_t result = i2s_channel_read(rx_handle, (void *)samples,
                                    I2S_READ_BUFFER_SAMPLE_COUNT * sizeof(int32_t),
                                    &bytes_read, portMAX_DELAY);
        if (result == ESP_OK && bytes_read > 0) {
            int len = uart_read_bytes(UART_NUM_0, &data, 1, 0); // non-blocking
            if (len > 0) {
                if (data == '1') {
                    strcpy(recognized, "开灯");
                } else if (data == '0') {
                    strcpy(recognized, "关灯");
                } else {
                    recognized[0] = '\0';
                }
                if (contains_keyword(recognized, "开灯")) {
                    ws2812_all_on();
                    printf("[VOICE] 识别到‘开灯’，灯带已点亮\n");
                } else if (contains_keyword(recognized, "关灯")) {
                    ws2812_all_off();
                    printf("[VOICE] 识别到‘关灯’，灯带已关闭\n");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
