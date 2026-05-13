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

    bool wifiSetupRequested(); // true once when user navigates to WiFi view
    bool backRequested();      // true once when user presses Fn/Opt to exit

private:
    enum class View { Clock, BatteryStats, SystemInfo, HistoryLog, WiFiSetup };
    void drawCurrentView(const AppState& state);

    Display&  _disp;
    Keyboard& _kb;
    Battery&  _bat;
    History&  _hist;
    View      _view            = View::Clock;
    int       _histScroll      = 0;
    bool      _wifiSetupReq    = false;
    bool      _backReq         = false;
    bool      _enterDown       = false;
    bool      _fnDown          = false; // rising-edge debounce for Fn/Opt back
};
