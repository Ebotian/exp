// NRZI TDM信号解码+同步头检测+CRC校验+分离输出
// 重构版本 V3: 移除所有实时打印，建立独立报告机制
#include <Arduino.h>
#include <stdint.h>
#include <HardwareTimer.h>

#define PIN_TDM_IN    PB0  // TDM信号输入
#define PIN_TOD_OUT   PB3  // TOD信号输出
#define PIN_PPS_OUT   PB4  // PPS信号输出
#define PIN_PPS_RAW   PB5  // 原始解码的PPS比特输出

// 使用BSRR寄存器快速操作GPIO
#define TOD_OUT_HIGH() (GPIOB->BSRR = (1U << 3))
#define TOD_OUT_LOW()  (GPIOB->BSRR = (1U << (3 + 16)))

// TDM帧参数
#define TDM_BIT_TIME_US     500
#define TDM_SYNC_PATTERN    0xAA
#define TDM_FRAME_SIZE      19
#define TDM_DATA_BITS       128

HardwareSerial Serial2(PA3, PA2);

// —— PPS 平滑滤波参数及变量 ——
#define PPS_WINDOW_US       (60UL * 1000000UL)
#define PPS_TIME_BUF_SIZE   32
#define PPS_THRESH_BUF_SIZE 20
static uint32_t pps_times[PPS_TIME_BUF_SIZE];
static uint8_t  pps_time_head = 0, pps_time_tail = 0;
static uint32_t pps_thresh_buf[PPS_THRESH_BUF_SIZE];
static uint8_t  pps_thresh_head = 0, pps_thresh_count = 0;
static uint32_t pps_period_smooth = 1000000UL;

static HardwareTimer ppsTimer(TIM2);
static HardwareTimer todUartTimer(TIM3);

// TOD UART输出参数
#define TOD_BAUD_RATE     9600
#define TOD_BIT_US        (1000000 / TOD_BAUD_RATE)
#define TOD_FRAME_BYTES   16
#define TOD_PREAMBLE_MS   160
#define TOD_IDLE_US       200
#define TOD_PADDING_BYTES 16

// TOD UART状态机
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

// --- 独立报告机制的统计变量 ---
// 注意：所有被中断修改的变量都应声明为 volatile
static volatile uint32_t pps_irq_count = 0;
static volatile uint32_t frames_received = 0;
static volatile uint32_t frames_error = 0;
static volatile uint32_t frames_seq_error = 0;
static volatile uint32_t buffer_full_count = 0;
// PPS 输入统计 (解码)
static volatile uint32_t pps_in_min_dt = 0xFFFFFFFF;
static volatile uint32_t pps_in_max_dt = 0;
static volatile uint64_t pps_in_sum_dt = 0;
static volatile uint32_t pps_in_stat_count = 0;
// PPS 输出统计 (中断)
static volatile uint32_t pps_out_min_dt = 0xFFFFFFFF;
static volatile uint32_t pps_out_max_dt = 0;
static volatile uint64_t pps_out_sum_dt = 0;
static volatile uint32_t pps_out_stat_count = 0;
// --- 结束 ---

// TDM帧结构
struct tdm_frame_t {
    uint8_t sync_header;
    uint8_t frame_number;
    uint8_t data[16];
    uint8_t crc;
} __attribute__((packed));

// 帧缓冲区
struct buffered_frame_t {
    struct tdm_frame_t frame;
    uint32_t reception_time;
};
#define FRAME_BUFFER_SIZE 8 // 增加缓冲区大小以应对潜在的处理延迟
static struct buffered_frame_t frame_buffer[FRAME_BUFFER_SIZE];
static volatile uint8_t frame_head = 0;
static volatile uint8_t frame_tail = 0;

// ppsOutCallback: PPS输出定时器中断服务程序 (ISR)
// 关键原则：ISR中代码必须极简，严禁任何阻塞或耗时操作（如Serial.print）
static void ppsOutCallback() {
    uint32_t now = micros();
    static uint32_t pps_out_last_time = 0;

    if (pps_out_last_time != 0) {
        uint32_t dt = now - pps_out_last_time;
        if (dt < pps_out_min_dt) pps_out_min_dt = dt;
        if (dt > pps_out_max_dt) pps_out_max_dt = dt;
        pps_out_sum_dt += dt;
        pps_out_stat_count++;
    }
    pps_out_last_time = now;

    digitalWrite(PIN_PPS_OUT, HIGH);
    pps_pulse_end_time = now + 100000; // 100ms脉冲
    pps_irq_count++;
}

