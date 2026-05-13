#pragma once
#include <cstdint>
#include <cstddef>

struct RecordResult {
    int16_t* samples;       // PSRAM buffer (caller must ps_free after use)
    size_t   numSamples;
    bool     voiceDetected; // false if silent (VAD failed)
};

class Recorder {
public:
    static constexpr int    SAMPLE_RATE    = 16000;
    static constexpr int    VAD_THRESHOLD  = 500;  // peak amplitude below = silence
    static constexpr size_t DRAIN_CHUNK    = 512;  // samples per drain call

    // Allocates PSRAM buffer and starts mic.
    void startRecord(int maxSecs = 15);

    // Call once per loop() iteration while key is held to pull mic samples.
    void drainChunk();

    // Stops mic, finalises buffer.
    void stopRecord();

    bool isRecording() const;

    // Call after stopRecord() to retrieve result. Caller owns samples buffer.
    RecordResult getResult();

private:
    bool     _recording  = false;
    int16_t* _buf        = nullptr;
    size_t   _maxSamples = 0;
    size_t   _captured   = 0;
};
