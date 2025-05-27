#include "driver/i2s.h"

// I2S 引脚定义
#define I2S_WS_PIN 25  // Word Select (LRCLK)
#define I2S_SCK_PIN 26 // Serial Clock (BCLK)
#define I2S_SD_PIN 35  // Serial Data (DIN)

// I2S 端口号
#define I2S_PORT_NUMBER I2S_NUM_0

// I2S 采样参数
#define I2S_SAMPLE_RATE (16000) // 采样率，例如 16kHz
#define I2S_BITS_PER_SAMPLE                                                    \
  I2S_BITS_PER_SAMPLE_32BIT // INMP441 是 24 位，通常读为 32 位 (高位对齐)

// 读取缓冲区大小 (读取多少个采样点)
#define I2S_READ_BUFFER_SAMPLE_COUNT 64

void setup() {
  Serial.begin(115200);
  Serial.println("INMP441 I2S Test");

  // 配置 I2S
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // 主机接收模式
      .sample_rate = I2S_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // L/R 接地，数据在左声道
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // 中断分配标志
      .dma_buf_count = 4,                       // DMA 缓冲区数量
      .dma_buf_len = 1024,                      // 每个 DMA 缓冲区的采样点数
      .use_apll = true,            // 使用 APLL 时钟以获得更精确的采样率
      .tx_desc_auto_clear = false, // 不用于 RX 模式
      .fixed_mclk = 0              // 不用于 RX 模式
  };

  // 安装 I2S 驱动
  esp_err_t err = i2s_driver_install(I2S_PORT_NUMBER, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    while (1)
      ;
  }

  // 设置 I2S 引脚
  i2s_pin_config_t pin_config = {.bck_io_num = I2S_SCK_PIN,
                                 .ws_io_num = I2S_WS_PIN,
                                 .data_out_num =
                                     I2S_PIN_NO_CHANGE, // 不用于 RX 模式
                                 .data_in_num = I2S_SD_PIN};

  err = i2s_set_pin(I2S_PORT_NUMBER, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    while (1)
      ;
  }
  Serial.println("I2S driver installed and pins configured.");
}

void loop() {
  // 创建一个缓冲区来存储读取的采样数据
  // INMP441 是 24 位数据，通常在 32 位采样中左对齐 (高 24 位是有效数据)
  int32_t samples[I2S_READ_BUFFER_SAMPLE_COUNT];
  size_t bytes_read = 0;

  // 从 I2S 读取数据
  esp_err_t result = i2s_read(I2S_PORT_NUMBER, (void *)samples,
                              I2S_READ_BUFFER_SAMPLE_COUNT *
                                  sizeof(int32_t), // 缓冲区大小 (字节)
                              &bytes_read,
                              portMAX_DELAY); // 阻塞直到读取到数据

  if (result == ESP_OK && bytes_read > 0) {
    int samples_read = bytes_read / sizeof(int32_t);
    Serial.print("Read ");
    Serial.print(samples_read);
    Serial.print(" samples. First sample: ");
    // INMP441 的数据是 24 位的，在 32 位整数的高位。
    // 直接打印原始 32 位值可以观察变化。
    // 如果需要实际的 24 位值，可以右移 8 位: samples[0] >> 8
    Serial.println(samples[0]);

    // 你可以在这里添加更复杂的处理，例如计算 RMS 值来表示音量大小
    long total = 0;
    for (int i = 0; i < samples_read; i++) {
      total += abs(samples[i] >> 8); // 使用移位后的24位值
    }
    Serial.print("Average Magnitude: ");
    Serial.println(total / samples_read);

  } else {
    Serial.printf("Failed to read from I2S: %d\n", result);
  }

  delay(100); // 稍微延迟一下，避免串口输出过快
}