// record_pps: 记录解码后的PPS边沿，并更新平滑周期
// 已移除所有打印
static void record_pps(uint32_t now) {
    pps_times[pps_time_head] = now;
    pps_time_head = (pps_time_head + 1) % PPS_TIME_BUF_SIZE;
    if (pps_time_head == pps_time_tail)
        pps_time_tail = (pps_time_tail + 1) % PPS_TIME_BUF_SIZE;
    while (pps_time_tail != pps_time_head && now - pps_times[pps_time_tail] > PPS_WINDOW_US) {
        pps_time_tail = (pps_time_tail + 1) % PPS_TIME_BUF_SIZE;
    }
    uint32_t min_dt = UINT32_MAX;
    uint8_t idx = pps_time_tail;
    while ((idx = (idx + 1) % PPS_TIME_BUF_SIZE) != pps_time_head) {
        uint8_t prev = (idx + PPS_TIME_BUF_SIZE - 1) % PPS_TIME_BUF_SIZE;
        uint32_t dt = pps_times[idx] - pps_times[prev];
        if (dt < min_dt) min_dt = dt;
    }
    if (min_dt == UINT32_MAX) return;

    // 更新输入PPS统计
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

// 解码状态机
typedef enum {
    SYNC_SEARCH,
    FRAME_DECODE,
} decode_state_t;

// 函数声明
void todUartCallback();
void start_tod_frame_transmission(const uint8_t* data);
void process_frame(const struct tdm_frame_t* frame, uint32_t reception_time);

void setup() {
    pinMode(PIN_TDM_IN, INPUT);
    pinMode(PIN_TOD_OUT, OUTPUT);
    pinMode(PIN_PPS_OUT, OUTPUT);
    pinMode(PIN_PPS_RAW, OUTPUT);
    TOD_OUT_HIGH();
    digitalWrite(PIN_PPS_OUT, LOW);
    digitalWrite(PIN_PPS_RAW, LOW);
    Serial.begin(115200);
    Serial2.begin(115200);
    delay(1000);
    Serial2.println("NRZI TDM Decode V3 (Clean Arch)");

    ppsTimer.setOverflow(pps_period_smooth, MICROSEC_FORMAT);
    ppsTimer.attachInterrupt(ppsOutCallback);
    ppsTimer.resume();

    todUartTimer.setOverflow(TOD_BIT_US, MICROSEC_FORMAT);
    todUartTimer.attachInterrupt(todUartCallback);
    todUartTimer.pause();
    TOD_OUT_HIGH();
}

void loop() {
    // --- 非阻塞管理PPS输出脉冲宽度 ---
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
    static decode_state_t state = SYNC_SEARCH;
    static uint8_t sync_shift_reg = 0;
    static struct tdm_frame_t rx_frame;
    static uint8_t bit_buffer[152];
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
        uint8_t decoded_bit = (current_level == nrzi_last_level) ? 1 : 0;
        nrzi_last_level = current_level;

        switch (state) {
            case SYNC_SEARCH:
                sync_shift_reg = (sync_shift_reg << 1) | decoded_bit;
                if (sync_shift_reg == TDM_SYNC_PATTERN) {
                    state = FRAME_DECODE;
                    bit_count = 0;
                    next_sample_time = current_time + TDM_BIT_TIME_US;
                }
                break;

            case FRAME_DECODE:
                if (bit_count < 144) {
                    bit_buffer[bit_count] = decoded_bit;
                    bit_count++;
                }

                if (bit_count >= 144) {
                    for (uint8_t i = 0; i < 18; i++) {
                        uint8_t byte_val = 0;
                        for (uint8_t j = 0; j < 8; j++) {
                            byte_val |= (bit_buffer[i * 8 + j] << (7 - j));
                        }
                        ((uint8_t*)&rx_frame)[i + 1] = byte_val;
                    }
                    rx_frame.sync_header = TDM_SYNC_PATTERN;

                    uint8_t next_head = (frame_head + 1) % FRAME_BUFFER_SIZE;
                    if (next_head != frame_tail) {
                        memcpy(&frame_buffer[frame_head].frame, &rx_frame, sizeof(struct tdm_frame_t));
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

    // --- 独立的状态报告模块 ---
    static uint32_t last_report_time = 0;
    if (millis() - last_report_time > 2000) { // 每2秒报告一次
        last_report_time = millis();

        // 为保证数据一致性，在安全上下文中复制volatile变量
        noInterrupts();
        uint32_t frames_ok = frames_received;
        uint32_t frames_bad = frames_error;
        uint32_t seq_err = frames_seq_error;
        uint32_t pps_in_min = pps_in_min_dt;
        uint32_t pps_in_max = pps_in_max_dt;
        uint64_t pps_in_sum = pps_in_sum_dt;
        uint32_t pps_in_count = pps_in_stat_count;
        uint32_t pps_out_min = pps_out_min_dt;
        uint32_t pps_out_max = pps_out_max_dt;
        uint64_t pps_out_sum = pps_out_sum_dt;
        uint32_t pps_out_count = pps_out_stat_count;
        uint32_t pps_irqs = pps_irq_count;
        uint32_t buf_full = buffer_full_count;
        interrupts();

        uint32_t total_frames = frames_ok + frames_bad;
        uint32_t success_rate = (total_frames > 0) ? (frames_ok * 100) / total_frames : 0;

        Serial2.println("--- STATUS REPORT ---");
        Serial2.print("Frame RX OK: "); Serial2.print(frames_ok);
        Serial2.print(" | CRC Err: "); Serial2.print(frames_bad);
        Serial2.print(" | Seq Err: "); Serial2.print(seq_err);
        Serial2.print(" | Success: "); Serial2.print(success_rate); Serial2.println("%");
        Serial2.print("Buffer Full Drops: "); Serial2.println(buf_full);

        Serial2.print("PPS IRQ Count: "); Serial2.println(pps_irqs);

        if (pps_in_count > 0) {
            Serial2.print("PPS In (us):  min="); Serial2.print(pps_in_min);
            Serial2.print(" max="); Serial2.print(pps_in_max);
            Serial2.print(" avg="); Serial2.println((uint32_t)(pps_in_sum / pps_in_count));
        }
        if (pps_out_count > 0) {
            Serial2.print("PPS Out (us): min="); Serial2.print(pps_out_min);
            Serial2.print(" max="); Serial2.print(pps_out_max);
            Serial2.print(" avg="); Serial2.println((uint32_t)(pps_out_sum / pps_out_count));
        }
        Serial2.print("PPS Smooth Period (us): "); Serial2.println(pps_period_smooth);
        Serial2.println("---------------------");
    }
}

// process_frame: 帧处理函数 (消费者逻辑)，已移除所有打印
void process_frame(const struct tdm_frame_t* frame, uint32_t reception_time) {
    static uint8_t expected_frame_seq = 0;
    static uint8_t last_pps_bit = 0;

    uint8_t calc_crc = crc8((uint8_t*)&frame->frame_number, 17);
    if (calc_crc == frame->crc) {
        frames_received++;
        if (frames_received > 1 && frame->frame_number != expected_frame_seq) {
            frames_seq_error++;
        }
        expected_frame_seq = frame->frame_number + 1;

        uint8_t bit_pos = 0;
        for (uint8_t i = 0; i < 64; i++) {
            // TOD bit
            bit_pos++;
            // PPS bit
            uint8_t pps_bit = (frame->data[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1;
            digitalWrite(PIN_PPS_RAW, pps_bit);
            if (pps_bit == 1 && last_pps_bit == 0) {
                record_pps(reception_time);
            }
            last_pps_bit = pps_bit;
            bit_pos++;
        }
        start_tod_frame_transmission(frame->data);
    } else {
        frames_error++;
    }
}

// start_tod_frame_transmission: 启动TOD完整帧的非阻塞发送
void start_tod_frame_transmission(const uint8_t* data) {
    if (tod_tx_state != TOD_STATE_IDLE) {
        return;
    }
    memcpy(tod_tx_data, data, TOD_FRAME_BYTES);
    tod_tx_state = TOD_STATE_PREAMBLE;
    TOD_OUT_LOW();
    todUartTimer.setOverflow(TOD_PREAMBLE_MS * 1000, MICROSEC_FORMAT);
    todUartTimer.refresh();
    todUartTimer.resume();
}

// todUartCallback: TOD UART bit-bang输出定时器回调 (非阻塞状态机)
void todUartCallback() {
    if (tod_tx_state == TOD_STATE_PREAMBLE) {
        TOD_OUT_HIGH();
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
        const int bits_per_uart_char = 10;
        const int data_bits_total = TOD_FRAME_BYTES * bits_per_uart_char;
        const int padding_bits_total = TOD_PADDING_BYTES * bits_per_uart_char;
        uint8_t byte_to_send;
        int bit_in_uart_char;

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
            TOD_OUT_HIGH();
            tod_tx_state = TOD_STATE_IDLE;
            todUartTimer.pause();
            return;
        }

        if (bit_in_uart_char == 0) {
            TOD_OUT_LOW();
        } else if (bit_in_uart_char < 9) {
            if ((byte_to_send >> (bit_in_uart_char - 1)) & 0x01) {
                TOD_OUT_HIGH();
            } else {
                TOD_OUT_LOW();
            }
        } else {
            TOD_OUT_HIGH();
        }
        tod_tx_bit_counter++;
    }
}