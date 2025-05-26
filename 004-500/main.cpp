#include <Arduino.h>

// --- 引脚定义 (Blue Pill) ---
// 主时钟 (CLK / M) 输出引脚。选择具有定时器PWM功能的引脚。
// PA8 (TIM1_CH1), PA6 (TIM3_CH1), PA7 (TIM3_CH2), PB0 (TIM3_CH3), PB1 (TIM3_CH4)
// PA0 (TIM2_CH1_ETR), PA1 (TIM2_CH2) 等等
// 我们使用PA8作为TIM1_CH1的例子
const int MASTER_CLOCK_PIN = PA8;  // 从PinName改为int
// 移位门 (SH) 输出引脚
const int SH_PIN = PB0; // 例如: PB0
// 积分清零门 (ICG) 输出引脚
const int ICG_PIN = PB1; // 例如: PB1
// ADC输入引脚 (OS - Output Signal)
const int ADC_PIN = PA0; // 例如: PA0 (ADC1_IN0)

// --- TCD1304 时序常数 (参考数据手册) ---
// 主时钟 (M)
const float F_MASTER_CLOCK_HZ = 2000000.0f; // 2.0 MHz (VDD >= 4.0V时的典型值)
const float MASTER_CLOCK_DUTY_CYCLE = 50.0f;  // 50%

// 积分时间 (tINT) - 用户定义
uint32_t T_INT_MS = 10; // 10毫秒 (可动态调整)

// t2: 从ICG上升沿到SH上升沿的延时
const uint32_t T2_ICG_SH_DELAY_NS = 500; // 500 ns (典型值)

// t3: SH脉冲宽度
const uint32_t T3_SH_PULSE_WIDTH_NS = 1000; // 1000 ns = 1 µs (最小值)

// 数据读出时间
const uint32_t NUM_TOTAL_PIXELS_READOUT = 3694; // 总共要读出的元素数
const float F_DATA_RATE_HZ = 500000.0f;        // 0.5 MHz 数据速率 (典型值)
const float T_DATA_PERIOD_US = (1.0f / F_DATA_RATE_HZ) * 1000000.0f; // 一个像素数据的周期
const uint32_t T_READOUT_US = (uint32_t)(NUM_TOTAL_PIXELS_READOUT * T_DATA_PERIOD_US); // 例如: 3694 * 2µs = 7388 µs

// ADC相关变量
const uint32_t NUM_VALID_PIXELS = 3648; // 有效像素数 (从数据手册获取)
uint16_t pixelData[NUM_VALID_PIXELS]; // 存储像素数据的数组
volatile bool dataReady = false; // 数据准备标志

// 控制变量
bool autoMode = true;        // 自动模式标志
bool continuousMode = false; // 连续采集模式
uint32_t frameCounter = 0;   // 帧计数器
String outputFormat = "RAW"; // 输出格式: FULL, RAW, CSV, BINARY

HardwareTimer *MasterClockTimer;
HardwareTimer *AdcTimer; // ADC采样定时器

// DWT (数据观察点和跟踪) 用于精确的纳秒延时
#if (__CORTEX_M > 0U) && defined(ARDUINO_ARCH_STM32)
  #define DWT_CONTROL             (*(volatile uint32_t *)0xE0001000)
  #define DWT_CYCCNT              (*(volatile uint32_t *)0xE0001004)
  #define SCB_DEMCR               (*(volatile uint32_t *)0xE000EDFC)

  void DWT_Init(void) {
    SCB_DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // 启用TRC
    DWT_CYCCNT = 0;                          // 重置周期计数器
    DWT_CONTROL |= DWT_CTRL_CYCCNTENA_Msk;   // 启用CYCCNT
  }

  // 延时指定的系统时钟周期数
  inline void delay_cycles(uint32_t cycles) {
    uint32_t start_cycle = DWT_CYCCNT;
    while ((DWT_CYCCNT - start_cycle) < cycles);
  }

  // 纳秒延时
  void delay_ns(uint32_t ns) {
    if (ns == 0) return;
    // 计算周期数，确保SystemCoreClock被STM32Duino正确定义
    uint32_t cycles = (uint32_t)(((uint64_t)ns * SystemCoreClock) / 1000000000ULL);
    if (cycles == 0 && ns > 0) cycles = 1; // 确保至少有一个小延时
    delay_cycles(cycles);
  }
