#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../system/battery.h"

class UtilitiesMode {
public:
    UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat);
    void tick(AppState& state);

private:
    enum class View { Clock, BatteryStats, SystemInfo };
    void drawCurrentView(const AppState& state);

    Display&  _disp;
    Keyboard& _kb;
    Battery&  _bat;
    View      _view = View::Clock;
};
