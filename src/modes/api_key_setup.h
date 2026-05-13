#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../system/nvs_store.h"
#include <WebServer.h>

class ApiKeySetupMode {
public:
    ApiKeySetupMode(Display& disp, Keyboard& kb, NvsStore& nvs);

    // Returns true when all 3 keys have been saved via the web portal
    bool tick(AppState& state);

private:
    void handleRoot();
    void handleSave();
    void handleDone();
    void drawPortalScreen();

    Display&   _disp;
    Keyboard&  _kb;
    NvsStore&  _nvs;
    WebServer  _server{80};
    AppState*  _state = nullptr;
    bool       _saved = false;
};
