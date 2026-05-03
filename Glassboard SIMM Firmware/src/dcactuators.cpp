#include <Arduino.h>
#include "pins.h"     
#include "config.h"      
#include "types.h"      
#include "dcactuators.h"


/*
  Function disableMotor:
  Description: Disables a given motor (1 - Mixing Motor, 2 - Injection Motor)
  Inputs:
    int motor - indicates which motor to disable (motor==1 -> Mixing Motor, motor==2 -> Injection Motor)
  Outputs:
    none
*/ 
void disableMotor(int motor) {

  if(motor == 1) {
    analogWrite(PIN_MIXMOTOR_PWM, 0);
    digitalWrite(PIN_MIXMOTOR_FWD, LOW);
    digitalWrite(PIN_MIXMOTOR_BKWD, LOW);
    statusRegister &= ~FLAG_MIXING_MOTOR_ENABLED;
  } else if (motor == 2) {
    analogWrite(PIN_INJECTMOTOR_PWM, 0);
    digitalWrite(PIN_INJECTMOTOR_FWD, LOW);
    digitalWrite(PIN_INJECTMOTOR_BKWD, LOW);
    statusRegister &= ~FLAG_INJECTION_MOTOR_ENABLED;
  }
  
}

/*
  Function driveMotor:
  Description: Drive a given motor based on a PWN value and direction
  Inputs:
    int speed - integer value (Between 0 - 255) for PWM signal to motor driver
    int direction - indicates motor direction (1 for forwards, 0 for backwards)
    int motor - indicates which motor to drive (motor==1 -> Mixing Motor, motor==2 -> Injection Motor)
  Outputs:
    none
*/
void driveMotor(int speed, int direction, int motor){


  if(motor == 1) {
    statusRegister |= FLAG_MIXING_MOTOR_ENABLED;
    statusRegister &= ~FLAG_INJECTION_MOTOR_ENABLED;
    digitalWrite(PIN_MIXMOTOR_FWD, direction);
    digitalWrite(PIN_MIXMOTOR_BKWD, !direction);
    analogWrite(PIN_MIXMOTOR_PWM, speed);
    disableMotor(2); // Disables "Motor 2" - Injection motor
  } else if(motor == 2) {
    statusRegister |= FLAG_INJECTION_MOTOR_ENABLED;
    statusRegister &= ~FLAG_MIXING_MOTOR_ENABLED;
    digitalWrite(PIN_INJECTMOTOR_FWD, direction);
    digitalWrite(PIN_INJECTMOTOR_BKWD, !direction);
    analogWrite(PIN_INJECTMOTOR_PWM, speed);
    disableMotor(1); // Disables "Motor 1" - Mixing motor
  } else {
    disableMotor(1); // Disables "Motor 1" - Mixing motor
    disableMotor(2); // Disables "Motor 2" - Injection motor
  }
}