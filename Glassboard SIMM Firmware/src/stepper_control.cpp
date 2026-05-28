#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "types.h"
#include "stepper_control.h"

// ============================================================================
//  Stepper motor / linear stage control
//
//  Coordinate convention used by this module:
//    - Home / lower limit switch = 0 mm
//    - Positive position = gantry/stage moves upward away from home
//    - Logical direction true  = up / positive
//    - Logical direction false = down / negative / toward home
//
//  Important:
//    - stepper_moveTo_mm(x)       = absolute position command from home
//    - stepper_moveRelative_mm(x) = relative move from current position
//    - stepper_move_mm(x)         = legacy wrapper; still relative movement
// ============================================================================

// ---------------------------------------------------------------------------
// Derived constants
// ---------------------------------------------------------------------------
static const int stepsPerRev = (int)(360.0f / LINEAR_STAGE_MOTOR_STEP_ANGLE)
                                * LINEAR_STAGE_MICROSTEPS;
static const int stepsPerMm  = stepsPerRev / LINEAR_STAGE_LEADSCREW_LEAD;

static const int32_t minPositionSteps = 0;
static const int32_t maxPositionSteps = (int32_t)(LINEAR_STAGE_THROW * stepsPerMm);

static const uint32_t maxSpeedInterval_us = (uint32_t)(1000000.0f /
                                             (STEPPER_MAX_SPEED_MM_S * stepsPerMm));
static const uint32_t homingInterval_us   = (uint32_t)(1000000.0f /
                                             (STEPPER_HOMING_SPEED_MM_S * stepsPerMm));
static const uint32_t syncInterval_us     = (uint32_t)(1000000.0f /
                                             (STEPPER_SYNC_SPEED_MM_S * stepsPerMm));

// If positive commands ever move physically downward, change this to true.
// For your current machine, joystick polarity is correct, so leave false.
static const bool STEPPER_DIR_INVERTED = false;

// ---------------------------------------------------------------------------
// Motion state
// ---------------------------------------------------------------------------
static volatile int32_t  currentPosition = 1000;  // intentionally non-zero until homed
static volatile int32_t  stepsRemaining  = 0;
static volatile bool     stepDir         = true;  // true = up / positive
static volatile uint32_t lastStepTime    = 0;
static volatile bool     moving          = false;
static volatile bool     continuousMode  = false;
static uint32_t          stepInterval_us = maxSpeedInterval_us;

// ---------------------------------------------------------------------------
// Homing state
// ---------------------------------------------------------------------------
volatile bool isHoming = false;
volatile bool atHome   = false;

// ---------------------------------------------------------------------------
// Limit switch polling state
// ---------------------------------------------------------------------------
static bool     lastLimitState     = HIGH;
static bool     limitConfirmed     = true;
static uint32_t lastLimitCheckTime = 0;

// ---------------------------------------------------------------------------
// Target tracking for finite position moves
// ---------------------------------------------------------------------------
static int32_t targetPosition = 0;
static bool    hasTarget      = false;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
static int32_t abs32(int32_t value) {
    return (value < 0) ? -value : value;
}

static int32_t clampPositionSteps(int32_t targetSteps) {
    if (targetSteps < minPositionSteps) return minPositionSteps;
    if (targetSteps > maxPositionSteps) return maxPositionSteps;
    return targetSteps;
}

static void writeStepperDir(bool logicalUp) {
    bool dirLevel = STEPPER_DIR_INVERTED ? !logicalUp : logicalUp;
    digitalWrite(PIN_STEPPER_DIR, dirLevel ? HIGH : LOW);
    delayMicroseconds(2);  // driver direction setup time before first STEP pulse
}

static void markStoppedAtLimit(int32_t knownPositionSteps) {
    stepsRemaining  = 0;
    moving          = false;
    continuousMode  = false;
    currentPosition = clampPositionSteps(knownPositionSteps);

    // If a finite absolute/relative move was actually targeting this limit,
    // allow stepper_atTarget() to become true. If this was manual jogging or an
    // unexpected limit stop, clear the target so the state machine will not be
    // falsely told that a commanded move completed.
    if (hasTarget && targetPosition == currentPosition) {
        hasTarget = true;
    } else {
        hasTarget = false;
    }
}

static void startContinuousMove(bool directionUp, uint32_t interval_us) {
    stepDir         = directionUp;
    stepInterval_us = interval_us;
    continuousMode  = true;
    stepsRemaining  = 0;
    hasTarget       = false;
    moving          = true;

    writeStepperDir(stepDir);
}

