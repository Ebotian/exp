/* Wi-Fi STA Connect and Disconnect Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

*/
#include <WiFi.h>
const char *ssid = "Nicolette86132";  // 替换为您的 WiFi 名称
const char *password = "Yibotian123"; // 替换为您的 WiFi 密码

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("[WiFi] Connecting to SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Scanning for available networks...");
    int n = WiFi.scanNetworks();
    if (n == 0) {
      Serial.println("No networks found");
    } else {
      Serial.printf("%d networks found:\n", n);
      for (int i = 0; i < n; ++i) {
        Serial.printf("  %d: %s (RSSI: %d)%s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? " [open]" : "");
      }
    }
    WiFi.disconnect(); // 先断开
    delay(200);
    WiFi.begin(ssid, password);
    int tryCount = 0;
    while (WiFi.status() != WL_CONNECTED && tryCount < 20) { // 最多尝试10秒
      delay(500);
      Serial.print(".");
      tryCount++;
    }
    if (WiFi.status() == WL_CONNECTED) break;
    Serial.println("\n[WiFi] Retry connecting...");
  }
  Serial.println();
  Serial.println("[WiFi] WiFi is connected!");
  Serial.print("[WiFi] IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost! Trying to reconnect...");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Scanning for available networks...");
      int n = WiFi.scanNetworks();
      if (n == 0) {
        Serial.println("No networks found");
      } else {
        Serial.printf("%d networks found:\n", n);
        for (int i = 0; i < n; ++i) {
          Serial.printf("  %d: %s (RSSI: %d)%s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? " [open]" : "");
        }
      }
      WiFi.disconnect();
      delay(200);
      WiFi.begin(ssid, password);
      int tryCount = 0;
      while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
        delay(500);
        Serial.print(".");
        tryCount++;
      }
      if (WiFi.status() == WL_CONNECTED) break;
      Serial.println("\n[WiFi] Retry connecting...");
    }
    Serial.println();
    Serial.println("[WiFi] Reconnected!");
    Serial.print("[WiFi] IP address: ");
    Serial.println(WiFi.localIP());
  }
  delay(5000); // 每5秒检查一次
}
