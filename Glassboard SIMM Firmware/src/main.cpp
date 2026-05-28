#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <Servo.h>

/*
  Project: Control panel firmware for Glassboard Prototype Silicone
           Injection Molding Machine
  Microcontroller: Raspberry Pi Pico RP2040
  Author: Quinlan Walinder
  Version: 0.3
*/

#include "pins.h"
#include "config.h"
#include "types.h"
#include "dcactuators.h"
#include "stepper_control.h"
#include "cooling.h"
#include "state_machine.h"
#include "magneticLocatorControl.h"


// ---------------------------------------------------------------------------
// LCD — also used by state_machine.cpp (extern declared there)
// ---------------------------------------------------------------------------
LiquidCrystal_PCF8574 lcd(ADDR_1602LCD_I2C);

// ---------------------------------------------------------------------------
// Mixing motor variables (manual mode)
// ---------------------------------------------------------------------------
uint8_t mixSpeedSetVal = 0;   // also used by state_machine.cpp (extern)
uint8_t mixSpeed       = 1;
volatile int autoMixTimer = 0;
int mixDir = 1;

// ---------------------------------------------------------------------------
// Status register
// ---------------------------------------------------------------------------
volatile uint8_t statusRegister = 0;

// ---------------------------------------------------------------------------
// Confirm button debounce
// ---------------------------------------------------------------------------
static bool     lastConfirmBtnState = HIGH;
static bool     confirmBtnState     = HIGH;
static uint32_t confirmDebounceMs   = 0;

// ---------------------------------------------------------------------------
// LCD throttle (manual mode only — auto mode writes on state transitions)
// ---------------------------------------------------------------------------
static uint32_t lastLcdUpdate = 0;

// ---------------------------------------------------------------------------
// Function declarations
// ---------------------------------------------------------------------------
void handleManualMode();
void joystickDrivenInjection();
void manualAutoMix();
void setMixSpeed();
void updateLCD();
void printDebugSummary();

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    DBG_BEGIN;
    delay(500);
    DBG_PRINTLN("Booting...");

    analogReadResolution(12);

    // I2C + LCD
    Wire.setSDA(0);
    Wire.setSCL(1);
    Wire.begin();
    Wire.beginTransmission(ADDR_1602LCD_I2C);
    lcd.begin(STATUS_LCD_COLUMNS, STATUS_LCD_ROWS);
    lcd.setBacklight(1);
    lcd.clear();
    lcd.print("  Initializing....");

    // Initialize Magnetic Locator
    initializeMagneticLocatorController();

    // Motor driver pins
    pinMode(PIN_MIXMOTOR_FWD,      OUTPUT);
    pinMode(PIN_MIXMOTOR_BKWD,     OUTPUT);
    pinMode(PIN_MIXMOTOR_PWM,      OUTPUT);
    pinMode(PIN_INJECTMOTOR_FWD,   OUTPUT);
    pinMode(PIN_INJECTMOTOR_BKWD,  OUTPUT);
    pinMode(PIN_INJECTMOTOR_PWM,   OUTPUT);

    analogWrite(PIN_MIXMOTOR_PWM,    0);
    analogWrite(PIN_INJECTMOTOR_PWM, 0);
    digitalWrite(PIN_MIXMOTOR_FWD,    LOW);
    digitalWrite(PIN_MIXMOTOR_BKWD,   LOW);
    digitalWrite(PIN_INJECTMOTOR_FWD,  LOW);
    digitalWrite(PIN_INJECTMOTOR_BKWD, LOW);

    // Control inputs
    pinMode(PIN_JOYSTICK,                       INPUT);
    pinMode(PIN_MIX_SPEED_POTENTIOMETER,        INPUT);
    pinMode(PIN_SWITCH_MODESELECT,              INPUT_PULLUP);
    pinMode(PIN_BUTTON_AUTOMIX,                 INPUT_PULLUP);
    pinMode(PIN_BUTTON_AUTOINJECT,              INPUT_PULLUP);
    pinMode(PIN_LINEARSTAGECONTROL_BUTTON,      INPUT_PULLUP);
    pinMode(PIN_MAGNETICLOCATORCONTROL_BUTTON,  INPUT_PULLUP);

    // Stepper
    stepper_init();

    // State machine
    sm_init();

    // Cooling — idle level
    setCoolingLevel(1);

    // Limit switch — polled, no interrupt needed
    pinMode(PIN_STEPPER_LIMITSWITCH, INPUT_PULLUP);
    // attachInterrupt(digitalPinToInterrupt(PIN_STEPPER_LIMITSWITCH), ISR_StepperLowerLimit, FALLING);

    delay(500);
    lcd.clear();

    // Home stepper
    lcd.print("Homing...");
    stepper_home();
    while (!stepper_homingStatus()) { stepper_update(); }
    lcd.clear();
    lcd.print("Homed.");
    delay(500);

    updateLCD();
    DBG_PRINTLN("Setup complete.");
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {

    // Stepper always first
    stepper_update();

    // --- Confirm button (PIN_BUTTON_AUTOINJECT) debounce ---
    bool rawConfirm = digitalRead(PIN_BUTTON_AUTOINJECT);
    if (rawConfirm != lastConfirmBtnState) {
        confirmDebounceMs   = millis();
        lastConfirmBtnState = rawConfirm;
    }
    if ((millis() - confirmDebounceMs) >= BTN_DEBOUNCE_MS) {
        if (rawConfirm != confirmBtnState) {
            confirmBtnState = rawConfirm;
            if (confirmBtnState == LOW) {   // active-low: LOW = pressed
                sm_confirmPressed();
                DBG_PRINTLN("Confirm pressed");
            }
        }
    }

    // --- State machine ---
    sm_update();

    // --- Manual mode handling ---
    if (sm_getMode() == SystemMode::MANUAL) {
        handleManualMode();

        // LCD update throttled — auto mode updates its own LCD on transitions
        if (millis() - lastLcdUpdate >= 150) {
            lastLcdUpdate = millis();
            updateLCD();
        }
    }
}

