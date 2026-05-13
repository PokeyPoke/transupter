#include "wav.h"

WavHeader buildWavHeader(uint32_t sampleRate, uint32_t numSamples) {
    WavHeader h;
    h.sampleRate  = sampleRate;
    h.dataSize    = numSamples * sizeof(int16_t);
    h.blockAlign  = 1 * (16 / 8);
    h.byteRate    = sampleRate * h.blockAlign;
    h.fileSize    = 36 + h.dataSize;
    return h;
}
