#include <Arduino.h>
#include "pins.h"     
#include "config.h"      
#include "types.h"      
#include "stepper_control.h"

static int stepsPerRev = (360 / LINEAR_STAGE_MOTOR_STEP_ANGLE) * LINEAR_STAGE_MICROSTEPS;
static int stepsPerMm = stepsPerRev / LINEAR_STAGE_LEADSCREW_LEAD;
volatile bool isHoming = false;
volatile bool atHome = false;
static volatile bool limit_switch_triggered = false;
// static float mmPerStep = LINEAR_STAGE_LEADSCREW_LEAD / (LINEAR_STAGE_MICROSTEPS * LINEAR_STAGE_MOTOR_STEP_ANGLE);

// Variables for motion state
static volatile int32_t stepsRemaining = 0;
static volatile bool    stepDir        = true;
static volatile uint32_t lastStepTime  = 0;
static volatile bool    moving         = false;

// Step interval in microseconds — derived from STEPPER_MAX_SPEED_MM_S
// Can make a separate homing speed by changing this before stepper_home()
static uint32_t stepInterval_us = 1000000 / (STEPPER_MAX_SPEED_MM_S * stepsPerMm);
static uint32_t homingInterval_us = 1000000 / (STEPPER_HOMING_SPEED_MM_S * stepsPerMm);

static int32_t currentPosition = 1000;  // in steps from home

static bool continuousMode = false; // different behaviors whether it's being run manually or automatically

void stepper_init() {
    pinMode(PIN_STEPPER_STEP, OUTPUT);
    pinMode(PIN_STEPPER_DIR,  OUTPUT);
    pinMode(PIN_STEPPER_EN,   OUTPUT);
    digitalWrite(PIN_STEPPER_EN, LOW);
    digitalWrite(PIN_STEPPER_STEP, LOW);
    digitalWrite(PIN_STEPPER_DIR,  LOW);
}

void stepper_set_speed(int32_t steps_per_sec) {
    // Zero stops the motor
    if (steps_per_sec == 0) {
        stepper_stopImmediately();
        continuousMode = false;
        return;
    }
    
    continuousMode  = true;
    stepDir         = steps_per_sec > 0;
    stepInterval_us = 1000000 / abs(steps_per_sec);
    digitalWrite(PIN_STEPPER_DIR, stepDir ? HIGH : LOW);
    delayMicroseconds(2);
    moving = true;
}

void stepper_move(int32_t steps, bool direction) {
    stepDir        = direction;
    stepsRemaining = abs(steps);
    moving         = true;
    digitalWrite(PIN_STEPPER_DIR, direction ? HIGH : LOW);
    delayMicroseconds(2); // direction setup time
}

void stepper_move_mm(float mm) {
    int32_t steps = (int32_t)(mm * stepsPerMm);
    stepper_move(abs(steps), steps >= 0);
}

void stepper_home() {
    isHoming = true;
    atHome   = false;
    // Drive in negative direction at homing speed
    stepInterval_us = homingInterval_us;
    stepper_move(9999, false); // large number — ISR will stop it
}

void stepper_stopImmediately() {
    stepsRemaining = 0;
    moving = false;
}

void stepper_update() {
    if (limit_switch_triggered) {
        limit_switch_triggered = false;
        stepper_stopImmediately();
        
        if (isHoming) {
            // Only zero position during a real homing sequence
            currentPosition = 0;
            isHoming   = false;
            atHome     = true;
            stepInterval_us = 1000000 / (STEPPER_MAX_SPEED_MM_S * stepsPerMm);
            continuousMode  = false;
            DBG_PRINTLN("Homed");
        } else {
            // Hit limit during normal operation — just stop, don't reset position
            DBG_PRINTLN("Lower limit hit");
        }
        return;
    }

    if (!moving) return;

    // In continuous mode, never runs out of steps
    if (!continuousMode && stepsRemaining == 0) {
        moving = false;
        return;
    }

    // Software limit — prevent moving below home position
    if (!stepDir && currentPosition <= 0) {
        stepper_stopImmediately();
        return;
    } else if (stepDir && currentPosition >= LINEAR_STAGE_THROW * stepsPerMm) {
        stepper_stopImmediately();
        return;
    }

    

    uint32_t now = micros();
    if (now - lastStepTime >= stepInterval_us) {
        lastStepTime = now;
        digitalWrite(PIN_STEPPER_STEP, HIGH);
        delayMicroseconds(2);
        digitalWrite(PIN_STEPPER_STEP, LOW);
        currentPosition += stepDir ? 1 : -1;
        if (!continuousMode) stepsRemaining--;
    }
}

bool stepper_homingStatus() {
    return atHome;
}

bool stepper_isMoving()     { 
    return moving; 
}

int32_t stepper_getPosition() { 
    return currentPosition; 
}

float stepper_getPosition_mm() { 
    return (float)currentPosition / stepsPerMm; 
}

int stepper_getPosition_mm_rounded() { 
    return currentPosition / stepsPerMm; 
}

int stepper_getStepsPerMm() { 
    return stepsPerMm; 
}

void stepper_printDiagnostics() {
    Serial.print("stepsPerRev: "); Serial.println(stepsPerRev);
    Serial.print("stepsPerMm: ");  Serial.println(stepsPerMm);
    Serial.print("stepInterval_us: "); Serial.println(stepInterval_us);
    Serial.print("homingInterval_us: "); Serial.println(homingInterval_us);
}

void ISR_StepperLowerLimit() {
    static uint32_t lastTriggerTime = 0;
    uint32_t now = millis();
    
    // Ignore triggers within 50ms of the last one
    if (now - lastTriggerTime < 50) return;
    lastTriggerTime = now;

    stepper_stopImmediately();
    limit_switch_triggered = true;
}

