#include "display.h"
#include "ssd1306.h"
#include <string.h>

static SSD1306_t dev;

void display_init(void) {
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&dev, 128, 64); // 128x64 屏幕
    ssd1306_clear_screen(&dev, 0x00);
    ssd1306_display_text(&dev, 0, "OLED Ready", 10, false);
}

void display_show_text(const char *text) {
    ssd1306_clear_screen(&dev, 0x00);
    ssd1306_display_text(&dev, 0, text, strlen(text), false);
}

void display_show_2lines(const char *line1, const char *line2) {
    ssd1306_clear_screen(&dev, 0x00);
    ssd1306_display_text(&dev, 0, line1, strlen(line1), false);
    ssd1306_display_text(&dev, 2, line2, strlen(line2), false); // 第二行，y=2
}
