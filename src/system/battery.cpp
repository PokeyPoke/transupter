#include "battery.h"
#include <M5Cardputer.h>

BatteryState Battery::read() const {
    return {
        .percent  = M5Cardputer.Power.getBatteryLevel(),
        .charging = M5Cardputer.Power.isCharging(),
    };
}
