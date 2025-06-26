// STM32G431C8T6 智能TDM信号解码与重建系统
// 参考: smart_tdm.cpp, pps重建.cpp, 说明文档
#include <Arduino.h>
#include <stdint.h>
#include <HardwareTimer.h>
#include <stddef.h>

#define PIN_TDM_IN    PB0  // TDM信号输入
#define PIN_TOD_OUT   PB3  // TOD信号输出
#define PIN_PPS_OUT   PB4  // PPS信号输出
#define PIN_PPS_RAW   PB5  // 原始PPS比特输出

#define TDM_BIT_TIME_US     500
#define TDM_SYNC_PATTERN    0x5A
#define MAX_FRAME_SIZE      1024
#define FRAME_BUFFER_SIZE   8
#define PPS_TIME_BUF_SIZE   32
#define PPS_THRESH_BUF_SIZE 20
#define TOD_BAUD_RATE       9600
#define TOD_BIT_US          (1000000 / TOD_BAUD_RATE)
#define TOD_PREAMBLE_MS     160
#define TOD_IDLE_US         200
#define TOD_FRAME_BYTES     64
#define TOD_PADDING_BYTES   16
#define TOD_SEG_MAX_BITS 512

HardwareSerial Serial2(PA3, PA2);

// --- TDM帧结构 ---
struct SmartTDMFrame {
    uint8_t sync_header;          // 0x5A
    uint8_t frame_type;           // 0x01=TOD, 0x02=PPS, 0x03=混合
    uint16_t sequence;
    uint32_t timestamp;
    uint16_t payload_length;
    uint8_t payload[512];         // 变长载荷
    uint8_t crc;
} __attribute__((packed));

struct buffered_frame_t {
    SmartTDMFrame frame;
    uint32_t reception_time;
};

static struct buffered_frame_t frame_buffer[FRAME_BUFFER_SIZE];
static volatile uint8_t frame_head = 0, frame_tail = 0;

// --- PPS双重环形缓冲 ---
static uint32_t pps_times[PPS_TIME_BUF_SIZE];
static uint8_t  pps_time_head = 0, pps_time_tail = 0;
static uint32_t pps_thresh_buf[PPS_THRESH_BUF_SIZE];
static uint8_t  pps_thresh_head = 0, pps_thresh_count = 0;
static uint32_t pps_period_smooth = 1000000UL;

static HardwareTimer ppsTimer(TIM2);
static HardwareTimer todUartTimer(TIM3);

// TOD UART输出状态机
typedef enum {
    TOD_STATE_IDLE,
    TOD_STATE_PREAMBLE,
    TOD_STATE_POST_PREAMBLE_IDLE,
    TOD_STATE_TRANSMITTING
} tod_tx_state_t;
static volatile tod_tx_state_t tod_tx_state = TOD_STATE_IDLE;
static uint8_t tod_tx_data[TOD_FRAME_BYTES];
static volatile int tod_tx_bit_counter = -1;

// PPS输出脉冲管理
static volatile uint32_t pps_pulse_end_time = 0;

// --- 统计变量 ---
static volatile uint32_t frames_received = 0;
static volatile uint32_t frames_error = 0;
static volatile uint32_t frames_seq_error = 0;
static volatile uint32_t buffer_full_count = 0;
static volatile uint32_t pps_irq_count = 0;

// PPS输入统计
static volatile uint32_t pps_in_min_dt = 0xFFFFFFFF;
static volatile uint32_t pps_in_max_dt = 0;
static volatile uint64_t pps_in_sum_dt = 0;
static volatile uint32_t pps_in_stat_count = 0;
// PPS输出统计
static volatile uint32_t pps_out_min_dt = 0xFFFFFFFF;
static volatile uint32_t pps_out_max_dt = 0;
static volatile uint64_t pps_out_sum_dt = 0;
static volatile uint32_t pps_out_stat_count = 0;

