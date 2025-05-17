// filepath: /home/ebit/exp/002-400/esp32.ino
// ESP32 <-> BC260Y 通信连通性测试

#define BC260Y_RX 23  // ESP32接BC260Y TX
#define BC260Y_TX 22  // ESP32接BC260Y RX

HardwareSerial bcSerial(2); // 使用Serial2

unsigned long previousMillis = 0;
const unsigned long interval = 5000; // 每隔5秒发送一次AT命令

void setup() {
  Serial.begin(115200);      // 调试串口
  bcSerial.begin(9600, SERIAL_8N1, BC260Y_RX, BC260Y_TX); // BC260Y串口
  delay(1000);
  Serial.println("ESP32 <-> BC260Y 连通性测试开始");

  // 初次发送AT指令
  bcSerial.print("AT\r\n");
}

void loop() {
    //Serial.println("Looping..."); // 新增这行
  // 定时反复发送AT指令
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    Serial.println("发送AT指令...");
    bcSerial.print("AT\r\n");
  }

  // 打印BC260Y返回内容到串口监视器
  if (bcSerial.available()) {
    String resp = bcSerial.readStringUntil('\n');
    Serial.print("BC260Y返回: ");
    Serial.println(resp);
  }
}