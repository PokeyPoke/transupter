#include "recorder.h"
#include <M5Cardputer.h>

void Recorder::startRecord(int maxSecs) {
    _maxSamples = SAMPLE_RATE * maxSecs;
    _buf        = (int16_t*) ps_malloc(_maxSamples * sizeof(int16_t));
    if (!_buf) return;
    _captured  = 0;
    _recording = true;
    // Mic was started in M5Cardputer.begin() — don't reinitialise.
    // Reinitialising breaks the running DMA stream.
}

void Recorder::drainChunk() {
    if (!_recording || !_buf || _captured >= _maxSamples) return;
    size_t toRead = min(DRAIN_CHUNK, _maxSamples - _captured);
    if (M5Cardputer.Mic.record(_buf + _captured, toRead, SAMPLE_RATE)) {
        _captured += toRead;
    }
}

void Recorder::stopRecord() {
    _recording = false;
    // Leave Mic running — it needs to stay active between recordings.
    // Mic.end() is only called by Player before Speaker.begin().
}

bool Recorder::isRecording() const { return _recording; }

RecordResult Recorder::getResult() {
    return {
        .samples       = _buf,
        .numSamples    = _captured,
        .voiceDetected = (_captured > 0),
    };
}
