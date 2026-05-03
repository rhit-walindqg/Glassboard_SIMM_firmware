#include <Arduino.h>
#include "pins.h"     
#include "config.h"      
#include "types.h"      
#include "stepper_control.h"

static int stepsPerRev = 360 / LINEAR_STAGE_MOTOR_STEP_ANGLE;
static float mmPerStep = LINEAR_STAGE_LEADSCREW_LEAD / LINEAR_STAGE_MOTOR_STEP_ANGLE;
