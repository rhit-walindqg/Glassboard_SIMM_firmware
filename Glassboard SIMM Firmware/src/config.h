#pragma once
 
// ============================================================
//  config.h
//  Project-wide settings, tuning values, and feature flags.
//  Adjust these without touching logic files.
// ============================================================
 

//// ---- Configuration Parameters for the Microcontroller ---- ////

// --- I2C Device Addresses ---
#define ADDR_1602LCD_I2C 0x27

// Analog-Digital Converter:
#define ADC_MAX 4095 // The current compiler allows for 10-bit ADC (1024 values) rather than the native 12-bit ADC on the Pico
#define ADC_MIN 0

// Serial Port Baud:
#define DBG_SERIAL_BAUD 9600

//// ---- Configuration Parameters for Specific Components ---- ////

// Injection Motor:
#define INJECTMOTOR_PWMLIMIT 100 // PWM Duty Cycle Speed Limit (between 0-255)

// Stepper Motor/Linear Stage:
#define LINEAR_STAGE_MAX_STEPS 1000 // Maximum number of steps the stepper motor can travel

// Status LCD:
#define STATUS_LCD_COLUMNS 16
#define STATUS_LCD_ROWS 2

// Cooling Fan:
#define COOLING_FAN_PWM_MAX 175 // PWM Duty Cycle Speed Limit (between 0-255)



//// ---- Configuration Parameters for Functions/Processes ---- ////

// Automatic Mixing Sequence
#define AUTOMIX_TIMER_STEPS 1000

// Definitions for Status Register flags:
extern volatile uint8_t statusRegister;
#define FLAG_MIXING_MOTOR_ENABLED    0b00000001 // Position 1: Mixing Motor Status
#define FLAG_INJECTION_MOTOR_ENABLED 0b00000010 // Position 2: Injection Motor Status
#define FLAG_MANUAL_INJECTION        0b00000100 // Position 3: Manual Injection Mode
#define FLAG_AUTOMATED_INJECTION     0b00001000 // Position 4: Automated Injection Sequence



//// ---- Feature flags (comment out to disable) ---- ////

#define ENABLE_SERIAL_DEBUG
// #define ENABLE_WATCHDOG
// #define ENABLE_SLEEP_MODE
 
// Debug helpers:
#ifdef ENABLE_SERIAL_DEBUG
  #define DBG_BEGIN         Serial.begin(DBG_SERIAL_BAUD)
  #define DBG_PRINT(x)      Serial.print(x)
  #define DBG_PRINTLN(x)    Serial.println(x)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
  #define DBG_PRINT_SUMMARY printDebugSummary()
#else
  #define DBG_BEGIN
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_PRINTF(...)
  #define DBG_PRINT_SUMMARY
#endif