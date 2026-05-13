#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct WavHeader {
    char     riff[4]       = {'R','I','F','F'};
    uint32_t fileSize;
    char     wave[4]       = {'W','A','V','E'};
    char     fmt[4]        = {'f','m','t',' '};
    uint32_t fmtSize       = 16;
    uint16_t audioFmt      = 1;    // PCM
    uint16_t channels      = 1;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char     data[4]       = {'d','a','t','a'};
    uint32_t dataSize;
};
#pragma pack(pop)

static_assert(sizeof(WavHeader) == 44, "WavHeader must be 44 bytes");

// Returns a fully populated WAV header for mono 16-bit PCM
WavHeader buildWavHeader(uint32_t sampleRate, uint32_t numSamples);