#else
  // 如果DWT不可用的后备方案 (对ns精度较低)
  void DWT_Init(void) { /* 什么都不做 */ }
  void delay_ns(uint32_t ns) {
    if (ns == 0) return;
    if (ns < 1000) { // 对短ns延时的粗略近似
        for (volatile uint32_t i = 0; i < (ns / 100); ++i) { __NOP(); } // 根据时钟调整除数
    } else {
        delayMicroseconds(ns / 1000);
    }
  }
#endif

// ADC定时器中断回调函数
void adcTimerCallback() {
  static uint32_t pixelIndex = 0;
  static uint32_t dummyCount = 0;
  static bool readoutStarted = false;

  if (!readoutStarted) {
    readoutStarted = true;
    pixelIndex = 0;
    dummyCount = 0;
    dataReady = false;
  }

  // 读取ADC值
  uint16_t adcValue = analogRead(ADC_PIN);

  // 跳过虚拟像素 (通常前几个和后几个像素是虚拟的)
  const uint32_t DUMMY_PIXELS_START = 32; // 前32个虚拟像素
  const uint32_t DUMMY_PIXELS_END = 14;   // 后14个虚拟像素

  if (dummyCount >= DUMMY_PIXELS_START && pixelIndex < NUM_VALID_PIXELS) {
    pixelData[pixelIndex] = adcValue;
    pixelIndex++;
  }

  dummyCount++;

  // 检查是否读取完所有数据
  if (dummyCount >= NUM_TOTAL_PIXELS_READOUT) {
    AdcTimer->pause(); // 停止ADC定时器
    readoutStarted = false;
    if (pixelIndex >= NUM_VALID_PIXELS) {
      dataReady = true; // 设置数据准备标志
    }
  }
}

void setup_master_clock_pwm(int pwm_pin, float frequency_hz, float duty_cycle_percent) {
  // 将引脚号转换为PinName供内部使用
  PinName pin_name = digitalPinToPinName(pwm_pin);

  TIM_TypeDef *Instance = (TIM_TypeDef *)pinmap_peripheral(pin_name, PinMap_PWM);
  if (Instance == nullptr) {
    Serial.println("ERROR: PWM引脚无效或未找到主时钟定时器");
    while(1);
  }
  uint32_t channel = STM_PIN_CHANNEL(pinmap_function(pin_name, PinMap_PWM));
  if (channel == 0) {
     Serial.println("ERROR: 无法找到主时钟引脚的PWM通道");
     while(1);
  }

  MasterClockTimer = new HardwareTimer(Instance);
  MasterClockTimer->setMode(channel, TIMER_OUTPUT_COMPARE_PWM1, pin_name);
  MasterClockTimer->setPrescaleFactor(1); // 如需要可调整极高/极低频率
  MasterClockTimer->setOverflow(frequency_hz, HERTZ_FORMAT);

  // 手动计算占空比值，因为PERCENT_FORMAT不可用
  uint32_t period = MasterClockTimer->getOverflow();
  uint32_t compare_value = (uint32_t)((duty_cycle_percent / 100.0f) * period);
  MasterClockTimer->setCaptureCompare(channel, compare_value);

  MasterClockTimer->resume();
  Serial.print("INFO: 主时钟PWM初始化完成 引脚:");
  Serial.print(pwm_pin);
  Serial.print(" 频率:");
  Serial.print(frequency_hz / 1000000.0, 2);
  Serial.println("MHz");
}

