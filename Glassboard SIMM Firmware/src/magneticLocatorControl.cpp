#include <Arduino.h>
#include <Servo.h>
#include "pins.h"
#include "config.h"
#include "types.h"
#include "cooling.h"
#include "magneticLocatorControl.h"

Servo magneticLocator;

void initializeMagneticLocatorController(){
    // Magnet locator servo
    magneticLocator.attach(PIN_MAGNETMOTOR_PWM);
}

// ---------------------------------------------------------------------------
// moveMagneticLocator
// ---------------------------------------------------------------------------
void moveMagneticLocator(int speed){
    magneticLocator.write(speed);
}