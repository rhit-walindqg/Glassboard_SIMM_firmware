// ============================================================
//  stepper_control.h
//  Header file for declaring functions to drive the stepper motor on the linear stage.
// ============================================================

#pragma once
#include "types.h"
#include "config.h"
#include "types.h"
#include <stdint.h>
#include <stdbool.h>

// Define physical parameters about the linear stage and motor:
#define LINEAR_STAGE_THROW 253            // throw (in mm)
#define LINEAR_STAGE_LEADSCREW_LEAD 2     // lead (in mm/rev)
#define LINEAR_STAGE_MOTOR_STEP_ANGLE 1.8 // step angle (in deg/step)

// Define constants for the stepper motor driver
#define R_SENSE        0.11f        // BTT TMC2209 sense resistor — do not change
#define DRIVER_ADDRESS 0x00         // MS1/MS2 both LOW on the board
#define LINEAR_STAGE_MICROSTEPS 8   // Number of microsteps per normal step used

// Speeds — conservative for syringe/injection work
#define STEPPER_MAX_SPEED_MM_S      15.0f   // = 7500 steps/s at 1600 steps/mm
#define STEPPER_ACCEL_MM_S2         10.0f   // gentle acceleration value
#define STEPPER_HOMING_SPEED_MM_S    2.0f   // very slow for stall detection
#define STEPPER_INJECT_SPEED_MM_S    3.0f   // slow and controlled for injection
#define STEPPER_SYNC_SPEED_MM_S      3.0f   // coordinated moves

void stepper_init();

void stepper_set_speed(int32_t steps_per_sec);

void stepper_step(int steps, bool direction);

void stepper_home();

void stepper_update();

bool stepper_homingStatus();

void stepper_stopImmediately();

int stepper_getStepsPerMm();

int32_t stepper_getPosition();

float stepper_getPosition_mm();

int stepper_getPosition_mm_rounded();

void stepper_printDiagnostics();

void ISR_StepperLowerLimit();