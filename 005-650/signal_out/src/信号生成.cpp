/*
 * STM32G431C8T6 1PPS+TOD Signal Generator and TDM Processor
 *
 * Signal Generation:
 * - A1: TOD output (9600 baud UART, 160ms low preamble + >300ms data frame)
 * - A4: PPS output (1Hz, 100ms high pulse, 900ms low)
 *
 * Signal Processing:
 * - A2: TOD_IN input
 * - A3: PPS_IN input
 * - B0: TDM output (Manchester encoded time-division multiplexed A2+A3)
 *
 * TOD Protocol: China Mobile QB-B-016-2010 specification
 */

#include <Arduino.h>
#include <HardwareTimer.h>
HardwareTimer* ppsPulseTimer = nullptr;
// Network byte order conversion functions (missing in Arduino)
uint16_t htons(uint16_t hostshort) {
    return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
}

uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) |
           ((hostlong & 0xFF00) << 8) |
           ((hostlong & 0xFF0000) >> 8) |
           ((hostlong & 0xFF000000) >> 24);
}

// Pin definitions
#define PIN_TOD_OUT   PA1  // TOD signal output
#define PIN_TOD_IN    PA2  // TOD signal input
#define PIN_PPS_IN    PA3  // PPS signal input
#define PIN_PPS_OUT   PA4  // PPS signal output
#define PIN_TDM_OUT   PB0  // TDM output

// Timing constants
#define PPS_PERIOD_MS     1000  // 1 second PPS period
#define PPS_HIGH_MS       100   // PPS高电平脉宽, 单位ms (已修正问题)
#define TOD_DELAY_MS      1     // 1ms delay after PPS rising edge
#define TOD_PREAMBLE_MS   160   // 160ms low preamble
#define TOD_BAUD_RATE     9600  // UART baud rate
#define TOD_BIT_TIME_US   (1000000 / TOD_BAUD_RATE)  // ~104us per bit

// TOD timing calculation for >300ms data transmission
// At 9600 baud: 1 byte = 10 bits = ~1.04ms
// For >300ms: need ~290 bytes total (including frame + padding)
#define TOD_PADDING_BYTES 270   // Padding bytes to extend transmission >300ms

// TDM and Manchester encoding
#define TDM_SAMPLE_RATE_HZ  2000   // 2kHz sampling rate for inputs
#define TDM_BIT_TIME_US     (1000000 / TDM_SAMPLE_RATE_HZ / 2)  // 250us per Manchester bit
#define MANCHESTER_HIGH_LOW 0      // Manchester: 1 = high-to-low transition
#define MANCHESTER_LOW_HIGH 1      // Manchester: 0 = low-to-high transition

// TOD frame structure (China Mobile QB-B-016-2010)
typedef struct {
    uint8_t sync1;          // 0x43 ('C')
    uint8_t sync2;          // 0x4D ('M')
    uint8_t class_id;       // 0x01 (time info message)
    uint8_t msg_id;         // 0x20
    uint16_t length;        // 16 bytes payload (big endian)
    uint32_t tow;           // GPS time of week (seconds)
    int32_t reserved;       // Reserved field
    uint16_t week;          // GPS week number
    int8_t leap_sec;        // Leap seconds offset
    uint8_t pps_status;     // PPS status (0x00 = normal)
    uint8_t time_acc;       // Time accuracy (0xFF for equipment)
    uint8_t reserve1;       // Reserved
    uint8_t reserve2;       // Reserved
    uint8_t reserve3;       // Reserved
    uint8_t fcs;            // Frame check sequence
} __attribute__((packed)) tod_frame_t;

// Global variables
static uint32_t last_pps_time = 0;
static bool pps_state = false;
static bool tod_sent = false;
static uint32_t gps_tow = 0;        // GPS time of week counter
static uint16_t gps_week = 2267;    // Current GPS week (example)

// --- 新增TOD时序控制变量 ---
static bool tod_pending = false;           // TOD帧是否待发送
static uint32_t tod_trigger_time = 0;      // TOD帧允许发送的时间点（PPS下降沿+16ms）

// TDM state
static uint32_t last_tdm_time = 0;
static bool tdm_source_select = false;  // false=TOD_IN, true=PPS_IN
static uint8_t manchester_phase = 0;    // 0 or 1 for Manchester encoding

/*
 * CRC-8 calculation for FCS field
 * Polynomial: x^8 + x^5 + x^4 + 1 (0x8C when reversed)
 * Initial value: 0xFF
 */
