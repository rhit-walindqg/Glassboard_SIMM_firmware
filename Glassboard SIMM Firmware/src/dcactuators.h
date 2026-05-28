// ============================================================
//  dcactuators.h
//  Motor 1 = Mixing motor
//  Motor 2 = Injection/plunger motor
//  Motor 3 = Magnet locator DC linear actuator
// ============================================================
#pragma once
#include "pins.h"
#include "config.h"
#include "types.h"
#include <stdint.h>
#include <stdbool.h>

void disableMotor(int motor);
void driveMotor(int speed, int direction, int motor);