static void startFiniteMoveSteps(int32_t deltaSteps, uint32_t interval_us) {
    stepInterval_us = interval_us;

    if (deltaSteps == 0) {
        moving         = false;
        continuousMode = false;
        return;
    }

    stepDir         = (deltaSteps > 0);
    stepsRemaining  = abs32(deltaSteps);
    continuousMode  = false;
    moving          = true;

    DBG_PRINT("Stepper raw finite move | steps: ");
    DBG_PRINT(stepsRemaining);
    DBG_PRINT(" | direction: ");
    DBG_PRINTLN(stepDir ? "UP" : "DOWN");

    writeStepperDir(stepDir);
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void stepper_init() {
    pinMode(PIN_STEPPER_STEP, OUTPUT);
    pinMode(PIN_STEPPER_DIR,  OUTPUT);
    pinMode(PIN_STEPPER_EN,   OUTPUT);

    digitalWrite(PIN_STEPPER_EN,   LOW);   // enable driver; active low
    digitalWrite(PIN_STEPPER_STEP, LOW);
    writeStepperDir(false);                // default direction = down/toward home

    // Limit switch is polled inside stepper_update(); do not attach an interrupt.
    pinMode(PIN_STEPPER_LIMITSWITCH, INPUT_PULLUP);
}

// ---------------------------------------------------------------------------
// Motion commands
// ---------------------------------------------------------------------------
void stepper_set_speed(int32_t steps_per_sec) {
    if (steps_per_sec == 0) {
        stepper_stopImmediately();
        return;
    }

    uint32_t interval = (uint32_t)(1000000UL / abs32(steps_per_sec));
    startContinuousMove(steps_per_sec > 0, interval);

    DBG_PRINT("Stepper jog | speed steps/s: ");
    DBG_PRINT(steps_per_sec);
    DBG_PRINT(" | direction: ");
    DBG_PRINTLN(stepDir ? "UP" : "DOWN");
}

void stepper_move(int32_t steps, bool direction) {
    // Low-level finite move in raw steps. This function intentionally does not
    // clear hasTarget, because stepper_moveTo_mm() and stepper_moveRelative_mm()
    // rely on hasTarget staying true until the move completes.
    int32_t signedDelta = direction ? abs32(steps) : -abs32(steps);
    startFiniteMoveSteps(signedDelta, stepInterval_us);
}

void stepper_moveTo_mm(float target_mm) {
    // Absolute position from home/zero.
    int32_t target = (int32_t)(target_mm * stepsPerMm);
    target = clampPositionSteps(target);

    int32_t delta = target - currentPosition;

    targetPosition = target;
    hasTarget      = true;

    DBG_PRINT("Stepper moveTo mm: ");
    DBG_PRINT(target_mm);
    DBG_PRINT(" | current steps: ");
    DBG_PRINT(currentPosition);
    DBG_PRINT(" | target steps: ");
    DBG_PRINT(targetPosition);
    DBG_PRINT(" | delta steps: ");
    DBG_PRINTLN(delta);

    startFiniteMoveSteps(delta, maxSpeedInterval_us);
}

void stepper_moveRelative_mm(float delta_mm) {
    // Relative movement from current position.
    int32_t deltaSteps = (int32_t)(delta_mm * stepsPerMm);
    int32_t target     = currentPosition + deltaSteps;
    target = clampPositionSteps(target);

    int32_t delta = target - currentPosition;

    targetPosition = target;
    hasTarget      = true;

    DBG_PRINT("Stepper moveRelative mm: ");
    DBG_PRINT(delta_mm);
    DBG_PRINT(" | current steps: ");
    DBG_PRINT(currentPosition);
    DBG_PRINT(" | target steps: ");
    DBG_PRINT(targetPosition);
    DBG_PRINT(" | delta steps: ");
    DBG_PRINTLN(delta);

    startFiniteMoveSteps(delta, maxSpeedInterval_us);
}

void stepper_move_mm(float mm) {
    // Legacy function kept for existing code. It remains RELATIVE.
    stepper_moveRelative_mm(mm);
}

void stepper_home() {
    isHoming        = true;
    atHome          = false;
    targetPosition  = 0;
    hasTarget       = false;

    DBG_PRINTLN("Stepper: homing started, direction DOWN");

    // Homing is continuous. The physical lower limit switch, not a fake step
    // count, defines the true zero position.
    startContinuousMove(false, homingInterval_us);
}

void stepper_stopImmediately() {
    stepsRemaining = 0;
    moving         = false;
    continuousMode = false;
    hasTarget      = false;
}

// ---------------------------------------------------------------------------
// Limit switch polling — called inside stepper_update()
// Returns true on a confirmed debounced LOW reading.
// ---------------------------------------------------------------------------
static bool pollLimitSwitch() {
    bool currentState = digitalRead(PIN_STEPPER_LIMITSWITCH);
    uint32_t now = millis();

    if (currentState != lastLimitState) {
        lastLimitState     = currentState;
        lastLimitCheckTime = now;
        limitConfirmed     = false;
    }

    if (!limitConfirmed &&
        (now - lastLimitCheckTime >= LIMITSWITCH_DEBOUNCE_MS)) {
        limitConfirmed = true;
    }

    return (limitConfirmed && currentState == LOW);
}

// ---------------------------------------------------------------------------
// stepper_update() — call every loop()
// ---------------------------------------------------------------------------
void stepper_update() {
    bool limitHit = pollLimitSwitch();

    // --- Physical lower limit switch ---
    if (limitHit) {
        if (isHoming) {
            stepsRemaining  = 0;
            moving          = false;
            continuousMode  = false;
            currentPosition = 0;
            targetPosition  = 0;
            hasTarget       = false;
            isHoming        = false;
            atHome          = true;
            stepInterval_us = maxSpeedInterval_us;

            DBG_PRINTLN("Stepper: homed");
            return;
        }

        // Only stop on the lower switch if the motor is actively trying to move
        // downward into the switch. If moving upward, ignore it so the gantry can
        // back away from home after homing.
        if (moving && !stepDir) {
            markStoppedAtLimit(0);
            DBG_PRINTLN("Stepper: lower limit hit");
            return;
        }
    }

    if (!moving) return;

    // --- Finite move completion ---
    if (!continuousMode && stepsRemaining == 0) {
        moving = false;
        return;
    }

    // --- Software travel limits ---
    // Do not apply the lower software limit while homing. During homing, the
    // physical switch is the source of truth.
    if (!isHoming && !stepDir && currentPosition <= minPositionSteps) {
        markStoppedAtLimit(minPositionSteps);
        DBG_PRINTLN("Stepper: lower software limit reached");
        return;
    }

    if (!isHoming && stepDir && currentPosition >= maxPositionSteps) {
        markStoppedAtLimit(maxPositionSteps);
        DBG_PRINTLN("Stepper: upper software limit reached");
        return;
    }

    // --- Step pulse ---
    uint32_t now = micros();
    if (now - lastStepTime >= stepInterval_us) {
        lastStepTime = now;

        digitalWrite(PIN_STEPPER_STEP, HIGH);
        delayMicroseconds(2);
        digitalWrite(PIN_STEPPER_STEP, LOW);

        currentPosition += stepDir ? 1 : -1;

        if (!continuousMode && stepsRemaining > 0) {
            stepsRemaining--;
        }
    }
}

// ---------------------------------------------------------------------------
// Status queries
// ---------------------------------------------------------------------------
bool stepper_homingStatus()          { return atHome; }
bool stepper_isMoving()              { return moving; }
bool stepper_atTarget()              { return hasTarget && !moving; }
int32_t stepper_getPosition()        { return currentPosition; }
float stepper_getPosition_mm()       { return (float)currentPosition / stepsPerMm; }
int stepper_getPosition_mm_rounded() { return currentPosition / stepsPerMm; }
int stepper_getStepsPerMm()          { return stepsPerMm; }

void stepper_printDiagnostics() {
    DBG_PRINT("stepsPerRev: ");       DBG_PRINTLN(stepsPerRev);
    DBG_PRINT("stepsPerMm: ");        DBG_PRINTLN(stepsPerMm);
    DBG_PRINT("maxPositionSteps: ");  DBG_PRINTLN(maxPositionSteps);
    DBG_PRINT("stepInterval_us: ");   DBG_PRINTLN(stepInterval_us);
    DBG_PRINT("position_mm: ");       DBG_PRINTLN(stepper_getPosition_mm());
    DBG_PRINT("moving: ");            DBG_PRINTLN(moving ? "true" : "false");
    DBG_PRINT("direction: ");         DBG_PRINTLN(stepDir ? "UP" : "DOWN");
    DBG_PRINT("hasTarget: ");         DBG_PRINTLN(hasTarget ? "true" : "false");
    DBG_PRINT("target_mm: ");         DBG_PRINTLN((float)targetPosition / stepsPerMm);
}

// ---------------------------------------------------------------------------
// ISR — kept only for compatibility. Do not attach this while using the polled
// homing/limit logic above.
// ---------------------------------------------------------------------------
void ISR_StepperLowerLimit() {
    static uint32_t lastTrigger = 0;
    uint32_t now = millis();

    if (now - lastTrigger < 50) return;
    lastTrigger = now;

    stepper_stopImmediately();
}
