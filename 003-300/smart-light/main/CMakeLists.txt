idf_component_register(
    SRCS "wifi.c" "pir.c" "ldr.c" "ws2812.c" "display.c" "voice.c" "webserver.c" "tcp_client.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES ssd1306 led_strip
    PRIV_REQUIRES esp_event driver esp_netif esp_wifi nvs_flash esp_adc esp_http_server json
)