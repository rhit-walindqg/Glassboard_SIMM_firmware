#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal.h>

/*
  Project: Control panel firmware for Glassboard Prototype Silicone
           Injection Molding Machine
  Description: Code for automated control of linear actuator and DC 
               mixing motor for mixing and injecting two-part silicone
               into prototype molds.
  Microcontroller: Raspberry Pi Pico RP2040
  Author: Quinlan Walinder
  Version: 0.1
  Version Notes:
    - V0.1: 
*/

// Define Status Register:
volatile uint8_t statusRegister = 0;
#define FLAG_MIXING_MOTOR_ENABLED    0b00000001 // Position 1: Mixing Motor Status
#define FLAG_INJECTION_MOTOR_ENABLED 0b00000010 // Position 2: Injection Motor Status
#define FLAG_MANUAL_INJECTION        0b00000100 // Position 3: Manual Injection Mode



// Microcontroller Parameters
#define ADC_MAX 1023
#define ADC_MIN 0

// Injection Motor Speed Limit (between 0-255)
#define INJECTMOTOR_PWMLIMIT 100

// Pin Declarations for speed input using joystick:
#define PIN_JOYSTICK A0 // Must be analog input

// Pin Declarations for DC Motor Control:
#define PIN_MIXMOTOR_PWM 7
#define PIN_MIXMOTOR_FWD 6
#define PIN_MIXMOTOR_BKWD 5
#define PIN_INJECTMOTOR_PWM 4
#define PIN_INJECTMOTOR_FWD 3
#define PIN_INJECTMOTOR_BKWD 2

// Pin Declarations for Input Buttons
#define PIN_SWITCH_MODESELECT 13
#define PIN_BUTTON_AUTOINJECT 12
#define PIN_BUTTON_AUTOMIX 11

// Pin Declarations for Thermistor Reading:
#define PIN_THERMISTOR 000 // Must be analog input

// Pin Declarations for Status LCD 
#define LCD_D4 18
#define LCD_D5 19
#define LCD_D6 20
#define LCD_D7 21
#define LCD_RS 16
#define LCD_EN 17

// Initialize Status LCD
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Variable Definitions for Manual Injection Mode
uint16_t joystickReading;
int pwmNoiseThreshold = 5; // Ignore speed values below this to account for noise in the joystick when manually injecting

// Variable Definitions for Automatic Mixing Mode
#define AUTOMIX_TIMER_STEPS 1000
#define PIN_MIX_SPEED_POTENTIOMETER A1
uint8_t mixSpeedSetVal; // PWM value for maximum mixing motor speed
uint8_t mixSpeed = 1; // PWM value for mixing motor speed - changed within autoMix();
volatile int autoMixTimer = 0;
int mixDir = 1;


// Function Declarations:
void checkStatus();
void setMixSpeed();
void autoMix();
void driveMotor(int speed, int direction, int motor);
void disableMotor(int motor);
void joystickDrivenInjection();
void updateLCD();


void setup() {

  // Start Initialization Screen on LCD
  lcd.begin(16, 2);
  lcd.print("  Initializing....  ");

  // Set input/output mode for motor driver pins:
  pinMode(PIN_MIXMOTOR_FWD, OUTPUT);
  pinMode(PIN_MIXMOTOR_BKWD, OUTPUT);
  pinMode(PIN_MIXMOTOR_PWM, OUTPUT);
  pinMode(PIN_INJECTMOTOR_FWD, OUTPUT);
  pinMode(PIN_INJECTMOTOR_BKWD, OUTPUT);
  pinMode(PIN_INJECTMOTOR_PWM, OUTPUT);

  // Disable motors until they are manually driven
  analogWrite(PIN_MIXMOTOR_PWM, 0);
  digitalWrite(PIN_MIXMOTOR_FWD, LOW);
  digitalWrite(PIN_MIXMOTOR_BKWD, LOW);
  analogWrite(PIN_INJECTMOTOR_PWM, 0);
  digitalWrite(PIN_INJECTMOTOR_FWD, LOW);
  digitalWrite(PIN_INJECTMOTOR_BKWD, LOW);

  // Set pin mode for analog pin
  pinMode(PIN_JOYSTICK, INPUT);

  // Set pin mode for control panel inputs
  pinMode(PIN_SWITCH_MODESELECT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_AUTOINJECT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_AUTOMIX, INPUT_PULLUP);

  // Clear LCD Screen
  delay(500);
  lcd.clear();
   
}

