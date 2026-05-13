#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../system/nvs_store.h"

class ApiKeySetupMode {
public:
    ApiKeySetupMode(Display& disp, Keyboard& kb, NvsStore& nvs);

    // Returns true when all 3 keys have been entered and saved
    bool tick(AppState& state);

private:
    enum class Step { EnterGroq, EnterAnthropic, EnterOpenAI, Done };

    void drawEntry(const char* label, const String& current);

    Display&  _disp;
    Keyboard& _kb;
    NvsStore& _nvs;

    Step   _step = Step::EnterGroq;
    String _input;
};
