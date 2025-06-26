#include <Arduino.h>
#include <HardwareTimer.h>

// ==== 配置参数 ====
#define TDM_SYNC_PATTERN 0x5A
#define TDM_MAX_PAYLOAD 64
#define TDM_FRAME_BUF_SIZE 8
#define TOD_SEG_BUF_SIZE 8
#define PPS_EVT_BUF_SIZE 8
#define TOD_SEG_MAX_BITS 512

// ==== 结构体定义 ====
struct TODSegment {
    uint32_t start_timestamp; // us
    uint16_t duration_us;
    uint16_t bit_count;
    uint8_t  data[TOD_SEG_MAX_BITS / 8];
    uint8_t  valid;
};

struct PPSEvent {
    uint32_t timestamp; // us
    uint8_t  is_rising;
    uint8_t  valid;
};

struct SmartTDMFrame {
    uint8_t  sync_header;      // 0x5A
    uint8_t  frame_type;       // 0x01:TOD, 0x02:PPS, 0x03:混合
    uint16_t sequence;
    uint32_t timestamp;
    uint16_t payload_length;
    uint8_t  payload[TDM_MAX_PAYLOAD];
    uint8_t  crc;
} __attribute__((packed));

// ==== 环形缓冲区 ====
struct {
    TODSegment buf[TOD_SEG_BUF_SIZE];
    uint8_t head, tail, count;
} tod_seg_queue = {0};

struct {
    PPSEvent buf[PPS_EVT_BUF_SIZE];
    uint8_t head, tail, count;
} pps_evt_queue = {0};

static uint16_t frame_seq = 0;

// ==== CRC8 ====
uint8_t crc8(const uint8_t* data, uint16_t len) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
    return crc;
}

// ==== NRZI编码 ====
void nrzi_encode(const uint8_t* in, uint16_t in_bits, uint8_t* out, uint16_t* out_bits) {
    uint8_t last = 1, bit, out_bit = 0, out_byte = 0, out_idx = 0;
    for (uint16_t i = 0; i < in_bits; ++i) {
        bit = (in[i / 8] >> (i % 8)) & 1;
        if (bit) last = !last;
        if (last) out_byte |= (1 << (out_bit));
        if (++out_bit == 8) {
            out[out_idx++] = out_byte;
            out_byte = 0; out_bit = 0;
        }
    }
    if (out_bit) out[out_idx++] = out_byte;
    *out_bits = in_bits;
}

// ==== TOD采样与智能截断 ====
#define TOD_IN_PIN   PB3
#define PPS_IN_PIN   PB4
#define TDM_OUT_PIN  PB0

void tod_sample_handler(uint8_t tod_in, uint32_t timestamp) {
    static TODSegment seg = {0};
    static uint8_t idle_cnt = 0;

    if (!tod_in) { // 低电平采样
        if (!seg.valid) {
            seg.start_timestamp = timestamp;
            seg.bit_count = 0;
            memset(seg.data, 0, sizeof(seg.data));
            seg.valid = 1;
        }
        uint16_t idx = seg.bit_count / 8, bit = seg.bit_count % 8;
        if (idx < sizeof(seg.data)) {
            if (tod_in) seg.data[idx] |= (1 << bit);
            seg.bit_count++;
        }
        idle_cnt = 0;
    } else { // 高电平
        if (seg.valid) idle_cnt++;
        if (idle_cnt > 200) { // 200ms空闲截断
            seg.duration_us = timestamp - seg.start_timestamp;
            // 入队
            if (tod_seg_queue.count < TOD_SEG_BUF_SIZE) {
                tod_seg_queue.buf[tod_seg_queue.tail] = seg;
                tod_seg_queue.tail = (tod_seg_queue.tail + 1) % TOD_SEG_BUF_SIZE;
                tod_seg_queue.count++;
            }
            seg.valid = 0;
            idle_cnt = 0;
        }
    }
}

