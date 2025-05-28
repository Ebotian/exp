#include "pir.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define PIR_GPIO_PIN GPIO_NUM_23 // 根据实际硬件连接修改
static const char *TAG = "pir";

void pir_init(void) {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = 1ULL << PIR_GPIO_PIN;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
  ESP_LOGI(TAG, "PIR initialized on GPIO %d", PIR_GPIO_PIN);
}

bool pir_detected(void) {
  int level = gpio_get_level(PIR_GPIO_PIN);
  return level == 1;
}