uint8_t calculate_fcs(const uint8_t *data, uint16_t length) {
    uint8_t crc = 0xFF;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0x8C;  // Polynomial: x^8+x^5+x^4+1
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/*
 * Send single byte via bit-banged UART
 * Format: 8N1 (8 data bits, no parity, 1 stop bit)
 * Idle state: HIGH, Start bit: LOW, Stop bit: HIGH
 */
void uart_send_byte(uint8_t pin, uint8_t data) {
    // Start bit (LOW)
    digitalWrite(pin, LOW);
    delayMicroseconds(TOD_BIT_TIME_US);

    // Data bits (LSB first)
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(pin, (data >> i) & 0x01);
        delayMicroseconds(TOD_BIT_TIME_US);
    }

    // Stop bit (HIGH)
    digitalWrite(pin, HIGH);
    delayMicroseconds(TOD_BIT_TIME_US);
}

/*
 * 构建并发送TOD帧 (QB-B-016-2010规范)
 * 已扩展填充字节以确保数据段传输时间 > 300ms
 * TOD帧的填充数据已修改为递增序列
 */
void send_tod_frame() {
    tod_frame_t frame;

    // 构建帧头
    frame.sync1 = 0x43;        // 'C'
    frame.sync2 = 0x4D;        // 'M'
    frame.class_id = 0x01;     // Time information message
    frame.msg_id = 0x20;       // Message ID
    frame.length = htons(16);  // 16 bytes payload (big endian)

    // Build payload
    frame.tow = htonl(gps_tow);        // GPS time of week
    frame.reserved = htonl(0);         // Reserved
    frame.week = htons(gps_week);      // GPS week
    frame.leap_sec = 18;               // Current GPS-UTC offset
    frame.pps_status = 0x00;           // Normal status
    frame.time_acc = 0xFF;             // Equipment setting
    frame.reserve1 = 0x00;
    frame.reserve2 = 0x00;
    frame.reserve3 = 0x00;

    // Calculate FCS (from class_id to reserve3)
    frame.fcs = calculate_fcs((uint8_t*)&frame.class_id,
                              sizeof(frame) - 3);  // Exclude sync1, sync2, fcs

    // Send 160ms low preamble
    digitalWrite(PIN_TOD_OUT, LOW);
    delay(TOD_PREAMBLE_MS);

    // Return to idle state briefly
    digitalWrite(PIN_TOD_OUT, HIGH);
    delayMicroseconds(200);  // Brief idle before data

    // Send frame via bit-banged UART
    uint8_t *frame_bytes = (uint8_t*)&frame;
    for (uint16_t i = 0; i < sizeof(frame); i++) {
        uart_send_byte(PIN_TOD_OUT, frame_bytes[i]);
    }

    // Send padding bytes to extend data transmission >300ms
    // Each byte takes ~1.04ms at 9600 baud, so 270 bytes = ~281ms
    // Total: 22 bytes frame + 270 padding = 292 bytes = ~304ms
    static uint8_t padding_data_value = 0x01; // 用于生成递增的填充数据, 静态变量保持状态
    for (uint16_t i = 0; i < TOD_PADDING_BYTES; i++) {
        uart_send_byte(PIN_TOD_OUT, padding_data_value);  // 发送变化的填充字节
        padding_data_value++;
        if (padding_data_value == 0x00) { // 如果计数器溢出到0 (即达到256后变为0)
            padding_data_value = 0x01;    // 则重置为0x01，避免发送0x00，并形成循环
        }
    }

    // 为下一帧递增GPS时间
    gps_tow++;
    if (gps_tow >= 604800) {  // Seconds in a week
        gps_tow = 0;
        gps_week++;
    }
}

/*
 * Manchester encode a bit and output via TDM
 * Manchester encoding:
 * - Bit 1: High-to-Low transition (1st half high, 2nd half low)
 * - Bit 0: Low-to-High transition (1st half low, 2nd half high)
 */
void manchester_encode_bit(uint8_t bit) {
    if (bit) {
        // Bit 1: High-to-Low
        digitalWrite(PIN_TDM_OUT, HIGH);
        delayMicroseconds(TDM_BIT_TIME_US);
        digitalWrite(PIN_TDM_OUT, LOW);
        delayMicroseconds(TDM_BIT_TIME_US);
    } else {
        // Bit 0: Low-to-High
        digitalWrite(PIN_TDM_OUT, LOW);
        delayMicroseconds(TDM_BIT_TIME_US);
        digitalWrite(PIN_TDM_OUT, HIGH);
        delayMicroseconds(TDM_BIT_TIME_US);
    }
}

/*
 * Time Division Multiplexing with Manchester encoding
 * Alternately sample TOD_IN and PPS_IN, encode and output to TDM_OUT
 */