// ==== PPS采样 ====
void pps_sample_handler(uint8_t pps_in, uint32_t timestamp) {
    static uint8_t last = 0;
    if (pps_in != last) {
        PPSEvent evt = {timestamp, pps_in, 1};
        if (pps_evt_queue.count < PPS_EVT_BUF_SIZE) {
            pps_evt_queue.buf[pps_evt_queue.tail] = evt;
            pps_evt_queue.tail = (pps_evt_queue.tail + 1) % PPS_EVT_BUF_SIZE;
            pps_evt_queue.count++;
        }
    }
    last = pps_in;
}

// ==== 硬件定时器相关 ====
#define TDM_TIMER_NUM 1
#define TDM_BIT_TIME_US 500

static HardwareTimer tdmTimer(TIM1);
static volatile const uint8_t* tdm_bitstream = nullptr;
static volatile uint16_t tdm_bit_len = 0;
static volatile uint16_t tdm_bit_pos = 0;
static volatile bool tdm_sending = false;

void tdm_timer_callback() {
    if (!tdm_sending || tdm_bitstream == nullptr) return;
    if (tdm_bit_pos < tdm_bit_len) {
        uint8_t bit = (tdm_bitstream[tdm_bit_pos / 8] >> (tdm_bit_pos % 8)) & 1;
        digitalWrite(TDM_OUT_PIN, bit);
        tdm_bit_pos++;
    } else {
        tdm_sending = false;
        tdmTimer.pause();
        digitalWrite(TDM_OUT_PIN, LOW); // 空闲态
    }
}

void start_tdm_send_hw(const uint8_t* bits, uint16_t bitlen) {
    tdm_bitstream = bits;
    tdm_bit_len = bitlen;
    tdm_bit_pos = 0;
    tdm_sending = true;
    tdmTimer.setOverflow(TDM_BIT_TIME_US, MICROSEC_FORMAT);
    tdmTimer.attachInterrupt(tdm_timer_callback);
    tdmTimer.resume();
}

// ==== 统计变量 ====
static uint32_t stat_frame_cnt = 0;
static uint32_t stat_tod_cnt = 0;
static uint32_t stat_pps_cnt = 0;
static uint32_t stat_idle_cnt = 0;
static uint32_t stat_last_report = 0;