// ---------------------------------------------------------------------------
// handleManualMode
// ---------------------------------------------------------------------------
void handleManualMode() {
    joystickDrivenInjection();

    // Mixing motor: AUTOMIX button held AND in auto-mode position of switch
    if (!digitalRead(PIN_BUTTON_AUTOMIX) && digitalRead(PIN_SWITCH_MODESELECT)) {
        statusRegister |= FLAG_MIXING_MOTOR_ENABLED;
    } else {
        statusRegister &= ~FLAG_MIXING_MOTOR_ENABLED;
    }

    if (statusRegister & FLAG_MIXING_MOTOR_ENABLED) {
        manualAutoMix();
        setCoolingLevel(2);
    } else if (autoMixTimer != 0 || mixSpeed != 1) {
        autoMixTimer = 0;
        mixSpeed     = 1;
        disableMotor(1);
    } else {
        setMixSpeed();
    }

    if (!(statusRegister & FLAG_INJECTION_MOTOR_ENABLED) &&
        !(statusRegister & FLAG_MIXING_MOTOR_ENABLED)) {
        setCoolingLevel(1);
    }
}

// ---------------------------------------------------------------------------
// joystickDrivenInjection
//
// Button priority (active low, INPUT_PULLUP):
//   Neither button pressed        → joystick drives injection motor
//   PIN_LINEARSTAGECONTROL_BUTTON → joystick drives stepper stage
//   PIN_MAGNETICLOCATORCONTROL_BUTTON → joystick drives magnet locator servo
//   (If both pressed, injection motor takes priority)
// ---------------------------------------------------------------------------
void joystickDrivenInjection() {
    uint16_t ADCVal = analogRead(PIN_JOYSTICK);

    bool stageBtn  = !digitalRead(PIN_LINEARSTAGECONTROL_BUTTON);
    bool magnetBtn = !digitalRead(PIN_MAGNETICLOCATORCONTROL_BUTTON);

    if (stageBtn && !magnetBtn) {
        // Joystick → stepper stage
        int dutyCycle = map(ADCVal, ADC_MIN, ADC_MAX,
                            -stepper_getStepsPerMm() * 10,
                             stepper_getStepsPerMm() * 10);
        if (abs(dutyCycle) < 350) {
            stepper_set_speed(0);
        } else {
            stepper_set_speed(dutyCycle);
        }

    } else if (magnetBtn && !stageBtn) {
        // Joystick → magnet locator servo angle
        int angle = map(ADCVal, ADC_MIN, ADC_MAX, 0, 180);
        moveMagneticLocator(angle);
    } else {
        // Default: joystick → injection motor
        int PWMVal = map(ADCVal, ADC_MIN, ADC_MAX,
                         -INJECTMOTOR_PWMLIMIT, INJECTMOTOR_PWMLIMIT);
        if (PWMVal > 20) {
            driveMotor(PWMVal, 0, 2);
        } else if (PWMVal < -20) {
            driveMotor(abs(PWMVal), 1, 2);
        } else {
            disableMotor(2);
        }
    }
}

