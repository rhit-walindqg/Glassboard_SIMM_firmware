// ============================================================
//  cooling.h
//  Header file for declaring functions to drive the cooling fan
// ============================================================

#pragma once
#include "pins.h"
#include "config.h"
#include "types.h"
#include <stdint.h>
#include <stdbool.h>

// Define PWM values for fan speeds
#define FANSPEED_OFF      0
#define FANSPEED_IDLING   124
#define FANSPEED_COOLING  255

// Declare functions for cooling fan control

void setCoolingLevel(int coolingLevel);