#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  pinMode(PC13, OUTPUT); // 设置PC13为输出
}

void loop() {
  digitalWrite(PC13, LOW);  // 点亮LED（低电平点亮）
  delay(500);
  digitalWrite(PC13, HIGH); // 熄灭LED
  delay(500);// put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}