// ==== 帧组装与发送（1:2拼接版） ====
void send_tdm_frame() {
    SmartTDMFrame frame = {0};
    frame.sync_header = TDM_SYNC_PATTERN;
    frame.sequence = frame_seq++;
    frame.timestamp = micros();
    uint8_t* pl = frame.payload;
    uint16_t pl_len = 0;
    bool has_tod1 = false, has_tod2 = false, has_pps = false;

    // 取第1个TOD片段
    if (tod_seg_queue.count && (pl_len + sizeof(TODSegment) <= TDM_MAX_PAYLOAD)) {
        TODSegment* seg = &tod_seg_queue.buf[tod_seg_queue.head];
        memcpy(pl + pl_len, seg, sizeof(TODSegment));
        pl_len += sizeof(TODSegment);
        tod_seg_queue.head = (tod_seg_queue.head + 1) % TOD_SEG_BUF_SIZE;
        tod_seg_queue.count--;
        stat_tod_cnt++;
        has_tod1 = true;
    }
    // 取第2个TOD片段
    if (tod_seg_queue.count && (pl_len + sizeof(TODSegment) <= TDM_MAX_PAYLOAD)) {
        TODSegment* seg = &tod_seg_queue.buf[tod_seg_queue.head];
        memcpy(pl + pl_len, seg, sizeof(TODSegment));
        pl_len += sizeof(TODSegment);
        tod_seg_queue.head = (tod_seg_queue.head + 1) % TOD_SEG_BUF_SIZE;
        tod_seg_queue.count--;
        stat_tod_cnt++;
        has_tod2 = true;
    }
    // 取一个PPS事件
    if (pps_evt_queue.count && (pl_len + sizeof(PPSEvent) <= TDM_MAX_PAYLOAD)) {
        PPSEvent* evt = &pps_evt_queue.buf[pps_evt_queue.head];
        memcpy(pl + pl_len, evt, sizeof(PPSEvent));
        pl_len += sizeof(PPSEvent);
        pps_evt_queue.head = (pps_evt_queue.head + 1) % PPS_EVT_BUF_SIZE;
        pps_evt_queue.count--;
        stat_pps_cnt++;
        has_pps = true;
    }
    // 若无数据则空闲帧
    if (pl_len == 0) {
        frame.frame_type = 0x00;
        stat_idle_cnt++;
    } else if ((has_tod1 || has_tod2) && has_pps) {
        frame.frame_type = 0x03; // 混合帧
    } else if (has_tod1 || has_tod2) {
        frame.frame_type = 0x01;
    } else if (has_pps) {
        frame.frame_type = 0x02;
    }

    frame.payload_length = pl_len;
    frame.crc = crc8((uint8_t*)&frame, sizeof(SmartTDMFrame) - 1);

    // 调试输出帧头信息
    Serial.print("[TDM FRAME] sync_header: 0x"); Serial.print(frame.sync_header, HEX);
    Serial.print(" frame_type: 0x"); Serial.print(frame.frame_type, HEX);
    Serial.print(" sequence: 0x"); Serial.print(frame.sequence, HEX);
    Serial.print(" timestamp: 0x"); Serial.print(frame.timestamp, HEX);
    Serial.print(" payload_length: 0x"); Serial.print(frame.payload_length, HEX);
    Serial.print(" crc: 0x"); Serial.println(frame.crc, HEX);

    // NRZI编码
    uint8_t nrzi_buf[128] = {0};
    uint16_t nrzi_bits = 0;
    nrzi_encode((uint8_t*)&frame, 8 * (sizeof(SmartTDMFrame) - (TDM_MAX_PAYLOAD - pl_len)), nrzi_buf, &nrzi_bits);

    // 用硬件定时器逐bit输出
    start_tdm_send_hw(nrzi_buf, nrzi_bits);

    stat_frame_cnt++;
}

void print_stat_report() {
    Serial.print("[TDM STAT] frames: "); Serial.print(stat_frame_cnt);
    Serial.print(" | TOD: "); Serial.print(stat_tod_cnt);
    Serial.print(" | PPS: "); Serial.print(stat_pps_cnt);
    Serial.print(" | idle: "); Serial.print(stat_idle_cnt);
    Serial.print(" | tod_q: "); Serial.print(tod_seg_queue.count);
    Serial.print(" | pps_q: "); Serial.print(pps_evt_queue.count);
    Serial.print(" | last_seq: "); Serial.print(frame_seq-1);
    Serial.println();
    stat_frame_cnt = 0;
    stat_tod_cnt = 0;
    stat_pps_cnt = 0;
    stat_idle_cnt = 0;
}

void setup() {
    pinMode(TOD_IN_PIN, INPUT);
    pinMode(PPS_IN_PIN, INPUT);
    pinMode(TDM_OUT_PIN, OUTPUT);
    digitalWrite(TDM_OUT_PIN, LOW);
    Serial.begin(115200);
    tdmTimer.pause();
}

void loop() {
    static uint32_t last_send = 0;
    uint8_t tod_in = digitalRead(TOD_IN_PIN);
    uint8_t pps_in = digitalRead(PPS_IN_PIN);
    uint32_t ts = micros();
    tod_sample_handler(tod_in, ts);
    pps_sample_handler(pps_in, ts);
    if (!tdm_sending && ts - last_send > 1000) { // 1ms一帧
        send_tdm_frame();
        last_send = ts;
    }
    // 汇总统计输出，每2秒
    if (millis() - stat_last_report > 2000) {
        stat_last_report = millis();
        print_stat_report();
    }
}