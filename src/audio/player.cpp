#include "player.h"
#include <M5Cardputer.h>

void Player::playWav(const uint8_t* data, size_t length) {
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(200);
    M5Cardputer.Speaker.playWav(data, length, 1, 0, true);
    M5Cardputer.Speaker.end();
}

void Player::stop() {
    M5Cardputer.Speaker.stop();
}
