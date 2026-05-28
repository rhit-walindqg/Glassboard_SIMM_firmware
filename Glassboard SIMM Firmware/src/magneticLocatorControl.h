// ============================================================
//  magneticLocator.h
//  Header file for controlling the motion of the magnetic locator
// ============================================================

#pragma once
#include "pins.h"
#include "config.h"
#include "types.h"
#include <stdint.h>
#include <stdbool.h>
#include <Servo.h>

// ---------------------------------------------------------------------------
// Magnet locator servo
// ---------------------------------------------------------------------------
extern Servo magneticLocator;

// Define PWM values for speeds
#define MAGNETICLOCATOR_STOP      90
#define MAGNETICLOCATOR_BKWDSPD   0
#define MAGNETICLOCATOR_FWDSPD    180

// Declare functions for control

void initializeMagneticLocatorController();
void moveMagneticLocator(int speed);