#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "types.h"
#include "cooling.h"

void setCoolingLevel(int coolingLevel) {
    if(coolingLevel == 0) {
        analogWrite(PIN_COOLINGFAN_DUTYCYCLE, FANSPEED_OFF);
    } else if(coolingLevel == 1) {
        analogWrite(PIN_COOLINGFAN_DUTYCYCLE, FANSPEED_IDLING);
    } else if(coolingLevel == 2) {
        analogWrite(PIN_COOLINGFAN_DUTYCYCLE, FANSPEED_COOLING);
    }
}