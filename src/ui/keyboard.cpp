#include "keyboard.h"
#include "../config.h"
#include <M5Cardputer.h>

static bool isLangChar(char c) {
    for (int i = 0; i < LANG_KEY_COUNT; i++) {
        if (LANG_KEYS[i].key == c) return true;
    }
    return false;
}

KeyState Keyboard::poll() {
    KeyState ks{};

    if (!M5Cardputer.Keyboard.isChange()) return ks;

    auto& raw = M5Cardputer.Keyboard.keysState();

    ks.isEnter = raw.enter;
    ks.isDel   = raw.del;
    ks.isEsc   = raw.fn; // treat Fn as ESC/cancel

    // Primary character key
    if (!raw.word.empty()) {
        ks.ch = tolower(raw.word[0]);
    }

    // Fn+W/S as up/down for menu navigation
    if (raw.fn && !raw.word.empty()) {
        if (raw.word[0] == 'w' || raw.word[0] == 'W') ks.isUp   = true;
        if (raw.word[0] == 's' || raw.word[0] == 'S') ks.isDown = true;
    }

    return ks;
}

bool Keyboard::isLangKeyHeld(char& outKey) const {
    auto& raw = M5Cardputer.Keyboard.keysState();
    if (raw.word.empty()) {
        _prevLangKey = 0;
        _langHeld    = false;
        return false;
    }
    char c = tolower(raw.word[0]);
    if (isLangChar(c)) {
        outKey       = c;
        _prevLangKey = c;
        _langHeld    = true;
        return true;
    }
    return false;
}
