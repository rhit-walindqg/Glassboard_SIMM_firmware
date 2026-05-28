#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "types.h"
#include "dcactuators.h"
#include "magneticLocatorControl.h"

// ---------------------------------------------------------------------------
// disableMotor
//   motor 1 = mixing motor
//   motor 2 = injection / plunger motor
//   motor 3 = magnet locator linear actuator
// ---------------------------------------------------------------------------
void disableMotor(int motor) {
    if (motor == 1) {
        analogWrite(PIN_MIXMOTOR_PWM, 0);
        digitalWrite(PIN_MIXMOTOR_FWD,  LOW);
        digitalWrite(PIN_MIXMOTOR_BKWD, LOW);
        statusRegister &= ~FLAG_MIXING_MOTOR_ENABLED;
    } else if (motor == 2) {
        analogWrite(PIN_INJECTMOTOR_PWM, 0);
        digitalWrite(PIN_INJECTMOTOR_FWD,  LOW);
        digitalWrite(PIN_INJECTMOTOR_BKWD, LOW);
        statusRegister &= ~FLAG_INJECTION_MOTOR_ENABLED;
    } else if (motor == 3) {
        // Magnet locator — uses PIN_MAGNETMOTOR_PWM as a simple PWM+dir output.
        moveMagneticLocator(90);
    }
}

// ---------------------------------------------------------------------------
// driveMotor
// ---------------------------------------------------------------------------
void driveMotor(int speed, int direction, int motor) {
    if (motor == 1) {
        statusRegister |= FLAG_MIXING_MOTOR_ENABLED;
        statusRegister &= ~FLAG_INJECTION_MOTOR_ENABLED;
        digitalWrite(PIN_MIXMOTOR_FWD,  direction);
        digitalWrite(PIN_MIXMOTOR_BKWD, !direction);
        analogWrite(PIN_MIXMOTOR_PWM, speed);
        disableMotor(2);
    } else if (motor == 2) {
        statusRegister |= FLAG_INJECTION_MOTOR_ENABLED;
        statusRegister &= ~FLAG_MIXING_MOTOR_ENABLED;
        digitalWrite(PIN_INJECTMOTOR_FWD,  direction);
        digitalWrite(PIN_INJECTMOTOR_BKWD, !direction);
        analogWrite(PIN_INJECTMOTOR_PWM, speed);
        disableMotor(1);
    } else if (motor == 3) {
        // Magnet locator — single PWM pin via Servo/PWM.
        moveMagneticLocator(speed);
    } else {
        disableMotor(1);
        disableMotor(2);
        disableMotor(3);
    }
}