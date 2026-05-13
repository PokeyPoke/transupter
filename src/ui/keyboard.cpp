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
    ks.isEsc   = raw.fn;   // Fn = back/cancel
    ks.isOpt   = raw.opt;  // Opt = alternate action

    // Primary character key (suppress nav keys from ch)
    if (!raw.word.empty()) {
        char c = raw.word[0];
        if (c != ';' && c != '.') {
            ks.ch = tolower(c);
        }
    }

    // ; = up, . = down — right-side vertical pair, natural arrow substitute
    if (!raw.word.empty()) {
        if (raw.word[0] == ';') ks.isUp   = true;
        if (raw.word[0] == '.') ks.isDown = true;
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
