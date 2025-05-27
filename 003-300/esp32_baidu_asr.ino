/*
  ESP32 录音0.5秒 + WiFi联网 + 百度ASR语音识别（极限内存版）
  - 自动WiFi扫描与重连
  - INMP441 I2S麦克风采集
  - 录音0.5秒，base64编码，POST到百度ASR
  - 串口输出识别结果
*/
#include "driver/i2s.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// WiFi参数
const char *ssid = "Nicolette86132";
const char *password = "Yibotian123";

// 百度ASR参数
const char *API_KEY = "VcCpPm5kV1g4OUSIVnb0y488";
const char *SECRET_KEY = "qdRiuLIeME3cLriGthtxIkqQblB5DskI";
const char *CUID = "esp32demo123456"; // 可用MAC或自定义
const int SAMPLE_RATE = 16000;
const int RECORD_SECONDS = 2; // 录音2秒
const int CHANNELS = 1;
const int PCM_BYTES = SAMPLE_RATE * RECORD_SECONDS * 2; // 16bit
uint8_t audio_buffer[PCM_BYTES];                        // 直接存小端字节

// I2S引脚
#define I2S_WS_PIN 25
#define I2S_SCK_PIN 26
#define I2S_SD_PIN 35
#define I2S_PORT I2S_NUM_0

// Base64编码表
const char b64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// base64编码函数，结果写入dst_buf，返回实际长度
typedef void (*base64_cb_t)(const char *b64, size_t len, void *user);
size_t base64_encode_to_buf(const uint8_t *data, size_t len, char *dst_buf) {
  int i = 0, j = 0;
  uint8_t arr3[3], arr4[4];
  size_t out_idx = 0;
  while (len--) {
    arr3[i++] = *(data++);
    if (i == 3) {
      arr4[0] = (arr3[0] & 0xfc) >> 2;
      arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
      arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
      arr4[3] = arr3[2] & 0x3f;
      for (i = 0; i < 4; i++)
        dst_buf[out_idx++] = b64_alphabet[arr4[i]];
      i = 0;
    }
  }
  if (i) {
    for (j = i; j < 3; j++)
      arr3[j] = 0;
    arr4[0] = (arr3[0] & 0xfc) >> 2;
    arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
    arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
    arr4[3] = arr3[2] & 0x3f;
    for (j = 0; j < i + 1; j++)
      dst_buf[out_idx++] = b64_alphabet[arr4[j]];
    while (i++ < 3)
      dst_buf[out_idx++] = '=';
  }
  dst_buf[out_idx] = 0;
  return out_idx;
}

// 获取百度Token
String get_baidu_token() {
  HTTPClient http;
  String url = "https://aip.baidubce.com/oauth/2.0/"
               "token?grant_type=client_credentials&client_id=" +
               String(API_KEY) + "&client_secret=" + String(SECRET_KEY);
  WiFiClientSecure client;
  client.setInsecure();
  if (!http.begin(client, url))
    return "";
  int code = http.POST("");
  if (code != 200) {
    http.end();
    return "";
  }
  String resp = http.getString();
  http.end();
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, resp) == DeserializationError::Ok &&
      doc["access_token"]) {
    return doc["access_token"].as<String>();
  }
  return "";
}

