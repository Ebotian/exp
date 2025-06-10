/*
 * STM32G431C8T6 TDM Signal Debug Monitor
 *
 * Simple signal analyzer to understand the actual TDM signal characteristics
 */

#include <Arduino.h>

#define PIN_TDM_IN    PB0  // TDM signal input
HardwareSerial Serial2(PA3, PA2); // RX, TX
void setup() {
    pinMode(PIN_TDM_IN, INPUT);
    Serial2.begin(115200);
    delay(1000);
    Serial2.println("TDM Signal Debug Monitor");
    Serial2.println("Format: Level Duration(us)");
}

void loop() {
    static uint8_t last_level = 255;  // Invalid initial value
    static uint32_t last_time = 0;

    uint8_t current_level = digitalRead(PIN_TDM_IN);
    uint32_t current_time = micros();

    if (last_level == 255) {
        // First reading
        last_level = current_level;
        last_time = current_time;
        return;
    }

    if (current_level != last_level) {
        // Level transition detected
        uint32_t duration = current_time - last_time;

        Serial2.print(last_level ? "H" : "L");
        Serial2.print(" ");
        Serial2.println(duration);

        last_level = current_level;
        last_time = current_time;
    }

    delayMicroseconds(10);
}
