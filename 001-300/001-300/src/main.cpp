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

const int AUTO_LEFT_STEPS = 120;   // 自动模式左转步数
const int AUTO_RIGHT_STEPS = 120;  // 自动模式右转步数
const int AUTO_STEP_DELAY = 3;    // 自动模式步进延时(ms)
const int AUTO_LOOP_DELAY = 3;   // 自动模式每步间隔(ms)

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
      for (int i = 0; i < 10; i++) {
        stepLeft(8, 3);
      }
  } else if (digitalRead(BTN2) == LOW) {
      for (int i = 0; i < 10; i++) {
        stepRight(8, 3);
      }
  }

  // 串口控制
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    Serial.print("Received: ");
    Serial.println(cmd); // 调试用
    if (cmd == "LEFT") {
      for (int i = 0; i < 10; i++) {
        stepLeft(8, 3);
      }
    } else if (cmd == "RIGHT") {
      for (int i = 0; i < 10; i++) {
        stepRight(8, 3);
      }
    } else if (cmd == "STOP") {
      stopMotor();
      autoMode = false;
    } else if (cmd == "AUTO") {
      autoMode = true;
    }
  }

  // 自动模式
  static int autoDir = 1; // 1: 左转, -1: 右转
  static int autoStepSum = 0;
  static unsigned long lastAutoStepTime = 0;
  if (autoMode) {
    unsigned long now = millis();
    if (now - lastAutoStepTime >= 1000) { // 每10秒
      if (autoDir == 1) {
      for (int i = 0; i < 90; i++) {
        stepLeft(8, 3);
      }
      } else {
      for (int i = 0; i < 90; i++) {
        stepRight(8, 3);
      }
      }
      autoStepSum += 30;
      lastAutoStepTime = now;
      if (autoStepSum >= 360) {
        autoDir = -autoDir; // 换方向
        autoStepSum = 0;
      }
    }
    // 检查是否收到STOP指令
    if (!autoMode) {
      stopMotor();
      autoStepSum = 0;
      autoDir = 1;
    }
  }
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}