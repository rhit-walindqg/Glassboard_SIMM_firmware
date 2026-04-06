#pragma once
 
// ============================================================
//  config.h
//  Project-wide settings, tuning values, and feature flags.
//  Adjust these without touching logic files.
// ============================================================
 
// --- I2C Device Addresses ---
#define ADDR_1602LCD_I2C 0x27

// Microcontroller Parameters
#define ADC_MAX 1023
#define ADC_MIN 0

// Injection Motor Speed Limit (between 0-255)
#define INJECTMOTOR_PWMLIMIT 100

// Automatic Mixing Sequence Parameters
#define AUTOMIX_TIMER_STEPS 1000

// Definitions for flags on status register
#define FLAG_MIXING_MOTOR_ENABLED    0b00000001 // Position 1: Mixing Motor Status
#define FLAG_INJECTION_MOTOR_ENABLED 0b00000010 // Position 2: Injection Motor Status
#define FLAG_MANUAL_INJECTION        0b00000100 // Position 3: Manual Injection Mode
#define FLAG_AUTOMATED_INJECTION     0b00001000 // Position 4: Automated Injection Sequence

// --- Feature flags (comment out to disable) ---
#define ENABLE_SERIAL_DEBUG
// #define ENABLE_WATCHDOG
// #define ENABLE_SLEEP_MODE
 
// --- Debug helpers ---
#ifdef ENABLE_SERIAL_DEBUG
  #define DBG_PRINT(x)    Serial.print(x)
  #define DBG_PRINTLN(x)  Serial.println(x)
  #define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_PRINTF(...)
#endif