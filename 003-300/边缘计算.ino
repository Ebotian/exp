// ESP32边缘计算原型：本地关键词检测+云端ASR混合（极简）
// 只识别“开灯”“关灯”命令，控制GPIO输出
#include "driver/i2s.h"
#include <WiFi.h>

// WiFi参数
const char *ssid = "Nicolette86132";
const char *password = "Yibotian123";

// 百度ASR参数
const char *API_KEY = "VcCpPm5kV1g4OUSIVnb0y488";
const char *SECRET_KEY = "qdRiuLIeME3cLriGthtxIkqQblB5DskI";
const char *CUID = "esp32demo123456";
const int SAMPLE_RATE = 16000;
const int RECORD_SECONDS = 2;
const int CHANNELS = 1;
const int PCM_BYTES = SAMPLE_RATE * RECORD_SECONDS * 2;
uint8_t audio_buffer[PCM_BYTES];

#define I2S_WS_PIN 25
#define I2S_SCK_PIN 26
#define I2S_SD_PIN 35
#define I2S_PORT I2S_NUM_0

#define PIN_LIGHT 2 // 用GPIO2控制灯

// ...base64编码、WiFi连接、I2S采集等函数同前...
// 这里只保留核心流程

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LIGHT, OUTPUT);
  digitalWrite(PIN_LIGHT, LOW);
  // ...WiFi连接、I2S初始化、Token获取...
}

// 简单本地能量门限，静音不上传
bool is_voice_detected() {
  uint32_t sum = 0;
  for (int i = 0; i < PCM_BYTES; i += 2) {
    int16_t sample = audio_buffer[i] | (audio_buffer[i + 1] << 8);
    sum += abs(sample);
  }
  return sum / (PCM_BYTES / 2) > 500;
}

// 解析ASR结果，控制GPIO
void handle_asr_result(const String &text) {
  if (text.indexOf("开灯") >= 0) {
    digitalWrite(PIN_LIGHT, HIGH);
    Serial.println("[ACTION] 开灯");
  } else if (text.indexOf("关灯") >= 0) {
    digitalWrite(PIN_LIGHT, LOW);
    Serial.println("[ACTION] 关灯");
  } else {
    Serial.println("[ACTION] 未识别命令");
  }
}

void loop() {
  // ...Token刷新逻辑...
  Serial.println("[INFO] 开始录音...");
  // ...录音到audio_buffer...
  if (!is_voice_detected()) {
    Serial.println("[INFO] 静音，跳过上传");
    delay(2000);
    return;
  }
  Serial.println("[INFO] 上传ASR...");
  String asr_text = ""; // ...调用send_to_baidu_asr并返回识别文本...
  handle_asr_result(asr_text);
  delay(8000);
}
// ...send_to_baidu_asr函数可用前述分块极限内存实现，返回识别文本...