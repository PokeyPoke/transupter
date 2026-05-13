#pragma once
#include <Arduino.h>
#include "../app.h"

class Display {
public:
    void begin();

    // Full-screen helpers
    void clear();
    void showBootScreen(const char* message);

    // Status bar — always drawn at top, 18px tall
    void drawStatusBar(const AppState& state);

    // Content area (below status bar)
    void clearContent();
    void printContent(const char* text, int textSize = 2);
    void printCentered(const char* text, int y, int textSize = 2);

    // Overlay
    void showPopup(const char* title, const char* body);

    static constexpr int CONTENT_Y = 18; // pixels below status bar
};