// WiFi连接
void wifi_connect() {
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Scanning...");
    WiFi.scanNetworks();
    WiFi.disconnect();
    delay(200);
    WiFi.begin(ssid, password);
    int tryCount = 0;
    while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
      delay(500);
      Serial.print(".");
      tryCount++;
    }
    if (WiFi.status() == WL_CONNECTED)
      break;
    Serial.println("\n[WiFi] Retry...");
  }
  Serial.println("\n[WiFi] Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// I2S初始化
void i2s_init() {
  i2s_config_t cfg = {.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                      .sample_rate = SAMPLE_RATE,
                      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
                      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
                      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
                      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                      .dma_buf_count = 4,
                      .dma_buf_len = 1024,
                      .use_apll = true,
                      .tx_desc_auto_clear = false,
                      .fixed_mclk = 0};
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_pin_config_t pin = {.bck_io_num = I2S_SCK_PIN,
                          .ws_io_num = I2S_WS_PIN,
                          .data_out_num = I2S_PIN_NO_CHANGE,
                          .data_in_num = I2S_SD_PIN};
  i2s_set_pin(I2S_PORT, &pin);
}

// 录音1秒
void record_audio() {
  size_t bytes_read = 0;
  int32_t raw[32]; // 小buffer
  size_t idx = 0;
  i2s_start(I2S_PORT);
  while (idx < PCM_BYTES) {
    size_t to_read = min(sizeof(raw), PCM_BYTES - idx);
    if (i2s_read(I2S_PORT, (void *)raw, to_read, &bytes_read, portMAX_DELAY) ==
            ESP_OK &&
        bytes_read > 0) {
      // 直接拷贝原始字节到audio_buffer
      size_t copy_len = min(bytes_read, PCM_BYTES - idx);
      memcpy(audio_buffer + idx, raw, copy_len);
      idx += copy_len;
    }
  }
  i2s_stop(I2S_PORT);
}

// 发送到百度ASR（裸WiFiClient分块POST，极限低内存）
void send_to_baidu_asr(const String &token) {
  // 1. 计算base64长度和Content-Length
  size_t b64_len = ((PCM_BYTES + 2) / 3) * 4;
  size_t json_head_len =
      snprintf(NULL, 0,
               "{\"format\":\"pcm\",\"rate\":%d,\"dev_pid\":1537,\"channel\":%"
               "d,\"cuid\":\"%s\","
               "\"token\":\"%s\",\"len\":%d,\"speech\":\"",
               SAMPLE_RATE, CHANNELS, CUID, token.c_str(), PCM_BYTES);
  size_t json_tail_len = 2; // "}"
  size_t content_len = json_head_len + b64_len + json_tail_len;

  char *json_head = (char *)malloc(json_head_len + 1);
  if (!json_head) {
    Serial.println("[ASR] json_head malloc fail");
    return;
  }
  snprintf(json_head, json_head_len + 1,
           "{\"format\":\"pcm\",\"rate\":%d,\"dev_pid\":1537,\"channel\":%d,"
           "\"cuid\":\"%s\","
           "\"token\":\"%s\",\"len\":%d,\"speech\":\"",
           SAMPLE_RATE, CHANNELS, CUID, token.c_str(), PCM_BYTES);

  WiFiClient client;
  const char *host = "vop.baidu.com";
  const int port = 80;
  if (!client.connect(host, port)) {
    Serial.println("[ASR] TCP connect fail");
    free(json_head);
    return;
  }
  // 2. 发送HTTP头
  client.printf("POST /server_api HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Content-Type: application/json\r\n"
                "Accept: application/json\r\n"
                "Content-Length: %u\r\n"
                "Connection: close\r\n\r\n",
                host, (unsigned)content_len);
  // 3. 发送JSON头
  client.write((const uint8_t *)json_head, json_head_len);
  free(json_head);
  // 4. 分块base64编码并发送
  const size_t CHUNK = 1024;
  char b64buf[CHUNK * 4 / 3 + 8];
  for (size_t i = 0; i < PCM_BYTES; i += CHUNK) {
    size_t chunk_len = (i + CHUNK <= PCM_BYTES) ? CHUNK : (PCM_BYTES - i);
    size_t b64out = base64_encode_to_buf(audio_buffer + i, chunk_len, b64buf);
    client.write((const uint8_t *)b64buf, b64out);
  }
  // 5. 发送JSON尾部
  client.write((const uint8_t *)"\"}", 2);
  client.flush();
  // 6. 读取响应
  String resp;
  unsigned long t_start = millis();
  while (client.connected() && millis() - t_start < 8000) {
    while (client.available()) {
      char c = client.read();
      resp += c;
    }
  }
  client.stop();
  // 7. 提取HTTP body
  int body_idx = resp.indexOf("\r\n\r\n");
  String body = (body_idx >= 0) ? resp.substring(body_idx + 4) : resp;
  Serial.println("[ASR] Response: " + body);
  // 解析识别结果
  DynamicJsonDocument doc2(1024);
  DeserializationError err = deserializeJson(doc2, body);
  if (!err && doc2["err_no"] == 0 && doc2["result"]) {
    Serial.print("[ASR] 识别文本: ");
    Serial.println(doc2["result"][0].as<String>());
  } else {
    Serial.print("[ASR] 识别失败: ");
    if (doc2["err_msg"])
      Serial.println(doc2["err_msg"].as<String>());
    else
      Serial.println("未知错误");
  }
}

String g_baidu_token;
unsigned long last_token_time = 0;
const unsigned long TOKEN_VALID_MS = 60 * 60 * 1000UL; // 1小时有效

void setup() {
  Serial.begin(115200);
  delay(1000);
  wifi_connect();
  i2s_init();
  g_baidu_token = get_baidu_token();
  last_token_time = millis();
  if (g_baidu_token.length() == 0) {
    Serial.println("[ERROR] 获取百度Token失败");
    while (1)
      delay(1000);
  }
  Serial.println("[INFO] Token获取成功");
}

void loop() {
  // Token定期刷新
  if (millis() - last_token_time > TOKEN_VALID_MS ||
      g_baidu_token.length() == 0) {
    Serial.println("[INFO] 刷新Token...");
    String t = get_baidu_token();
    if (t.length() > 0) {
      g_baidu_token = t;
      last_token_time = millis();
      Serial.println("[INFO] Token刷新成功");
    } else {
      Serial.println("[ERROR] Token刷新失败，重试...");
      delay(5000);
      return;
    }
  }
  Serial.println("[INFO] 开始录音...");
  record_audio();
  delay(10);
  Serial.println("[INFO] 录音完成，发送ASR...");
  send_to_baidu_asr(g_baidu_token);
  delay(8000); // 每8秒识别一次
}