void setup_adc_timer() {
  // 使用TIM2设置ADC采样定时器
  AdcTimer = new HardwareTimer(TIM2);
  AdcTimer->setOverflow(F_DATA_RATE_HZ, HERTZ_FORMAT); // 设置采样频率
  AdcTimer->attachInterrupt(adcTimerCallback); // 附加中断回调

  Serial.print("INFO: ADC定时器初始化完成 采样频率:");
  Serial.print(F_DATA_RATE_HZ / 1000.0, 1);
  Serial.println("kHz");
}

void start_adc_readout() {
  // 重置ADC定时器并开始采样
  AdcTimer->refresh();
  AdcTimer->resume();
}

// 完整格式输出（包含统计信息）
void print_pixel_data_full() {
  if (!dataReady) return;

  Serial.println("=== FRAME_START ===");
  Serial.print("FRAME_ID:");
  Serial.println(frameCounter);
  Serial.print("TIMESTAMP:");
  Serial.println(millis());
  Serial.print("INTEGRATION_TIME:");
  Serial.print(T_INT_MS);
  Serial.println("ms");
  Serial.print("PIXEL_COUNT:");
  Serial.println(NUM_VALID_PIXELS);

  // 输出所有像素数据，每行10个数据
  Serial.println("--- PIXEL_DATA ---");
  for (uint32_t i = 0; i < NUM_VALID_PIXELS; i++) {
    Serial.print(pixelData[i]);
    if ((i + 1) % 10 == 0) {
      Serial.println(); // 每10个数据换行
    } else {
      Serial.print("\t"); // 使用制表符分隔
    }
  }

  // 如果最后一行不足10个数据，添加换行
  if (NUM_VALID_PIXELS % 10 != 0) {
    Serial.println();
  }

  // 计算统计信息
  uint32_t sum = 0;
  uint16_t min_val = pixelData[0];
  uint16_t max_val = pixelData[0];

  for (uint32_t i = 0; i < NUM_VALID_PIXELS; i++) {
    sum += pixelData[i];
    if (pixelData[i] < min_val) min_val = pixelData[i];
    if (pixelData[i] > max_val) max_val = pixelData[i];
  }

  float average = (float)sum / NUM_VALID_PIXELS;

  Serial.println("--- STATISTICS ---");
  Serial.print("AVERAGE:");
  Serial.println(average, 2);
  Serial.print("MIN:");
  Serial.println(min_val);
  Serial.print("MAX:");
  Serial.println(max_val);
  Serial.print("RANGE:");
  Serial.println(max_val - min_val);

  Serial.println("=== FRAME_END ===");
  Serial.println();

  dataReady = false; // 重置标志
}

// 原始数据输出（仅数值）
void print_pixel_data_raw() {
  if (!dataReady) return;

  Serial.print("FRAME_ID:");
  Serial.println(frameCounter);
  Serial.println("RAW_DATA_START");
  for (uint32_t i = 0; i < NUM_VALID_PIXELS; i++) {
    Serial.println(pixelData[i]);
  }
  Serial.println("RAW_DATA_END");

  dataReady = false; // 重置标志
}

// CSV格式输出
void print_pixel_data_csv() {
  if (!dataReady) return;

  Serial.print("FRAME_ID:");
  Serial.println(frameCounter);
  Serial.print("TIMESTAMP:");
  Serial.println(millis());
  Serial.println("PIXEL_INDEX,ADC_VALUE");
  for (uint32_t i = 0; i < NUM_VALID_PIXELS; i++) {
    Serial.print(i);
    Serial.print(",");
    Serial.println(pixelData[i]);
  }
  Serial.println("CSV_END");

  dataReady = false; // 重置标志
}

