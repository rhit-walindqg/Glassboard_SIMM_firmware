#include <Arduino.h>
#include <LiquidCrystal_PCF8574.h>
#include "state_machine.h"
#include "stepper_control.h"
#include "dcactuators.h"
#include "cooling.h"
#include "config.h"
#include "pins.h"
#include "types.h"

// LCD owned by main.cpp
extern LiquidCrystal_PCF8574 lcd;

// mixSpeedSetVal owned by main.cpp — used by runMixStep()
extern uint8_t mixSpeedSetVal;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static AutoState  currentState   = AutoState::IDLE;
static SystemMode currentMode    = SystemMode::MANUAL;
static bool       confirmPending = false;

// ---------------------------------------------------------------------------
// Timed-operation helpers
// ---------------------------------------------------------------------------
static uint32_t stateStartTime = 0;  // set in transitionTo(), used by timed states

// ---------------------------------------------------------------------------
// Sync motion state (LIFT_FOR_CLEARANCE / LOWER_TO_INJECT)
// ---------------------------------------------------------------------------
static bool     syncMoving        = false;
static bool     syncDirectionUp   = true;
static uint32_t syncStartTime     = 0;

// Estimated time for the sync move based on stepper travel at sync speed.
// The plunger PWM runs for the same duration.
static uint32_t syncDuration_ms() {
    return (uint32_t)((SYNC_LIFT_DISTANCE_MM / STEPPER_SYNC_SPEED_MM_S) * 1000.0f);
}

static void syncStart(bool directionUp) {
    syncMoving      = true;
    syncDirectionUp = directionUp;
    syncStartTime   = millis();

    // Stepper: move SYNC_LIFT_DISTANCE_MM in the given direction
    stepper_moveRelative_mm(directionUp ? SYNC_LIFT_DISTANCE_MM : -SYNC_LIFT_DISTANCE_MM);

    // Plunger: run at sync speed in matching direction
    // Up = retract (direction 0), Down = extend (direction 1)
    driveMotor(INJECTMOTOR_SYNC_SPEED, directionUp ? 0 : 1, 2);
}

