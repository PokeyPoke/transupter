#include "player.h"
#include <M5Cardputer.h>

void Player::playWav(const uint8_t* data, size_t length) {
    // Mic and Speaker share I2S — must stop mic before starting speaker
    M5Cardputer.Mic.end();
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(200);
    M5Cardputer.Speaker.playWav(data, length, 1, 0, true);
    M5Cardputer.Speaker.end();
    // Resume mic for the next recording
    M5Cardputer.Mic.begin();
}

void Player::stop() {
    M5Cardputer.Speaker.stop();
}