// 二进制格式输出（紧凑）
void print_pixel_data_binary() {
  if (!dataReady) return;

  Serial.println("BINARY_DATA_START");
  Serial.print("FRAME_ID:");
  Serial.println(frameCounter);
  Serial.print("LENGTH:");
  Serial.println(NUM_VALID_PIXELS * 2); // 每个像素2字节

  // 输出二进制数据的十六进制表示
  for (uint32_t i = 0; i < NUM_VALID_PIXELS; i++) {
    if (pixelData[i] < 0x1000) Serial.print("0");
    if (pixelData[i] < 0x100) Serial.print("0");
    if (pixelData[i] < 0x10) Serial.print("0");
    Serial.print(pixelData[i], HEX);
    if ((i + 1) % 16 == 0) {
      Serial.println();
    }
  }
  if (NUM_VALID_PIXELS % 16 != 0) {
    Serial.println();
  }
  Serial.println("BINARY_DATA_END");

  dataReady = false; // 重置标志
}

// 处理串口命令
void processSerialCommand() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();

    if (command == "HELP" || command == "?") {
      Serial.println("=== TCD1304 控制命令 ===");
      Serial.println("HELP/?       - 显示帮助信息");
      Serial.println("START        - 开始单次采集");
      Serial.println("STOP         - 停止采集");
      Serial.println("CONTINUOUS   - 切换连续采集模式");
      Serial.println("AUTO         - 切换自动模式");
      Serial.println("STATUS       - 显示当前状态");
      Serial.println("SET_INT <ms> - 设置积分时间(毫秒)");
      Serial.println("FORMAT <type>- 设置输出格式(FULL/RAW/CSV/BINARY)");
      Serial.println("GET_DATA     - 获取当前数据");
      Serial.println("RESET        - 重置设备");

    } else if (command == "START") {
      autoMode = false;
      Serial.println("INFO: 开始单次采集");

    } else if (command == "STOP") {
      autoMode = false;
      continuousMode = false;
      Serial.println("INFO: 停止采集");

    } else if (command == "CONTINUOUS") {
      continuousMode = !continuousMode;
      autoMode = continuousMode;
      Serial.print("INFO: 连续模式 ");
      Serial.println(continuousMode ? "开启" : "关闭");

    } else if (command == "AUTO") {
      autoMode = !autoMode;
      Serial.print("INFO: 自动模式 ");
      Serial.println(autoMode ? "开启" : "关闭");

    } else if (command == "STATUS") {
      Serial.println("=== 设备状态 ===");
      Serial.print("自动模式: ");
      Serial.println(autoMode ? "开启" : "关闭");
      Serial.print("连续模式: ");
      Serial.println(continuousMode ? "开启" : "关闭");
      Serial.print("积分时间: ");
      Serial.print(T_INT_MS);
      Serial.println("ms");
      Serial.print("输出格式: ");
      Serial.println(outputFormat);
      Serial.print("帧计数: ");
      Serial.println(frameCounter);
      Serial.print("数据就绪: ");
      Serial.println(dataReady ? "是" : "否");

    } else if (command.startsWith("SET_INT ")) {
      int newIntTime = command.substring(8).toInt();
      if (newIntTime > 0 && newIntTime <= 10000) {
        T_INT_MS = newIntTime;
        Serial.print("INFO: 积分时间设置为 ");
        Serial.print(T_INT_MS);
        Serial.println("ms");
      } else {
        Serial.println("ERROR: 积分时间范围 1-10000ms");
      }

    } else if (command.startsWith("FORMAT ")) {
      String format = command.substring(7);
      if (format == "FULL" || format == "RAW" || format == "CSV" || format == "BINARY") {
        outputFormat = format;
        Serial.print("INFO: 输出格式设置为 ");
        Serial.println(outputFormat);
      } else {
        Serial.println("ERROR: 支持的格式: FULL, RAW, CSV, BINARY");
      }

    } else if (command == "GET_DATA") {
      if (dataReady) {
        Serial.println("INFO: 输出当前数据");
        // 根据格式输出数据
        if (outputFormat == "RAW") {
          print_pixel_data_raw();
        } else if (outputFormat == "CSV") {
          print_pixel_data_csv();
        } else if (outputFormat == "BINARY") {
          print_pixel_data_binary();
        } else {
          print_pixel_data_full();
        }
      } else {
        Serial.println("INFO: 无可用数据");
      }

    } else if (command == "RESET") {
      Serial.println("INFO: 重置设备");
      frameCounter = 0;
      dataReady = false;
      autoMode = true;
      continuousMode = false;
      outputFormat = "FULL";
      T_INT_MS = 10;

    } else {
      Serial.print("ERROR: 未知命令 '");
      Serial.print(command);
      Serial.println("' 输入HELP查看帮助");
    }
  }
}

