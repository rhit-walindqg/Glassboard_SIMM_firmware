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
#define LINEAR_STAGE_THROW 250            // throw (in mm)
#define LINEAR_STAGE_LEADSCREW_LEAD 2     // lead (in mm/rev)
#define LINEAR_STAGE_MOTOR_STEP_ANGLE 1.8 // step angle (in deg/step)