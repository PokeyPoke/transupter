#pragma once
#include <Arduino.h>

// Key event types
enum class KeyEvent { None, Down, Up, Held };

struct KeyState {
    char     ch;          // lowercase character, or 0 for special keys
    bool     isEnter;
    bool     isDel;
    bool     isEsc;       // Fn key — back/cancel
    bool     isOpt;       // Opt key — alternate action
    bool     isUp;        // ; key
    bool     isDown;      // . key
    KeyEvent event;
};

class Keyboard {
public:
    // Call once per loop() iteration AFTER M5Cardputer.update()
    KeyState poll();

    // Returns true while a translation language key (c/v/u/r/m/j/k/e) is held
    bool isLangKeyHeld(char& outKey) const;

private:
    mutable char  _prevLangKey = 0;
    mutable bool  _langHeld    = false;
};
