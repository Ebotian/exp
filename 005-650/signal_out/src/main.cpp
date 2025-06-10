/*
 * STM32G431C8T6 TDM Signal Processor with NRZI Encoding
 *
 * Signal Processing:
 * - PB3: TOD_IN input (from signal generator PA1)
 * - PB4: PPS_IN input (from signal generator PA4)
 * - PB0: TDM output (NRZI encoded time-division multiplexed PB3+PB4)
 *
 * TDM Frame Structure:
 * - Sync Header: 0xAA (8 bits)
 * - Frame Sequence: 1 byte
 * - Data: 128 bits (64 pairs of TOD/PPS samples)
 * - CRC-8: 1 byte
 * Total: 144 bits per frame, NRZI encoded
 */

#include <Arduino.h>
#include <stdint.h>

#define PIN_TOD_IN    PB3  // TOD信号输入
#define PIN_PPS_IN    PB4  // PPS信号输入
#define PIN_TDM_OUT   PB0  // TDM输出

// TDM和NRZI编码参数
#define TDM_BIT_TIME_US     500  // NRZI每bit持续时间500us (2kHz)
#define TDM_FRAME_DATA_BITS 128  // 每帧数据位数 (64对TOD/PPS)
#define TDM_SYNC_PATTERN    0xAA // 同步头模式 (10101010)

HardwareSerial Serial2(PA3, PA2); // RX, TX

