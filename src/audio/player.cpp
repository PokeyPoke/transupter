#include "player.h"
#include <M5Cardputer.h>

void Player::playWav(const uint8_t* data, size_t length) {
    M5.Speaker.begin();
    M5.Speaker.setVolume(200);
    M5.Speaker.playWav(data, length, 1, 0, true);
    M5.Speaker.end();
}

void Player::stop() {
    M5.Speaker.stop();
}
