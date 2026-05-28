// ============================================================
//  stepper_control.h
//  Header file for stepper motor / linear stage control.
//
//  Coordinate convention:
//    - Home / lower limit switch = 0 mm
//    - Positive position = gantry/stage moves upward away from home
//    - Limit switch is POLLED inside stepper_update(), not interrupt-driven
// ============================================================

#pragma once
#include "types.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>

// Physical parameters
#define LINEAR_STAGE_THROW             298     // max travel in mm
#define LINEAR_STAGE_LEADSCREW_LEAD      2     // mm per revolution
#define LINEAR_STAGE_MOTOR_STEP_ANGLE  1.8f    // degrees per full step
#define LINEAR_STAGE_MICROSTEPS          8     // microsteps; must match hardware

// Speed constants
#define STEPPER_MAX_SPEED_MM_S         5.0f
#define STEPPER_HOMING_SPEED_MM_S      3.0f
#define STEPPER_INJECT_SPEED_MM_S      3.0f
#define STEPPER_SYNC_SPEED_MM_S        3.0f

// ---------------------------------------------------------------------------
// Init / config
// ---------------------------------------------------------------------------
void stepper_init();

// ---------------------------------------------------------------------------
// Motion commands
// All commands are non-blocking. Call stepper_update() every loop.
// ---------------------------------------------------------------------------
void stepper_set_speed(int32_t steps_per_sec);   // continuous jog; + = up, - = down, 0 = stop
void stepper_move(int32_t steps, bool direction); // low-level finite move; direction true = up

void stepper_moveTo_mm(float target_mm);          // ABSOLUTE position from home/zero
void stepper_moveRelative_mm(float delta_mm);     // RELATIVE move from current position
void stepper_move_mm(float mm);                   // legacy wrapper; same as stepper_moveRelative_mm()

void stepper_home();                              // continuous downward move until lower switch
void stepper_stopImmediately();

// ---------------------------------------------------------------------------
// Must be called every loop(). Handles stepping, limit switch polling, homing
// completion detection, and software travel limits.
// ---------------------------------------------------------------------------
void stepper_update();

// ---------------------------------------------------------------------------
// Status queries
// ---------------------------------------------------------------------------
bool    stepper_homingStatus();          // true once homing is complete
bool    stepper_isMoving();
bool    stepper_atTarget();              // true after moveTo/moveRelative/move_mm completes
int32_t stepper_getPosition();           // steps from home
float   stepper_getPosition_mm();
int     stepper_getPosition_mm_rounded();
int     stepper_getStepsPerMm();

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------
void stepper_printDiagnostics();

// ---------------------------------------------------------------------------
// ISR — kept for compatibility only. Do not attach this while using the polled
// homing/limit-switch implementation.
// ---------------------------------------------------------------------------
void ISR_StepperLowerLimit();
