// ============================================================
//  dcactuators.h
//  Header file for declaring functions to drive the linear actuators
// ============================================================

#pragma once
#include "pins.h"
#include "config.h"
#include "types.h"
#include <stdint.h>
#include <stdbool.h>

void disableMotor(int motor);

void driveMotor(int speed, int direction, int motor);