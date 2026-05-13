#include "recorder.h"
#include <M5Cardputer.h>

void Recorder::startRecord(int maxSecs) {
    _maxSamples = SAMPLE_RATE * maxSecs;
    _buf        = (int16_t*) ps_malloc(_maxSamples * sizeof(int16_t));
    if (!_buf) return;
    _captured   = 0;
    _recording  = true;

    auto cfg = M5.Mic.config();
    cfg.sample_rate = SAMPLE_RATE;
    cfg.stereo      = false;
    M5.Mic.config(cfg);
    M5.Mic.begin();
}

void Recorder::drainChunk() {
    if (!_recording || !_buf || _captured >= _maxSamples) return;
    size_t toRead = min(DRAIN_CHUNK, _maxSamples - _captured);
    M5.Mic.record(_buf + _captured, toRead, SAMPLE_RATE);
    _captured += toRead;
}

void Recorder::stopRecord() {
    if (!_recording) return;
    M5.Mic.end();
    _recording = false;
}

bool Recorder::isRecording() const { return _recording; }

RecordResult Recorder::getResult() {
    int32_t peak = 0;
    for (size_t i = 0; i < _captured; i++) {
        int32_t v = abs(_buf[i]);
        if (v > peak) peak = v;
    }

    return {
        .samples       = _buf,
        .numSamples    = _captured,
        .voiceDetected = (peak >= VAD_THRESHOLD),
    };
}
