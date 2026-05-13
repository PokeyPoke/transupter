#pragma once
#include <cstdint>
#include <cstddef>

class Player {
public:
    // Play raw WAV bytes (including 44-byte header). Blocks until complete.
    void playWav(const uint8_t* data, size_t length);

    // Stop any current playback
    void stop();
};
