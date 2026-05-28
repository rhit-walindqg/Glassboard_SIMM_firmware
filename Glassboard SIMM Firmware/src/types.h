#pragma once
#include <stdint.h>

// ============================================================
//  types.h
// ============================================================

enum class SystemMode : uint8_t {
    MANUAL,
    AUTO
};

enum class AutoState : uint8_t {
    IDLE,

    HOMING,

    RAISE_FOR_FIXTURE,      // raise to cap-clearance offset
    AWAIT_FIXTURE,          // [CONFIRM] lock fixture, align mold to injection port

    RAISE_FOR_MIX,          // raise to mixing height
    MIXING,                 // timed mix cycle
    LOWER_AFTER_MIX,        // return stage to zero

    MOVE_MOLD_CLEAR,        // magnet locator drives mold clear (timed → hard stop)

    PURGE,                  // plunger descends slowly
    AWAIT_PURGE_DONE,       // [CONFIRM] purge sufficient

    LIFT_FOR_CLEARANCE,     // stage + plunger rise together
    MOVE_MOLD_BACK,         // magnet locator drives mold back (timed → hard stop)
    LOWER_TO_INJECT,        // stage + plunger descend together to zero

    INJECT,                 // plunger continues injecting
    AWAIT_INJECT_DONE,      // [CONFIRM] injection complete

    COMPLETE,
    ERROR
};

enum class ButtonState : uint8_t {
    IDLE,
    PRESSED,
    HELD,
    RELEASED
};