// CRC-8 (x^8 + x^2 + x + 1, poly=0x07, init=0x00)
uint8_t crc8(const uint8_t *data, uint16_t len) {
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// NRZI编码: 1=不变, 0=翻转 - 优化版本，更精确的时序控制
void nrzi_encode_and_send(uint8_t *bits, uint16_t bit_count) {
    static uint8_t nrzi_level = HIGH; // NRZI电平状态（静态保持）

    // 发送前设置稳定高电平，便于接收端初始化NRZI状态
    digitalWrite(PIN_TDM_OUT, HIGH);
    nrzi_level = HIGH;
    delayMicroseconds(2000); // 2ms稳定时间，确保接收端能检测到稳定状态

    // 记录开始时间，用于精确时序控制
    uint32_t start_time = micros();

    for (uint16_t i = 0; i < bit_count; i++) {
        // NRZI编码逻辑
        if (bits[i] == 0) nrzi_level = !nrzi_level; // 0=翻转
        // 1=保持不变

        // 计算当前bit的目标开始时间
        uint32_t bit_start_time = start_time + (i * TDM_BIT_TIME_US);

        // 精确等待到目标时间（避免溢出问题）
        while (true) {
            uint32_t current_time = micros();
            int32_t time_diff = (int32_t)(current_time - bit_start_time);
            if (time_diff >= 0) break;
        }

        // 立即输出电平
        digitalWrite(PIN_TDM_OUT, nrzi_level);

        // 减少调试输出频率，避免时序干扰
        static uint16_t debug_count = 0;
        if (++debug_count >= 152) { // 每整帧输出一次
            debug_count = 0;
            Serial2.print("NRZI Frame sent, Level: ");
            Serial2.println(nrzi_level);
        }
    }

    // 确保最后一个bit持续完整时间
    uint32_t frame_end_time = start_time + (bit_count * TDM_BIT_TIME_US);
    while (true) {
        uint32_t current_time = micros();
        int32_t time_diff = (int32_t)(current_time - frame_end_time);
        if (time_diff >= 0) break;
    }
}

// 将字节数组转换为bit数组
void bytes_to_bits(uint8_t *bytes, uint16_t byte_count, uint8_t *bits) {
    for (uint16_t i = 0; i < byte_count; i++) {
        for (uint8_t j = 0; j < 8; j++) {
            bits[i * 8 + j] = (bytes[i] >> (7 - j)) & 1;
        }
    }
}

// 发送一帧TDM数据（同步头+帧序号+数据+CRC，NRZI编码）
void send_tdm_frame(uint16_t bit_count) {
    static uint8_t frame_seq = 0;

    // TDM帧结构定义
    struct {
        uint8_t sync_header;    // 同步头 0xAA
        uint8_t frame_number;   // 帧序号
        uint8_t data[16];       // 128bit数据 (64对TOD/PPS采样)
        uint8_t crc;            // CRC校验
    } __attribute__((packed)) tdm_frame;

    // 构建帧头
    tdm_frame.sync_header = TDM_SYNC_PATTERN;
    tdm_frame.frame_number = frame_seq++;

    // 采样128bit数据 (64对TOD/PPS交替采样)
    uint8_t bit_pos = 0;
    for (uint8_t i = 0; i < 16; i++) tdm_frame.data[i] = 0; // 清零

    for (uint8_t i = 0; i < 64; i++) {
        // TOD采样
        uint8_t tod_bit = digitalRead(PIN_TOD_IN);
        tdm_frame.data[bit_pos / 8] |= (tod_bit << (7 - (bit_pos % 8)));
        bit_pos++;

        // PPS采样
        uint8_t pps_bit = digitalRead(PIN_PPS_IN);
        tdm_frame.data[bit_pos / 8] |= (pps_bit << (7 - (bit_pos % 8)));
        bit_pos++;
    }

    // 计算CRC (帧序号+数据)
    tdm_frame.crc = crc8((uint8_t*)&tdm_frame.frame_number, 17);

    // 转换为bit数组并NRZI编码发送
    uint8_t frame_bits[152]; // (1+1+16+1)*8 = 152 bits
    bytes_to_bits((uint8_t*)&tdm_frame, sizeof(tdm_frame), frame_bits);

    // NRZI编码发送整帧
    nrzi_encode_and_send(frame_bits, 152);

    // 帧间空闲 - 设置稳定的高电平空闲状态
    digitalWrite(PIN_TDM_OUT, HIGH);
    delayMicroseconds(3000); // 3ms帧间隔，给接收端充足的处理时间

    // 显著减少调试输出频率，避免串口干扰时序
    static uint8_t frame_debug_count = 0;
    if (++frame_debug_count >= 50) { // 每50帧输出一次
        frame_debug_count = 0;
        Serial2.print("Frame ");
        Serial2.print(tdm_frame.frame_number);
        Serial2.print(" CRC:0x");
        Serial2.print(tdm_frame.crc, HEX);
        Serial2.println();
    }
}

void setup() {
    pinMode(PIN_TOD_IN, INPUT);
    pinMode(PIN_PPS_IN, INPUT);
    pinMode(PIN_TDM_OUT, OUTPUT);
    digitalWrite(PIN_TDM_OUT, HIGH); // 空闲高电平
    Serial2.begin(115200);
    delay(1000);
    Serial2.println("TDM Encode Debug Start");
}

void loop() {
    static uint32_t last_frame_time = 0;
    const uint32_t TDM_FRAME_INTERVAL_MS = 50; // 每50ms发一帧 (20Hz)

    uint32_t now = millis();
    if (now - last_frame_time >= TDM_FRAME_INTERVAL_MS) {
        last_frame_time = now;
        send_tdm_frame(TDM_FRAME_DATA_BITS);
        digitalWrite(PIN_TDM_OUT, HIGH); // 确保帧间空闲高电平
    }

    // 减少输入信号监测频率，避免串口干扰
    static uint32_t last_debug_time = 0;
    static uint8_t input_debug_count = 0;
    if (now - last_debug_time > 2000 && ++input_debug_count >= 50) { // 每100秒打印一次
        last_debug_time = now;
        input_debug_count = 0;
        Serial2.print("Input TOD:");
        Serial2.print(digitalRead(PIN_TOD_IN));
        Serial2.print(" PPS:");
        Serial2.println(digitalRead(PIN_PPS_IN));
    }

    delayMicroseconds(50); // 降低CPU占用
}