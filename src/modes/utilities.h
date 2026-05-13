#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../system/battery.h"
#include "../storage/history.h"

class UtilitiesMode {
public:
    UtilitiesMode(Display& disp, Keyboard& kb, Battery& bat, History& hist);
    void tick(AppState& state);

private:
    enum class View { Clock, BatteryStats, SystemInfo, HistoryLog };
    void drawCurrentView(const AppState& state);

    Display&  _disp;
    Keyboard& _kb;
    Battery&  _bat;
    History&  _hist;
    View      _view = View::Clock;
    int       _histScroll = 0;
};
