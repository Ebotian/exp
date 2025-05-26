#include <Arduino.h>

// --- Pin Definitions (Blue Pill) ---
// Master Clock (CLK / M) output pin. Choose a pin with Timer PWM capability.
// PA8 (TIM1_CH1), PA6 (TIM3_CH1), PA7 (TIM3_CH2), PB0 (TIM3_CH3), PB1 (TIM3_CH4)
// PA0 (TIM2_CH1_ETR), PA1 (TIM2_CH2) etc.
// We'll use PA8 for TIM1_CH1 as an example.
const int MASTER_CLOCK_PIN = PA8;  // Changed from PinName to int
// Shift Gate (SH) output pin
const int SH_PIN = PB0; // Example: PB0
// Integration Clear Gate (ICG) output pin
const int ICG_PIN = PB1; // Example: PB1

// --- TCD1304 Timing Constants (Refer to Datasheet) ---
// Master Clock (M)
const float F_MASTER_CLOCK_HZ = 2000000.0f; // 2.0 MHz (Typical for VDD >= 4.0V)
const float MASTER_CLOCK_DUTY_CYCLE = 50.0f;  // 50%

// Integration Time (tINT) - User-defined
const uint32_t T_INT_MS = 10; // 10 milliseconds

// t2: Delay from ICG rising edge to SH rising edge
const uint32_t T2_ICG_SH_DELAY_NS = 500; // 500 ns (Typical)

// t3: SH pulse width
const uint32_t T3_SH_PULSE_WIDTH_NS = 1000; // 1000 ns = 1 µs (Minimum)

// Data Readout Time
const uint32_t NUM_TOTAL_PIXELS_READOUT = 3694; // Total elements to read out
const float F_DATA_RATE_HZ = 500000.0f;        // 0.5 MHz data rate (Typical)
const float T_DATA_PERIOD_US = (1.0f / F_DATA_RATE_HZ) * 1000000.0f; // Period of one pixel data
const uint32_t T_READOUT_US = (uint32_t)(NUM_TOTAL_PIXELS_READOUT * T_DATA_PERIOD_US); // e.g., 3694 * 2µs = 7388 µs

HardwareTimer *MasterClockTimer;

// DWT (Data Watchpoint and Trace) for precise nanosecond delays
#if (__CORTEX_M > 0U) && defined(ARDUINO_ARCH_STM32)
  #define DWT_CONTROL             (*(volatile uint32_t *)0xE0001000)
  #define DWT_CYCCNT              (*(volatile uint32_t *)0xE0001004)
  #define SCB_DEMCR               (*(volatile uint32_t *)0xE000EDFC)

  void DWT_Init(void) {
    SCB_DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable TRC
    DWT_CYCCNT = 0;                          // Reset cycle counter
    DWT_CONTROL |= DWT_CTRL_CYCCNTENA_Msk;   // Enable CYCCNT
  }

  // Delay for a number of system clock cycles
  inline void delay_cycles(uint32_t cycles) {
    uint32_t start_cycle = DWT_CYCCNT;
    while ((DWT_CYCCNT - start_cycle) < cycles);
  }

  // Delay in nanoseconds
  void delay_ns(uint32_t ns) {
    if (ns == 0) return;
    // Calculate cycles, ensure SystemCoreClock is correctly defined by STM32Duino
    uint32_t cycles = (uint32_t)(((uint64_t)ns * SystemCoreClock) / 1000000000ULL);
    if (cycles == 0 && ns > 0) cycles = 1; // Ensure at least a small delay
    delay_cycles(cycles);
  }
#else
  // Fallback if DWT is not available (less precise for ns)
  void DWT_Init(void) { /* Do nothing */ }
  void delay_ns(uint32_t ns) {
    if (ns == 0) return;
    if (ns < 1000) { // Very rough approximation for short ns delays
        for (volatile uint32_t i = 0; i < (ns / 100); ++i) { __NOP(); } // Adjust divisor based on clock
    } else {
        delayMicroseconds(ns / 1000);
    }
  }