static bool syncUpdate() {
    if (!syncMoving) return true;
    stepper_update();
    // Use the longer of stepper completion or timeout
    bool timedOut = (millis() - syncStartTime >= syncDuration_ms() + 500);
    bool stepperDone = stepper_atTarget();
    if (stepperDone || timedOut) {
        syncMoving = false;
        disableMotor(2);
        stepper_stopImmediately();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Non-blocking mix
// ---------------------------------------------------------------------------
static uint8_t  mixSpeed_sm      = 1;
static int      mixDir_sm        = 1;
static uint32_t mixStartTime     = 0;
static uint32_t lastMixStep      = 0;
static uint32_t lastDirSwap      = 0;

static void mixReset() {
    mixSpeed_sm  = 1;
    mixDir_sm    = 1;
    mixStartTime = 0;
    lastMixStep  = 0;
    lastDirSwap  = 0;
}

// Returns true when mix duration is complete
static bool runMixStep() {
    uint32_t now = millis();
    if (mixStartTime == 0) mixStartTime = now;

    if (now - mixStartTime >= MIX_DURATION_MS) {
        disableMotor(1);
        mixSpeed_sm = 1;
        return true;
    }

    // Ramp up to target speed
    if (mixSpeed_sm < mixSpeedSetVal) {
        if (now - lastMixStep >= 30) {
            lastMixStep = now;
            driveMotor(mixSpeed_sm, mixDir_sm, 1);
            mixSpeed_sm++;
        }
    } else {
        // Hold at speed, swap direction periodically
        driveMotor(mixSpeed_sm, mixDir_sm, 1);
        if (now - lastDirSwap >= MIX_DIR_SWAP_INTERVAL_MS) {
            lastDirSwap = now;
            mixDir_sm   = (mixDir_sm == 1) ? 0 : 1;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// LCD prompt
// ---------------------------------------------------------------------------
static void lcdPrompt(const char* line1, const char* line2 = "") {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(line1);
    lcd.setCursor(0, 1); lcd.print(line2);
}

// ---------------------------------------------------------------------------
// transitionTo — sets state, records start time, shows LCD prompt
// ---------------------------------------------------------------------------
static void transitionTo(AutoState next) {
    currentState   = next;
    confirmPending = false;
    stateStartTime = millis();

    switch (next) {
        case AutoState::IDLE:
            lcdPrompt("AUTO MODE READY", "CONFIRM to start");
            break;
        case AutoState::HOMING:
            lcdPrompt("Homing...", "Please wait");
            break;
        case AutoState::RAISE_FOR_FIXTURE:
            lcdPrompt("Raising stage...", "");
            break;
        case AutoState::AWAIT_FIXTURE:
            lcdPrompt("Lock fixture &", "setup. CONFIRM");
            break;
        case AutoState::RAISE_FOR_MIX:
            lcdPrompt("Raising to", "mixer...");
            break;
        case AutoState::MIXING:
            lcdPrompt("Mixing...", "");
            mixReset();
            break;
        case AutoState::MOVE_MOLD_CLEAR:
            lcdPrompt("Moving mold", "clear...");
            // Start magnet locator toward "clear" hard stop
            driveMotor(MAGNET_CLEAR_SPEED, MAGNET_DIR_CLEAR, 3);
            break;
        case AutoState::LOWER_AFTER_MIX:
            lcdPrompt("Lowering...", "");
            break;
        case AutoState::PURGE:
            lcdPrompt("Purging...", "CONFIRM if done.");
            driveMotor(INJECTMOTOR_PURGE_SPEED, 1, 2);
            break;
        case AutoState::AWAIT_PURGE_DONE:
            lcdPrompt("Purge done?", "CONFIRM to cont.");
            disableMotor(2);
            break;
        case AutoState::LIFT_FOR_CLEARANCE:
            lcdPrompt("Lifting for", "clearance...");
            syncStart(true);
            break;
        case AutoState::MOVE_MOLD_BACK:
            lcdPrompt("Returning mold", "to inj. port...");
            // Start magnet locator toward "back" hard stop
            driveMotor(MAGNET_BACK_SPEED, MAGNET_DIR_BACK, 3);
            break;
        case AutoState::LOWER_TO_INJECT:
            lcdPrompt("Lowering to", "inject port...");
            syncStart(false);
            break;
        case AutoState::INJECT:
            lcdPrompt("Injecting...", "CONFIRM when done");
            driveMotor(INJECTMOTOR_INJECT_SPEED, 1, 2);
            break;
        case AutoState::AWAIT_INJECT_DONE:
            lcdPrompt("Injection done?", "CONFIRM to finish");
            disableMotor(2);
            break;
        case AutoState::COMPLETE:
            lcdPrompt("Complete!", "CONFIRM to reset");
            setCoolingLevel(1);
            break;
        case AutoState::ERROR:
            lcdPrompt("! ERROR !", "CONFIRM to reset");
            stepper_stopImmediately();
            disableMotor(1);
            disableMotor(2);
            disableMotor(3);
            break;
    }
}

// ---------------------------------------------------------------------------
// sm_init
// ---------------------------------------------------------------------------
void sm_init() {
    currentState = AutoState::IDLE;
    currentMode  = SystemMode::MANUAL;
}

void sm_confirmPressed() { confirmPending = true; }
AutoState  sm_getState() { return currentState; }
SystemMode sm_getMode()  { return currentMode;  }

// ---------------------------------------------------------------------------
// sm_update — call every loop()
// ---------------------------------------------------------------------------
void sm_update() {

    // --- Mode switch (always checked) ---
    SystemMode newMode = digitalRead(PIN_SWITCH_MODESELECT)
                         ? SystemMode::MANUAL
                         : SystemMode::AUTO;

    if (newMode != currentMode) {
        currentMode = newMode;
        if (currentMode == SystemMode::MANUAL) {
            // Abort sequence on mode switch
            stepper_stopImmediately();
            disableMotor(1);
            disableMotor(2);
            disableMotor(3);
            currentState = AutoState::IDLE;
            DBG_PRINTLN("SM: switched to MANUAL, sequence aborted");
        } else {
            transitionTo(AutoState::IDLE);
            DBG_PRINTLN("SM: switched to AUTO");
        }
    }

    if (currentMode == SystemMode::MANUAL) {
        confirmPending = false;
        return;
    }

    // ---------------------------------------------------------------------------
    // AUTO sequence dispatch
    // ---------------------------------------------------------------------------
    switch (currentState) {

        // --- IDLE ---
        case AutoState::IDLE:
            if (confirmPending) {
                confirmPending = false;
                transitionTo(AutoState::HOMING);
                stepper_home();
            }
            break;

        // --- HOMING ---
        case AutoState::HOMING:
            stepper_update();
            if (stepper_homingStatus()) {
                transitionTo(AutoState::RAISE_FOR_FIXTURE);
                stepper_moveTo_mm(STAGE_POS_CAP_CLEARANCE);
            }
            break;

        // --- RAISE_FOR_FIXTURE ---
        case AutoState::RAISE_FOR_FIXTURE:
            stepper_update();
            if (stepper_atTarget()) {
                transitionTo(AutoState::AWAIT_FIXTURE);
            }
            break;

        // --- AWAIT_FIXTURE: operator locks fixture, aligns mold to injection port ---
        case AutoState::AWAIT_FIXTURE:
            if (confirmPending) {
                confirmPending = false;
                transitionTo(AutoState::RAISE_FOR_MIX);
                stepper_moveTo_mm(STAGE_POS_MIX);
            }
            break;

        // --- RAISE_FOR_MIX ---
        case AutoState::RAISE_FOR_MIX:
            stepper_update();
            if (stepper_atTarget()) {
                transitionTo(AutoState::MIXING);
            }
            break;

        // --- MIXING ---
        case AutoState::MIXING:
            stepper_update();
            setCoolingLevel(2);
            if (runMixStep()) {
                transitionTo(AutoState::MOVE_MOLD_CLEAR);
                stepper_moveTo_mm(STAGE_POS_ZERO);    // back to zero
            }
            break;


        // --- MOVE_MOLD_CLEAR: timed run to hard stop ---
        case AutoState::MOVE_MOLD_CLEAR:
            driveMotor(180, 1, 3);
            if (millis() - stateStartTime >= MAGNET_CLEAR_DURATION_MS) {
                disableMotor(3);
                transitionTo(AutoState::LOWER_AFTER_MIX);
            }
            break;

        // --- LOWER_AFTER_MIX ---
        case AutoState::LOWER_AFTER_MIX:
            stepper_update();
            if (stepper_atTarget()) {
                transitionTo(AutoState::PURGE);
            }
            break;

        // --- PURGE: plunger descends; operator watches ---
        case AutoState::PURGE:
            // Plunger running continuously — transition when confirm pressed
            if (confirmPending) {
                confirmPending = false;
                transitionTo(AutoState::AWAIT_PURGE_DONE);
            }
            break;

        // --- AWAIT_PURGE_DONE: plunger stopped, waiting for confirm ---
        // (Separated from PURGE so operator can inspect before continuing)
        case AutoState::AWAIT_PURGE_DONE:
            if (confirmPending) {
                confirmPending = false;
                transitionTo(AutoState::LIFT_FOR_CLEARANCE);
            }
            break;

        // --- LIFT_FOR_CLEARANCE: stage + plunger rise together ---
        case AutoState::LIFT_FOR_CLEARANCE:
            if (syncUpdate()) {
                transitionTo(AutoState::MOVE_MOLD_BACK);
            }
            break;

        // --- MOVE_MOLD_BACK: timed run to hard stop (re-aligns injection port) ---
        case AutoState::MOVE_MOLD_BACK:
            driveMotor(0, 1, 3);
            if (millis() - stateStartTime >= MAGNET_BACK_DURATION_MS) {
                disableMotor(3);
                transitionTo(AutoState::LOWER_TO_INJECT);
            }
            break;

        // --- LOWER_TO_INJECT: stage + plunger descend together to zero ---
        case AutoState::LOWER_TO_INJECT:
            if (syncUpdate()) {
                transitionTo(AutoState::INJECT);
            }
            break;

        // --- INJECT: plunger injects; operator confirms when done ---
        case AutoState::INJECT:
            if (confirmPending) {
                confirmPending = false;
                transitionTo(AutoState::AWAIT_INJECT_DONE);
            }
            break;

        // --- AWAIT_INJECT_DONE ---
        case AutoState::AWAIT_INJECT_DONE:
            if (confirmPending) {
                confirmPending = false;
                transitionTo(AutoState::COMPLETE);
            }
            break;

        // --- COMPLETE ---
        case AutoState::COMPLETE:
            if (confirmPending) {
                confirmPending = false;
                transitionTo(AutoState::IDLE);
            }
            break;

        // --- ERROR ---
        case AutoState::ERROR:
            if (confirmPending) {
                confirmPending = false;
                transitionTo(AutoState::IDLE);
            }
            break;

        default:
            break;
    }

    // Consume any unhandled confirm so it doesn't carry into the next state
    confirmPending = false;
}