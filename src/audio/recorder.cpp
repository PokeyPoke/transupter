#include "recorder.h"
#include <M5Cardputer.h>

void Recorder::startRecord(int maxSecs) {
    _maxSamples = SAMPLE_RATE * maxSecs;
    _buf        = (int16_t*) ps_malloc(_maxSamples * sizeof(int16_t));
    if (!_buf) return;
    _captured   = 0;
    _recording  = true;

    auto cfg = M5Cardputer.Mic.config();
    cfg.sample_rate = SAMPLE_RATE;
    cfg.stereo      = false;
    M5Cardputer.Mic.config(cfg);
    M5Cardputer.Mic.begin();
}

void Recorder::drainChunk() {
    if (!_recording || !_buf || _captured >= _maxSamples) return;
    size_t toRead = min(DRAIN_CHUNK, _maxSamples - _captured);
    if (M5Cardputer.Mic.record(_buf + _captured, toRead, SAMPLE_RATE)) {
        _captured += toRead;
    }
}

void Recorder::stopRecord() {
    if (!_recording) return;
    M5Cardputer.Mic.end();
    _recording = false;
}

bool Recorder::isRecording() const { return _recording; }

RecordResult Recorder::getResult() {
    // VAD: pass if we captured anything — Groq handles empty audio gracefully
    return {
        .samples       = _buf,
        .numSamples    = _captured,
        .voiceDetected = (_captured > 0),
    };
}