#endif


void setup_master_clock_pwm(int pwm_pin, float frequency_hz, float duty_cycle_percent) {
  // Convert pin number to PinName for internal use
  PinName pin_name = digitalPinToPinName(pwm_pin);

  TIM_TypeDef *Instance = (TIM_TypeDef *)pinmap_peripheral(pin_name, PinMap_PWM);
  if (Instance == nullptr) {
    Serial.println("Error: PWM pin is not valid or timer not found for master clock.");
    while(1);
  }
  uint32_t channel = STM_PIN_CHANNEL(pinmap_function(pin_name, PinMap_PWM));
  if (channel == 0) {
     Serial.println("Error: Could not find PWM channel for master clock pin.");
     while(1);
  }

  MasterClockTimer = new HardwareTimer(Instance);
  MasterClockTimer->setMode(channel, TIMER_OUTPUT_COMPARE_PWM1, pin_name);
  MasterClockTimer->setPrescaleFactor(1); // Adjust if needed for very high/low frequencies
  MasterClockTimer->setOverflow(frequency_hz, HERTZ_FORMAT);

  // Calculate duty cycle value manually since PERCENT_FORMAT is not available
  uint32_t period = MasterClockTimer->getOverflow();
  uint32_t compare_value = (uint32_t)((duty_cycle_percent / 100.0f) * period);
  MasterClockTimer->setCaptureCompare(channel, compare_value);

  MasterClockTimer->resume();
  Serial.print("Master Clock PWM initialized on pin ");
  Serial.print(pwm_pin);
  Serial.print(" at ");
  Serial.print(frequency_hz / 1000000.0, 2);
  Serial.println(" MHz");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Wait for serial port to connect
  Serial.println("TCD1304 Signal Generator Starting...");

  DWT_Init(); // Initialize DWT for precise delays

  // Initialize GPIO pins for SH and ICG as outputs
  pinMode(SH_PIN, OUTPUT);
  pinMode(ICG_PIN, OUTPUT);
  digitalWrite(SH_PIN, LOW);
  digitalWrite(ICG_PIN, LOW); // Start with ICG low initially (or high, depending on desired initial state)

  // Setup Master Clock (CLK / M)
  setup_master_clock_pwm(MASTER_CLOCK_PIN, F_MASTER_CLOCK_HZ, MASTER_CLOCK_DUTY_CYCLE);

  Serial.println("Setup complete. Starting signal generation loop.");
  Serial.print("tINT: "); Serial.print(T_INT_MS); Serial.println(" ms");
  Serial.print("t2 (ICG-SH delay): "); Serial.print(T2_ICG_SH_DELAY_NS); Serial.println(" ns");
  Serial.print("t3 (SH width): "); Serial.print(T3_SH_PULSE_WIDTH_NS); Serial.println(" ns");
  Serial.print("tReadout: "); Serial.print(T_READOUT_US); Serial.println(" us");
}

void loop() {
  // --- Start of Integration ---
  digitalWrite(ICG_PIN, LOW); // 1. ICG LOW: Start integration
  delay(T_INT_MS);            // 2. Wait for tINT (integration time)

  // --- End of Integration & Charge Transfer ---
  digitalWrite(ICG_PIN, HIGH); // 3. ICG HIGH: End integration
  delay_ns(T2_ICG_SH_DELAY_NS); // 4. Wait for t2

  digitalWrite(SH_PIN, HIGH);   // 5. SH HIGH: Start charge transfer
  delay_ns(T3_SH_PULSE_WIDTH_NS); // 6. Wait for t3 (SH pulse width)
  digitalWrite(SH_PIN, LOW);    // 7. SH LOW: End charge transfer

  // --- Data Readout Period ---
  // During this time, M is clocking, and ADC would sample OS.
  // We just need to wait for the readout to complete before the next cycle.
  delayMicroseconds(T_READOUT_US); // 8. Wait for readout time

  // The loop repeats, ICG will go LOW again.
}