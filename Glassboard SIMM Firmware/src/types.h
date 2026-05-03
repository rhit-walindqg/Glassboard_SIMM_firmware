#pragma once
#include <stdint.h>
 
// ============================================================
//  types.h
//  Shared enums, structs, and typed constants.
//  Prefer these over bare #define macros where types matter.
// ============================================================

enum class AutomatedInjectionState : uint8_t {
    INACTIVE,
    HOME,
    RAISE_SYRINGE,
    MIX_SYRINGE,
    LOWER_SYRINGE,
    PURGE_CAP,
    WAIT_FOR_MOLD_REPOSITION,
    INJECT,
    RESET,
    ERROR
};