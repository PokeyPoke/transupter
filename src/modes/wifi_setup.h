#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../system/wifi_mgr.h"
#include "../system/nvs_store.h"

class WifiSetupMode {
public:
    WifiSetupMode(Display& disp, Keyboard& kb, WifiMgr& wifi, NvsStore& nvs);

    // Returns true when WiFi is successfully connected (caller switches mode)
    bool tick(AppState& state);

private:
    enum class Step { Scanning, SelectNetwork, EnterPassword, Connecting };

    void drawNetworkList();
    void drawPasswordEntry();
    void drawConnecting();

    Display&  _disp;
    Keyboard& _kb;
    WifiMgr&  _wifi;
    NvsStore& _nvs;

    Step               _step        = Step::Scanning;
    std::vector<AccessPoint> _aps;
    int                _selected    = 0;
    int                _scrollOffset = 0;
    String             _password;
    bool               _enterDown   = false;
};