void loop() {
  
  ///// Set Status Register Flags /////

  // Check the "Mode Select" switch
  if(digitalRead(PIN_SWITCH_MODESELECT)){
    statusRegister |= FLAG_MANUAL_INJECTION; // Set manual injection flag
  } else {
    statusRegister &= ~FLAG_MANUAL_INJECTION; // Reset manual injection status flag
  }
  // Check the "Auto Mix" button
  if(!digitalRead(PIN_BUTTON_AUTOMIX)) {
    statusRegister |= FLAG_MIXING_MOTOR_ENABLED; // Set mixer status flag
  } else {
    statusRegister &= ~FLAG_MIXING_MOTOR_ENABLED; // Reset mixer status flag
  }

  // Check Status Register Flags 
  checkStatus();

  
  updateLCD();

  delay(10);
}


////////////////////////////////
///// FUNCTION DEFINITIONS /////
////////////////////////////////

/*
  Function checkStatus:
  Description: Checks status register and operates motors accordingly
  Inputs:
    none
  Outputs:
    none
*/ 
void checkStatus() {

  if(statusRegister & FLAG_MANUAL_INJECTION) {
    joystickDrivenInjection();
  } 
  if(statusRegister & FLAG_MIXING_MOTOR_ENABLED) {
    autoMix();
  } else if(autoMixTimer != 0 || mixSpeed != 1) {
    autoMixTimer = 0;
    mixSpeed = 1;
    disableMotor(1);
  } else {
    setMixSpeed();
  }
}

/*
  Function setMixSpeed:
  Description: Checks mixing speed potentiometer and sets maximum mixing speed
  Inputs:
    none
  Outputs:
    none
*/ 
void setMixSpeed() {

  uint16_t ADCVal = analogRead(PIN_MIX_SPEED_POTENTIOMETER);

  mixSpeedSetVal = map(ADCVal, ADC_MIN, ADC_MAX, 0, 255);

}

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

/*
  Function autoMix:
  Description: Controls the mixing motor
  Inputs:
    none
  Outputs:
    none
*/ 
void autoMix() {

  if(mixSpeed < mixSpeedSetVal && autoMixTimer == 0) {
    driveMotor(mixSpeed, mixDir, 1);
    delay(30);
    mixSpeed++;
  } else if(mixSpeed == mixSpeedSetVal && autoMixTimer < AUTOMIX_TIMER_STEPS) {
    driveMotor(mixSpeed, mixDir, 1);
    delay(10);
    autoMixTimer++;
  } else if(mixSpeed > 1 && autoMixTimer == AUTOMIX_TIMER_STEPS) {
    driveMotor(mixSpeed, mixDir, 1);
    delay(30);
    mixSpeed = mixSpeed - 1;
  } else {
    mixSpeed = 1; // Reset mixing speed
    autoMixTimer = 0; // Reset mixing timer
    if(mixDir == 1){ // Swap mixing direction
      mixDir = 0;
    } else {
      mixDir = 1;
    }
  }

}

/*
  Function joystickDrivenInjection:
  Description: Converts analog voltage read from joystick into a PWM signal for a DC motor controller
  Inputs:
    none
  Outputs:
    none
*/ 
void joystickDrivenInjection() {

  uint16_t ADCVal = analogRead(PIN_JOYSTICK);

  int PWMVal = map(ADCVal, ADC_MIN, ADC_MAX, (-1*INJECTMOTOR_PWMLIMIT), INJECTMOTOR_PWMLIMIT);
  
  if(PWMVal > 20){
    driveMotor(PWMVal, 1, 2);
  } else if (PWMVal < -20) {
    driveMotor((-1*PWMVal), 0, 2);
  } else {
    disableMotor(2);
  } 
}

/*
  Function updateLCD:
  Description: Updates LCD based on current status (read from statusEventFlags)
  Inputs:
    none
  Outputs:
    none
*/ 
void updateLCD() {

  // Update with status of injection motor and injection mode
  lcd.setCursor(0,0);
  lcd.print("INJ:");
  if(statusRegister & FLAG_INJECTION_MOTOR_ENABLED) {
    lcd.print("ON ");
  } else {
    lcd.print("OFF");
  }
  lcd.print("-MOD:");
  if(statusRegister & FLAG_MANUAL_INJECTION) {
    lcd.print("MANL");
  } else {
    lcd.print("AUTO");
  }

  // Update with status of mixing motor
  lcd.setCursor(0,1);
  lcd.print("MIX:");
  if(statusRegister & FLAG_MIXING_MOTOR_ENABLED) {
    lcd.print("ON ");
  } else {
    lcd.print("OFF");
  }  
  lcd.print("-SPD:");
  lcd.print(mixSpeedSetVal);
  lcd.print(" ");
}