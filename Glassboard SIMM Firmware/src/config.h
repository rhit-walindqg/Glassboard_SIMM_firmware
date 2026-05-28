#pragma once

// ============================================================
//  config.h
// ============================================================

// I2C
#define ADDR_1602LCD_I2C    0x27

// ADC
#define ADC_MAX             4095
#define ADC_MIN             0

// Serial
#define DBG_SERIAL_BAUD     9600

// --- Injection motor (DC linear actuator / plunger) ---
#define INJECTMOTOR_PWMLIMIT        100     // max PWM, manual joystick mode
#define INJECTMOTOR_PURGE_SPEED     60      // PWM for slow purge descent
#define INJECTMOTOR_INJECT_SPEED    80      // PWM for injection
#define INJECTMOTOR_SYNC_SPEED      70      // PWM during synchronized lift/lower

// --- Magnet locator (DC linear actuator) ---
// Both moves are timed runs to a hard stop so exact timing just needs to be
// "long enough to guarantee the actuator reaches the stop". Tune these.
#define MAGNET_CLEAR_SPEED          150     // PWM to drive mold clear
#define MAGNET_BACK_SPEED           150     // PWM to drive mold back to injection port
#define MAGNET_CLEAR_DURATION_MS    3000    // time to reach "clear" hard stop
#define MAGNET_BACK_DURATION_MS     4000    // time to reach "back" hard stop
// Direction defines — adjust to match your wiring
#define MAGNET_DIR_CLEAR            1       // direction to move mold away from syringe
#define MAGNET_DIR_BACK             0       // direction to return mold to injection port

// --- Stepper / linear stage ---
// #define LINEAR_STAGE_MAX_STEPS      1000    // legacy

// Stage positions (mm from home/zero)
#define STAGE_POS_CAP_CLEARANCE     5.0f    // offset so cap clears during fixture setup
#define STAGE_POS_MIX               295.0f   // height at mixing motor
#define STAGE_POS_ZERO              0.0f

// Synchronized lift/lower distance (mm) — tune to give mold repositioning clearance
#define SYNC_LIFT_DISTANCE_MM       25.0f
// Sync step interval — both axes slow for reliability
// Stepper side uses STEPPER_SYNC_SPEED_MM_S from stepper_control.h
// Plunger side uses INJECTMOTOR_SYNC_SPEED PWM above

// --- Mixing ---
#define AUTOMIX_TIMER_STEPS         1000    // legacy loop-count (manual mode)
#define MIX_DURATION_MS             75000   // auto sequence mix duration (ms)
#define MIX_DIR_SWAP_INTERVAL_MS    3000    // reverse mix direction every N ms

// --- LCD ---
#define STATUS_LCD_COLUMNS  16
#define STATUS_LCD_ROWS      2

// --- Cooling fan ---
#define COOLING_FAN_PWM_MAX 175

// --- Button debounce ---
#define BTN_DEBOUNCE_MS         30
#define BTN_LONG_PRESS_MS       1000

// --- Limit switch polling debounce ---
#define LIMITSWITCH_DEBOUNCE_MS 50

// --- Status register flags (kept for dcactuators.cpp compatibility) ---
extern volatile uint8_t statusRegister;
#define FLAG_MIXING_MOTOR_ENABLED    0b00000001
#define FLAG_INJECTION_MOTOR_ENABLED 0b00000010
#define FLAG_MANUAL_INJECTION        0b00000100
#define FLAG_AUTOMATED_INJECTION     0b00001000

// --- Feature flags ---
#define ENABLE_SERIAL_DEBUG

#ifdef ENABLE_SERIAL_DEBUG
  #define DBG_BEGIN           Serial.begin(DBG_SERIAL_BAUD)
  #define DBG_PRINT(x)        Serial.print(x)
  #define DBG_PRINTLN(x)      Serial.println(x)
  #define DBG_PRINTF(...)     Serial.printf(__VA_ARGS__)
  #define DBG_PRINT_SUMMARY   printDebugSummary()
#else
  #define DBG_BEGIN
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_PRINTF(...)
  #define DBG_PRINT_SUMMARY
#endif