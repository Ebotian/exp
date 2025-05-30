#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>

void ws2812_init(void);
void ws2812_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);
void ws2812_show(void);
void ws2812_running_light_task(void *pvParameter);

#endif // WS2812_H