// 执行一次CCD采集循环
void performCCDCapture() {
  // --- 开始积分 ---
  digitalWrite(ICG_PIN, LOW); // 1. ICG低电平: 开始积分
  delay(T_INT_MS);            // 2. 等待tINT (积分时间)

  // --- 结束积分和电荷转移 ---
  digitalWrite(ICG_PIN, HIGH); // 3. ICG高电平: 结束积分
  delay_ns(T2_ICG_SH_DELAY_NS); // 4. 等待t2

  digitalWrite(SH_PIN, HIGH);   // 5. SH高电平: 开始电荷转移
  delay_ns(T3_SH_PULSE_WIDTH_NS); // 6. 等待t3 (SH脉冲宽度)
  digitalWrite(SH_PIN, LOW);    // 7. SH低电平: 结束电荷转移

  // --- 数据读出周期 ---
  // 在此期间，M在时钟运行，开始ADC采样OS信号
  start_adc_readout(); // 开始ADC数据读取

  delayMicroseconds(T_READOUT_US); // 8. 等待读出时间完成

  frameCounter++; // 增加帧计数器
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // 等待串口连接
  Serial.println("INFO: TCD1304信号发生器启动");

  DWT_Init(); // 初始化DWT用于精确延时

  // 初始化GPIO引脚，SH和ICG作为输出
  pinMode(SH_PIN, OUTPUT);
  pinMode(ICG_PIN, OUTPUT);
  digitalWrite(SH_PIN, LOW);
  digitalWrite(ICG_PIN, LOW); // 初始时ICG为低电平

  // 初始化ADC引脚
  pinMode(ADC_PIN, INPUT);
  analogReadResolution(12); // 设置ADC分辨率为12位

  // 设置主时钟 (CLK / M)
  setup_master_clock_pwm(MASTER_CLOCK_PIN, F_MASTER_CLOCK_HZ, MASTER_CLOCK_DUTY_CYCLE);

  // 设置ADC定时器
  setup_adc_timer();

  Serial.println("INFO: 设备初始化完成");
  Serial.print("INFO: 积分时间 ");
  Serial.print(T_INT_MS);
  Serial.println("ms");
  Serial.print("INFO: 数据读出时间 ");
  Serial.print(T_READOUT_US);
  Serial.println("us");
  Serial.println("INFO: 输入HELP查看控制命令");
}

void loop() {
  // 处理串口命令
  processSerialCommand();

  // 执行采集（根据模式）
  if (autoMode) {
    performCCDCapture();

    // 输出数据（如果准备好）
    if (dataReady) {
      if (outputFormat == "RAW") {
        print_pixel_data_raw();
      } else if (outputFormat == "CSV") {
        print_pixel_data_csv();
      } else if (outputFormat == "BINARY") {
        print_pixel_data_binary();
      } else {
        print_pixel_data_full();
      }

      // 如果不是连续模式，采集一次后停止
      if (!continuousMode) {
        autoMode = false;
        Serial.println("INFO: 单次采集完成");
      }
    }
  }

  // 短暂延时，避免过度占用CPU
  delay(10);
}