// NRZI TDM信号解码+同步头检测+CRC校验+分离输出
#include <Arduino.h>
#include <stdint.h>

#define PIN_TDM_IN    PB0  // TDM信号输入
#define PIN_TOD_OUT   PB3  // TOD信号输出
#define PIN_PPS_OUT   PB4  // PPS信号输出

// TDM帧参数 (与发送端一致)
#define TDM_BIT_TIME_US     500  // NRZI每bit持续时间500us
#define TDM_SYNC_PATTERN    0xAA // 同步头模式 (10101010)
#define TDM_FRAME_SIZE      19   // 帧大小：同步头+帧序号+16字节数据+CRC
#define TDM_DATA_BITS       128  // 数据位数 (64对TOD/PPS)

HardwareSerial Serial2(PA3, PA2); // RX, TX

// CRC-8 校验函数 (与发送端一致)
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

// NRZI解码状态机
typedef enum {
    SYNC_SEARCH,    // 搜索同步头
    FRAME_DECODE,   // 解码帧内容
    FRAME_VALIDATE  // 校验帧
} decode_state_t;

// TDM帧结构
struct tdm_frame_t {
    uint8_t sync_header;    // 同步头 0xAA
    uint8_t frame_number;   // 帧序号
    uint8_t data[16];       // 128bit数据 (64对TOD/PPS采样)
    uint8_t crc;            // CRC校验
} __attribute__((packed));

void setup() {
    pinMode(PIN_TDM_IN, INPUT);
    pinMode(PIN_TOD_OUT, OUTPUT);
    pinMode(PIN_PPS_OUT, OUTPUT);
    digitalWrite(PIN_TOD_OUT, HIGH);
    digitalWrite(PIN_PPS_OUT, LOW);
    Serial2.begin(115200);
    delay(1000);
    Serial2.println("NRZI TDM Decode Start");
}

