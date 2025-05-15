#include <Arduino.h>

// 步进电机控制引脚
#define IN1 PB5   // 橙
#define IN2 PA12  // 黄
#define IN3 PA11  // 粉
#define IN4 PB15  // 蓝

// 按钮引脚
#define BTN1 PB12 // 左转
#define BTN2 PB13 // 右转

// 步进电机步进序列（8步半步序列，适配ULN2003）
const int stepCount = 8;
const int stepSequence[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

int currentStep = 0;
bool autoMode = false;
unsigned long lastStepTime = 0;
const unsigned long autoStepInterval = 10; // 自动模式步进间隔(ms)

void setStep(int step) {
  digitalWrite(IN1, stepSequence[step][0]);
  digitalWrite(IN2, stepSequence[step][1]);
  digitalWrite(IN3, stepSequence[step][2]);
  digitalWrite(IN4, stepSequence[step][3]);
}

void stepLeft(int steps = 1, int delayMs = 5) {
  for (int i = 0; i < steps; i++) {
    currentStep = (currentStep + 1) % stepCount;
    setStep(currentStep);
    delay(delayMs);
  }
}

void stepRight(int steps = 1, int delayMs = 5) {
  for (int i = 0; i < steps; i++) {
    currentStep = (currentStep - 1 + stepCount) % stepCount;
    setStep(currentStep);
    delay(delayMs);
  }
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  pinMode(PC13, OUTPUT); // 设置PC13为输出

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  Serial.begin(115200);
  stopMotor();
}

void loop() {
  digitalWrite(PC13, LOW);  // 点亮LED（低电平点亮）
  delay(500);
  digitalWrite(PC13, HIGH); // 熄灭LED
  delay(500);// put your main code here, to run repeatedly:

  // 按钮控制
  if (digitalRead(BTN1) == LOW) {
    stepLeft(8, 5); // 加速
  } else if (digitalRead(BTN2) == LOW) {
    stepRight(8, 5);
  }

  // 串口控制
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "LEFT") {
      stepLeft(8, 5);
    } else if (cmd == "RIGHT") {
      stepRight(8, 5);
    } else if (cmd == "STOP") {
      stopMotor();
      autoMode = false;
    } else if (cmd == "AUTO") {
      autoMode = true;
    }
  }

  // 自动模式
  if (autoMode && millis() - lastStepTime > autoStepInterval) {
    stepLeft(1, 5);
    lastStepTime = millis();
  }
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}