// ---------------------------------------------------------------------------
// manualAutoMix — non-blocking
// ---------------------------------------------------------------------------
void manualAutoMix() {
    static uint32_t lastMixStep = 0;
    uint32_t now = millis();

    if (mixSpeed < mixSpeedSetVal && autoMixTimer == 0) {
        if (now - lastMixStep >= 30) {
            lastMixStep = now;
            driveMotor(mixSpeed, mixDir, 1);
            mixSpeed++;
        }
    } else if (mixSpeed == mixSpeedSetVal && autoMixTimer < AUTOMIX_TIMER_STEPS) {
        if (now - lastMixStep >= 10) {
            lastMixStep = now;
            driveMotor(mixSpeed, mixDir, 1);
            autoMixTimer++;
        }
    } else if (mixSpeed > 1 && autoMixTimer == AUTOMIX_TIMER_STEPS) {
        if (now - lastMixStep >= 30) {
            lastMixStep = now;
            driveMotor(mixSpeed, mixDir, 1);
            mixSpeed--;
        }
    } else {
        mixSpeed     = 1;
        autoMixTimer = 0;
        mixDir       = (mixDir == 1) ? 0 : 1;
    }
}

// ---------------------------------------------------------------------------
// setMixSpeed
// ---------------------------------------------------------------------------
void setMixSpeed() {
    uint16_t ADCVal = analogRead(PIN_MIX_SPEED_POTENTIOMETER);
    mixSpeedSetVal  = (uint8_t)map(ADCVal, ADC_MIN, ADC_MAX, 0, 255);
}

// ---------------------------------------------------------------------------
// updateLCD — manual mode display
// ---------------------------------------------------------------------------
void updateLCD() {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("INJ:");
    lcd.print((statusRegister & FLAG_INJECTION_MOTOR_ENABLED) ? "ON " : "OFF");
    lcd.print(" STG:");
    lcd.print(stepper_getPosition_mm_rounded());
    lcd.print("mm");

    lcd.setCursor(0, 1);
    lcd.print("MIX:");
    if (statusRegister & FLAG_MIXING_MOTOR_ENABLED) {
        lcd.print("ON  SPD:");
        lcd.print(mixSpeedSetVal);
    } else {
        lcd.print("OFF");
    }
}

// ---------------------------------------------------------------------------
// printDebugSummary
// ---------------------------------------------------------------------------
void printDebugSummary() {
    DBG_PRINT("Mode: ");
    DBG_PRINTLN(sm_getMode() == SystemMode::MANUAL ? "MANUAL" : "AUTO");
    DBG_PRINT("AutoState: ");
    DBG_PRINTLN((uint8_t)sm_getState());
    DBG_PRINT("Stage pos mm: ");
    DBG_PRINTLN(stepper_getPosition_mm_rounded());
    DBG_PRINT("Confirm btn: ");
    DBG_PRINTLN(digitalRead(PIN_BUTTON_AUTOINJECT));
    delay(50);
}

