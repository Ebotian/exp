// filepath: /home/user/stm32_projects/pb1_test/src/main.cpp
#include <Arduino.h>

const int ICG_TEST_PIN = PB1; // The pin you are using for ICG

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Wait for serial connection
  pinMode(ICG_TEST_PIN, OUTPUT);
  Serial.println("PB1 (ICG_PIN) Test Started. Pin will toggle every 500ms.");
}

void loop() {
  digitalWrite(ICG_TEST_PIN, HIGH);
  Serial.println("PB1 HIGH");
  delay(500); // Keep it HIGH for 500ms

  digitalWrite(ICG_TEST_PIN, LOW);
  Serial.println("PB1 LOW");
  delay(500); // Keep it LOW for 500ms
}