void process_tdm() {
    uint32_t current_time = micros();

    // Check if it's time for next TDM sample
    if (current_time - last_tdm_time >= (2 * TDM_BIT_TIME_US)) {
        last_tdm_time = current_time;

        // Sample input based on current source selection
        uint8_t input_bit;
        if (tdm_source_select) {
            input_bit = digitalRead(PIN_PPS_IN);   // Sample PPS_IN
        } else {
            input_bit = digitalRead(PIN_TOD_IN);   // Sample TOD_IN
        }

        // Manchester encode and output
        manchester_encode_bit(input_bit);

        // Toggle source for next sample (time division)
        tdm_source_select = !tdm_source_select;
    }
}

/*
 * 生成PPS信号
 * 1Hz频率, 100ms高电平脉宽 (由硬件定时器精确控制), 900ms低电平
 */
void generate_pps() {
    uint32_t current_time = millis();

    // --- PPS上升沿处理 (周期开始) ---
    if (!pps_state && (current_time - last_pps_time >= PPS_PERIOD_MS)) {
        digitalWrite(PIN_PPS_OUT, HIGH); // 将PPS信号拉高
        pps_state = true;
        last_pps_time = current_time;
        tod_sent = false;
        tod_pending = false;
#if defined(STM32_CORE_VERSION) && (STM32_CORE_VERSION >= 0x02000000)
        if (ppsPulseTimer) {
            ppsPulseTimer->setCount(0);
            ppsPulseTimer->resume();
        }
#endif
    }

    // --- TOD帧发送触发 ---
    // 仅在PPS下降沿后16ms及以上，且本周期未发送TOD时执行
    if (tod_pending && !tod_sent && ((int32_t)(current_time - tod_trigger_time) >= 0)) {
        send_tod_frame();
        tod_sent = true;
        tod_pending = false;
    }
}

// PPS脉宽定时器中断服务程序
void ppsTimerISR() {
  digitalWrite(PIN_PPS_OUT, LOW); // 将PPS信号拉低
  pps_state = false;              // 更新PPS状态为低
#if defined(STM32_CORE_VERSION) && (STM32_CORE_VERSION >= 0x02000000)
  if (ppsPulseTimer) {
    ppsPulseTimer->pause(); // 停止定时器，直到下一次PPS脉冲
  }
#endif
  // --- TOD发送时序控制 ---
  tod_pending = true; // 标记TOD帧待发送
  tod_trigger_time = millis() + 16; // 记录允许发送TOD的时间点（PPS下降沿+16ms）
}

void setup() {
    // 初始化GPIO引脚
    pinMode(PIN_TOD_OUT, OUTPUT);
    pinMode(PIN_TOD_IN, INPUT);
    pinMode(PIN_PPS_IN, INPUT);
    pinMode(PIN_PPS_OUT, OUTPUT);
    pinMode(PIN_TDM_OUT, OUTPUT);

    // Set initial states
    digitalWrite(PIN_TOD_OUT, HIGH);   // TOD空闲高电平
    digitalWrite(PIN_PPS_OUT, LOW);    // PPS信号初始状态为低
    digitalWrite(PIN_TDM_OUT, LOW);    // TDM信号初始状态为低

#if defined(STM32_CORE_VERSION) && (STM32_CORE_VERSION >= 0x02000000)
    // 初始化PPS脉宽控制定时器 (例如使用TIM6)
    // TIM6 和 TIM7 通常是基本定时器，较少被Arduino核心功能占用
    ppsPulseTimer = new HardwareTimer(TIM6);
    if (ppsPulseTimer) {
        // 配置定时器在 PPS_HIGH_MS (100ms) 后触发中断
        // setOverflow 的周期单位由第二个参数决定，这里是微秒
        ppsPulseTimer->setOverflow(PPS_HIGH_MS * 1000, MICROSEC_FORMAT);
        ppsPulseTimer->attachInterrupt(ppsTimerISR); // 关联中断服务函数
        // 定时器在 generate_pps() 中PPS拉高时启动，此处不立即启动
    } else {
        // 如果 new HardwareTimer(TIM6) 失败 (例如TIM6不可用或内存不足)
        // 可以在此添加错误处理，例如通过串口打印错误信息
        Serial.begin(115200); // 假设串口已用于调试
        Serial.println("Error: Failed to initialize ppsPulseTimer!");
    }
#endif

    // 初始化时间变量
    last_pps_time = millis();
    last_tdm_time = micros();

    // 初始化GPS时间 (示例值)
    gps_tow = 345600;  // 示例: 星期三 4:00 AM GPS时间
    gps_week = 2267;   // 当前GPS周数
}

void loop() {
    // 生成PPS信号和TOD帧 (PPS脉宽由定时器中断控制)
    generate_pps();

    // Process TDM input sampling and Manchester encoding
    process_tdm();

    // Small delay to prevent overwhelming the processor
    delayMicroseconds(10);
}