// --- CRC8算法 (与编码端一致) ---
uint8_t crc8(const uint8_t *data, uint16_t len) {
    uint8_t crc = 0xFF;
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

// --- PPS重建: 记录PPS事件并平滑周期 ---
static void record_pps(uint32_t now) {
    pps_times[pps_time_head] = now;
    pps_time_head = (pps_time_head + 1) % PPS_TIME_BUF_SIZE;
    if (pps_time_head == pps_time_tail)
        pps_time_tail = (pps_time_tail + 1) % PPS_TIME_BUF_SIZE;
    while (pps_time_tail != pps_time_head && now - pps_times[pps_time_tail] > 60000000UL) {
        pps_time_tail = (pps_time_tail + 1) % PPS_TIME_BUF_SIZE;
    }
    uint32_t min_dt = 0xFFFFFFFF;
    uint8_t idx = pps_time_tail;
    while ((idx = (idx + 1) % PPS_TIME_BUF_SIZE) != pps_time_head) {
        uint8_t prev = (idx + PPS_TIME_BUF_SIZE - 1) % PPS_TIME_BUF_SIZE;
        uint32_t dt = pps_times[idx] - pps_times[prev];
        if (dt < min_dt) min_dt = dt;
    }
    if (min_dt == 0xFFFFFFFF) return;
    if (min_dt < pps_in_min_dt) pps_in_min_dt = min_dt;
    if (min_dt > pps_in_max_dt) pps_in_max_dt = min_dt;
    pps_in_sum_dt += min_dt;
    pps_in_stat_count++;
    pps_thresh_buf[pps_thresh_head] = min_dt;
    pps_thresh_head = (pps_thresh_head + 1) % PPS_THRESH_BUF_SIZE;
    if (pps_thresh_count < PPS_THRESH_BUF_SIZE) pps_thresh_count++;
    uint64_t sum = 0;
    for (uint8_t i = 0; i < pps_thresh_count; i++) sum += pps_thresh_buf[i];
    pps_period_smooth = sum / pps_thresh_count;
    ppsTimer.setOverflow(pps_period_smooth, MICROSEC_FORMAT);
}

// --- PPS输出定时器ISR ---
static void ppsOutCallback() {
    digitalWrite(PIN_PPS_OUT, HIGH);
    pps_pulse_end_time = micros() + 1000; // 1ms脉冲
    pps_irq_count++;
    static uint32_t last_pps_time = 0;
    uint32_t now = micros();
    if (last_pps_time != 0) {
        uint32_t dt = now - last_pps_time;
        if (dt < pps_out_min_dt) pps_out_min_dt = dt;
        if (dt > pps_out_max_dt) pps_out_max_dt = dt;
        pps_out_sum_dt += dt;
        pps_out_stat_count++;
    }
    last_pps_time = now;
}

// --- TOD UART输出状态机 ---
void todUartCallback() {
    const int bits_per_uart_char = 10;
    const int data_bits_total = TOD_FRAME_BYTES * bits_per_uart_char;
    const int padding_bits_total = TOD_PADDING_BYTES * bits_per_uart_char;
    uint8_t byte_to_send;
    int bit_in_uart_char;
    if (tod_tx_state == TOD_STATE_PREAMBLE) {
        digitalWrite(PIN_TOD_OUT, HIGH);
        tod_tx_state = TOD_STATE_POST_PREAMBLE_IDLE;
        todUartTimer.setOverflow(TOD_IDLE_US, MICROSEC_FORMAT);
        return;
    }
    if (tod_tx_state == TOD_STATE_POST_PREAMBLE_IDLE) {
        tod_tx_state = TOD_STATE_TRANSMITTING;
        tod_tx_bit_counter = 0;
        todUartTimer.setOverflow(TOD_BIT_US, MICROSEC_FORMAT);
    }
    if (tod_tx_state == TOD_STATE_TRANSMITTING) {
        if (tod_tx_bit_counter < data_bits_total) {
            int byte_index = tod_tx_bit_counter / bits_per_uart_char;
            bit_in_uart_char = tod_tx_bit_counter % bits_per_uart_char;
            byte_to_send = tod_tx_data[byte_index];
        } else if (tod_tx_bit_counter < data_bits_total + padding_bits_total) {
            int padding_bit_index = tod_tx_bit_counter - data_bits_total;
            int byte_index = padding_bit_index / bits_per_uart_char;
            bit_in_uart_char = padding_bit_index % bits_per_uart_char;
            byte_to_send = (byte_index + 1) % 256;
            if (byte_to_send == 0) byte_to_send = 1;
        } else {
            digitalWrite(PIN_TOD_OUT, HIGH);
            tod_tx_state = TOD_STATE_IDLE;
            todUartTimer.pause();
            return;
        }
        if (bit_in_uart_char == 0) {
            digitalWrite(PIN_TOD_OUT, LOW);
        } else if (bit_in_uart_char < 9) {
            if ((byte_to_send >> (bit_in_uart_char - 1)) & 0x01) {
                digitalWrite(PIN_TOD_OUT, HIGH);
            } else {
                digitalWrite(PIN_TOD_OUT, LOW);
            }
        } else {
            digitalWrite(PIN_TOD_OUT, HIGH);
        }
        tod_tx_bit_counter++;
    }
}

void start_tod_frame_transmission(const uint8_t* data, uint16_t len) {
    if (tod_tx_state != TOD_STATE_IDLE) return;
    memset(tod_tx_data, 0, sizeof(tod_tx_data));
    memcpy(tod_tx_data, data, len > TOD_FRAME_BYTES ? TOD_FRAME_BYTES : len);
    tod_tx_state = TOD_STATE_PREAMBLE;
    digitalWrite(PIN_TOD_OUT, LOW);
    todUartTimer.setOverflow(TOD_PREAMBLE_MS * 1000, MICROSEC_FORMAT);
    todUartTimer.refresh();
    todUartTimer.resume();
}

// --- 新增: TOD/PPS分离结构体 ---
struct DecodedTODSegment {
    uint32_t start_timestamp;
    uint16_t duration_us;
    uint16_t bit_count;
    uint8_t  data[TOD_SEG_MAX_BITS / 8];
    uint8_t  valid;
};
struct DecodedPPSEvent {
    uint32_t timestamp;
    uint8_t  is_rising;
    uint8_t  valid;
};

// --- 新增: 分离payload中的TOD/PPS ---
void split_and_process_payload(const uint8_t* payload, uint16_t payload_length) {
    uint16_t offset = 0;
    while (offset + sizeof(DecodedTODSegment) <= payload_length) {
        // 优先尝试TOD片段
        DecodedTODSegment tod;
        memcpy(&tod, payload + offset, sizeof(DecodedTODSegment));
        if (tod.valid == 1) {
            // 处理TOD片段
            start_tod_frame_transmission(tod.data, (tod.bit_count + 7) / 8);
        }
        offset += sizeof(DecodedTODSegment);
    }
    // 剩余部分尝试PPS事件
    while (offset + sizeof(DecodedPPSEvent) <= payload_length) {
        DecodedPPSEvent pps;
        memcpy(&pps, payload + offset, sizeof(DecodedPPSEvent));
        if (pps.valid == 1) {
            digitalWrite(PIN_PPS_RAW, pps.is_rising ? HIGH : LOW);
            if (pps.is_rising) record_pps(pps.timestamp);
        }
        offset += sizeof(DecodedPPSEvent);
    }
}

// --- 修改: process_frame ---
void process_frame(const SmartTDMFrame* frame, uint32_t reception_time) {
    static uint16_t last_sequence = 0;
    if (frame->sync_header != TDM_SYNC_PATTERN) return;
    uint8_t calc_crc = crc8((const uint8_t*)frame, sizeof(SmartTDMFrame) - 1);
    if (calc_crc != frame->crc) {
        frames_error++;
        return;
    }
    frames_received++;
    if (frames_received > 1 && frame->sequence != last_sequence + 1) frames_seq_error++;
    last_sequence = frame->sequence;
    if (frame->frame_type == 0x01 || frame->frame_type == 0x02 || frame->frame_type == 0x03) {
        split_and_process_payload(frame->payload, frame->payload_length);
    }
}

void setup() {
    pinMode(PIN_TDM_IN, INPUT);
    pinMode(PIN_TOD_OUT, OUTPUT);
    pinMode(PIN_PPS_OUT, OUTPUT);
    pinMode(PIN_PPS_RAW, OUTPUT);
    digitalWrite(PIN_TOD_OUT, HIGH);
    digitalWrite(PIN_PPS_OUT, LOW);
    digitalWrite(PIN_PPS_RAW, LOW);
    Serial2.begin(115200);
    delay(1000);
    Serial2.println("Smart TDM Decoder Start");
    ppsTimer.setOverflow(pps_period_smooth, MICROSEC_FORMAT);
    ppsTimer.attachInterrupt(ppsOutCallback);
    ppsTimer.resume();
    todUartTimer.setOverflow(TOD_BIT_US, MICROSEC_FORMAT);
    todUartTimer.attachInterrupt(todUartCallback);
    todUartTimer.pause();
    digitalWrite(PIN_TOD_OUT, HIGH);
}

void loop() {
    // --- PPS输出脉冲宽度管理 ---
    if (pps_pulse_end_time != 0 && micros() >= pps_pulse_end_time) {
        digitalWrite(PIN_PPS_OUT, LOW);
        pps_pulse_end_time = 0;
    }
    // --- 消费者: 处理已接收的完整帧 ---
    if (frame_tail != frame_head) {
        struct buffered_frame_t buffered_frame;
        noInterrupts();
        memcpy(&buffered_frame, &frame_buffer[frame_tail], sizeof(struct buffered_frame_t));
        frame_tail = (frame_tail + 1) % FRAME_BUFFER_SIZE;
        interrupts();
        process_frame(&buffered_frame.frame, buffered_frame.reception_time);
    }
    // --- 生产者: TDM信号采样与解码 ---
    static enum { SYNC_SEARCH, FRAME_HEADER, FRAME_PAYLOAD } state = SYNC_SEARCH;
    static uint8_t sync_shift_reg = 0;
    static SmartTDMFrame rx_frame;
    static uint8_t bit_buffer[MAX_FRAME_SIZE * 8];
    static uint16_t bit_count = 0;
    static uint8_t nrzi_last_level = HIGH;
    static uint32_t next_sample_time = 0;
    static bool timing_initialized = false;
    uint8_t current_level = digitalRead(PIN_TDM_IN);
    uint32_t current_time = micros();
    if (!timing_initialized) {
        next_sample_time = current_time + TDM_BIT_TIME_US;
        nrzi_last_level = current_level;
        timing_initialized = true;
        return;
    }
    if (current_time >= next_sample_time) {
        next_sample_time += TDM_BIT_TIME_US;
        uint8_t decoded_bit = (current_level != nrzi_last_level) ? 1 : 0;
        nrzi_last_level = current_level;
        static uint8_t debug_bit_count = 0;
        switch (state) {
            case SYNC_SEARCH:
                sync_shift_reg = (sync_shift_reg << 1) | decoded_bit;
                // shift-in到bit_buffer[0~7]
                for (int i = 0; i < 7; ++i) bit_buffer[i] = bit_buffer[i+1];
                bit_buffer[7] = decoded_bit;
                debug_bit_count++;
                if (debug_bit_count >= 8) {
                    Serial2.print("SYNC_SHIFT: 0x");
                    Serial2.println(sync_shift_reg, HEX);
                    debug_bit_count = 0;
                }
                if (sync_shift_reg == TDM_SYNC_PATTERN) {
                    Serial2.println("SYNC FOUND!");
                    // 用sync_shift_reg的8位按MSB first写入bit_buffer[0~7]
                    for (int i = 0; i < 8; ++i) {
                        bit_buffer[i] = (sync_shift_reg >> (7 - i)) & 1;
                    }
                    bit_count = 8;
                    memset(bit_buffer + 8, 0, 80);
                    state = FRAME_HEADER;
                }
                break;
            case FRAME_HEADER:
                bit_buffer[bit_count++] = decoded_bit;
                if (bit_count >= 80) { // 10字节*8bit
                    Serial2.print("bit_buffer (HEADER HEX): ");
                    for (int i = 0; i < 10; ++i) {
                        uint8_t val = 0;
                        for (int j = 0; j < 8; ++j) val |= (bit_buffer[i*8+j] << (7-j));
                        Serial2.print("0x"); Serial2.print(val, HEX); Serial2.print(" ");
                    }
                    Serial2.println();
                    uint8_t* p = (uint8_t*)&rx_frame;
                    for (uint8_t i = 0; i < 10; i++) {
                        uint8_t val = 0;
                        for (uint8_t j = 0; j < 8; j++) {
                            val |= (bit_buffer[i * 8 + j] << (7 - j)); // MSB first
                        }
                        p[i] = val;
                    }
                    Serial2.print("rx_frame.sync_header: 0x"); Serial2.println(rx_frame.sync_header, HEX);
                    Serial2.print("rx_frame.frame_type: 0x"); Serial2.println(rx_frame.frame_type, HEX);
                    Serial2.print("rx_frame.sequence: 0x"); Serial2.println(rx_frame.sequence, HEX);
                    Serial2.print("rx_frame.timestamp: 0x"); Serial2.println(rx_frame.timestamp, HEX);
                    Serial2.print("rx_frame.payload_length: 0x"); Serial2.println(rx_frame.payload_length, HEX);
                    state = FRAME_PAYLOAD;
                    bit_count = 0;
                }
                break;
            case FRAME_PAYLOAD:
                if (rx_frame.payload_length > sizeof(rx_frame.payload)) {
                    state = SYNC_SEARCH;
                    break;
                }
                bit_buffer[bit_count++] = decoded_bit;
                if (bit_count >= rx_frame.payload_length * 8 + 8) { // payload+crc
                    for (uint16_t i = 0; i < rx_frame.payload_length; i++) {
                        uint8_t val = 0;
                        for (uint8_t j = 0; j < 8; j++) {
                            val |= (bit_buffer[i * 8 + j] << (7 - j)); // MSB first
                        }
                        rx_frame.payload[i] = val;
                    }
                    rx_frame.crc = 0;
                    for (uint8_t j = 0; j < 8; j++) {
                        rx_frame.crc |= (bit_buffer[rx_frame.payload_length * 8 + j] << (7 - j));
                    }
                    // 入缓冲区
                    uint8_t next_head = (frame_head + 1) % FRAME_BUFFER_SIZE;
                    if (next_head != frame_tail) {
                        memcpy(&frame_buffer[frame_head].frame, &rx_frame, sizeof(SmartTDMFrame));
                        frame_buffer[frame_head].reception_time = current_time;
                        frame_head = next_head;
                    } else {
                        buffer_full_count++;
                    }
                    state = SYNC_SEARCH;
                    sync_shift_reg = 0;
                }
                break;
        }
    }
    // --- 独立状态报告 ---
    static uint32_t last_report_time = 0;
    if (millis() - last_report_time > 2000) {
        last_report_time = millis();
        noInterrupts();
        uint32_t ok = frames_received, err = frames_error, seq = frames_seq_error, buf = buffer_full_count;
        uint32_t pps_in_min = pps_in_min_dt, pps_in_max = pps_in_max_dt, pps_in_cnt = pps_in_stat_count;
        uint64_t pps_in_sum = pps_in_sum_dt;
        uint32_t pps_out_min = pps_out_min_dt, pps_out_max = pps_out_max_dt, pps_out_cnt = pps_out_stat_count;
        uint64_t pps_out_sum = pps_out_sum_dt;
        uint32_t pps_irq = pps_irq_count;
        interrupts();
        Serial2.println("--- STATUS REPORT ---");
        Serial2.print("Frame OK: "); Serial2.print(ok);
        Serial2.print(" | CRC Err: "); Serial2.print(err);
        Serial2.print(" | Seq Err: "); Serial2.print(seq);
        Serial2.print(" | BufFull: "); Serial2.print(buf);
        Serial2.print(" | PPS IRQ: "); Serial2.print(pps_irq);
        Serial2.print(" | PPSin[min:"); Serial2.print(pps_in_min);
        Serial2.print(",max:"); Serial2.print(pps_in_max);
        Serial2.print(",avg:"); Serial2.print(pps_in_cnt ? (pps_in_sum / pps_in_cnt) : 0);
        Serial2.print("] | PPSout[min:"); Serial2.print(pps_out_min);
        Serial2.print(",max:"); Serial2.print(pps_out_max);
        Serial2.print(",avg:"); Serial2.print(pps_out_cnt ? (pps_out_sum / pps_out_cnt) : 0);
        Serial2.println("]");
    }
}
