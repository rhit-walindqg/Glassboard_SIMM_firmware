// ============================================================
//  state_machine.h
// ============================================================
#pragma once
#include "types.h"

void       sm_init();
void       sm_update();
AutoState  sm_getState();
SystemMode sm_getMode();
void       sm_confirmPressed();