void loop() {
    static decode_state_t state = SYNC_SEARCH;
    static uint8_t sync_shift_reg = 0;
    static struct tdm_frame_t rx_frame;
    static uint8_t bit_buffer[152]; // 完整帧bit缓冲区
    static uint16_t bit_count = 0;
    static uint8_t nrzi_last_level = HIGH;
    static uint8_t expected_frame_seq = 0;
    static uint32_t frames_received = 0;
    static uint32_t frames_error = 0;
    static uint32_t next_sample_time = 0;
    static bool timing_initialized = false;

    uint8_t current_level = digitalRead(PIN_TDM_IN);
    uint32_t current_time = micros();

    // 初始化定时基准
    if (!timing_initialized) {
        next_sample_time = current_time + TDM_BIT_TIME_US;
        nrzi_last_level = current_level;
        timing_initialized = true;
        return;
    }

    // 统一的采样时序策略 - 使用绝对时间避免累积误差
    bool should_sample = false;

    // 检查是否到达下一个采样时间点
    if (current_time >= next_sample_time) {
        should_sample = true;
        next_sample_time += TDM_BIT_TIME_US; // 固定间隔递增，避免累积误差
    }

    if (should_sample) {
        // NRZI解码: 电平不变=1, 电平翻转=0
        uint8_t decoded_bit = (current_level == nrzi_last_level) ? 1 : 0;
        nrzi_last_level = current_level;

        // 调试输出 - 每64个bit输出一次解码状态（减少调试频率）
        static uint8_t debug_count = 0;
        if (++debug_count >= 64) {
            debug_count = 0;
            Serial2.print("S:");
            Serial2.print(state);
            Serial2.print(" Sync:0x");
            Serial2.print(sync_shift_reg, HEX);
            Serial2.print(" BC:");
            Serial2.print(bit_count);
            Serial2.print(" B:");
            Serial2.print(decoded_bit);
            Serial2.print(" L:");
            Serial2.println(current_level);
        }

        switch (state) {
            case SYNC_SEARCH:
                // 搜索同步头 0xAA (10101010) - 改进的同步检测
                sync_shift_reg = (sync_shift_reg << 1) | decoded_bit;
                if (sync_shift_reg == TDM_SYNC_PATTERN) {
                    // 找到同步头，重置状态准备接收帧数据
                    state = FRAME_DECODE;
                    bit_count = 0;
                    Serial2.print("SYNC @");
                    Serial2.print(current_time);
                    Serial2.print(" NextSample @");
                    Serial2.println(next_sample_time);
                }
                break;

            case FRAME_DECODE:
                // 收集帧数据bit (跳过同步头，收集帧序号+数据+CRC)
                if (bit_count < 144) { // 18字节 * 8bit = 144bit
                    bit_buffer[bit_count] = decoded_bit;
                    bit_count++;
                } else {
                    // 帧接收完成，转换为字节并校验
                    for (uint8_t i = 0; i < 18; i++) {
                        uint8_t byte_val = 0;
                        for (uint8_t j = 0; j < 8; j++) {
                            byte_val |= (bit_buffer[i * 8 + j] << (7 - j));
                        }
                        ((uint8_t*)&rx_frame)[i + 1] = byte_val; // +1跳过sync_header
                    }
                    rx_frame.sync_header = TDM_SYNC_PATTERN; // 手动设置同步头
                    state = FRAME_VALIDATE;
                }
                break;

            case FRAME_VALIDATE:
                // CRC校验
                uint8_t calc_crc = crc8((uint8_t*)&rx_frame.frame_number, 17);
                bool crc_ok = (calc_crc == rx_frame.crc);

                Serial2.print("F");
                Serial2.print(rx_frame.frame_number);
                Serial2.print(" ");
                if (crc_ok) {
                    Serial2.print("OK");
                    frames_received++;

                    // 检查帧序号连续性
                    if (frames_received > 1 && rx_frame.frame_number != expected_frame_seq) {
                        Serial2.print(" SeqErr(exp:");
                        Serial2.print(expected_frame_seq);
                        Serial2.print(")");
                    }
                    expected_frame_seq = rx_frame.frame_number + 1;

                    // 输出前4对TOD/PPS数据样本（减少输出量）
                    Serial2.print(" ");
                    uint8_t bit_pos = 0;
                    for (uint8_t i = 0; i < 4 && bit_pos < 128; i++) {
                        // TOD bit
                        uint8_t tod_bit = (rx_frame.data[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1;
                        digitalWrite(PIN_TOD_OUT, tod_bit);
                        Serial2.print("T");
                        Serial2.print(tod_bit);
                        bit_pos++;

                        // PPS bit
                        uint8_t pps_bit = (rx_frame.data[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1;
                        digitalWrite(PIN_PPS_OUT, pps_bit);
                        Serial2.print("P");
                        Serial2.print(pps_bit);
                        if (i < 3) Serial2.print(" ");
                        bit_pos++;
                    }
                } else {
                    Serial2.print("CRC_ERR");
                    frames_error++;
                }
                Serial2.println();

                // 返回同步搜索状态
                state = SYNC_SEARCH;
                sync_shift_reg = 0;

                // 定期输出统计信息
                static uint32_t last_stats_time = 0;
                if (current_time - last_stats_time > 3000000) { // 每3秒
                    last_stats_time = current_time;
                    uint32_t total_frames = frames_received + frames_error;
                    Serial2.print("=== RX:");
                    Serial2.print(frames_received);
                    Serial2.print(" ERR:");
                    Serial2.print(frames_error);
                    Serial2.print(" Rate:");
                    if (total_frames > 0) {
                        Serial2.print((frames_received * 100) / total_frames);
                    } else {
                        Serial2.print("0");
                    }
                    Serial2.println("% ===");
                }
                break;
        }
    }

    // 简化的边沿检测 - 仅用于信号质量监测
    static uint8_t prev_level = 255;
    static uint32_t last_edge_time = 0;
    static uint8_t edge_count = 0;

    if (current_level != prev_level && prev_level != 255) {
        uint32_t edge_interval = current_time - last_edge_time;
        if (edge_interval > 200 && edge_interval < 2000) { // 过滤噪声和异常长间隔
            if (++edge_count >= 100) { // 每100个边沿输出一次
                edge_count = 0;
                Serial2.print("Edge:");
                Serial2.print(edge_interval);
                Serial2.println("us");
            }
        }
        last_edge_time = current_time;
    }
    prev_level = current_level;
}