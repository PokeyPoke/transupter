#include "translator.h"
#include <M5Cardputer.h>
#include <cstring>

TranslatorMode::TranslatorMode(Display& disp, Keyboard& kb, History& hist)
    : _disp(disp), _kb(kb), _hist(hist) {
    drawIdle();
}

const LangKey* TranslatorMode::findLangKey(char ch) const {
    for (int i = 0; i < LANG_KEY_COUNT; i++) {
        if (LANG_KEYS[i].key == ch) return &LANG_KEYS[i];
    }
    return nullptr;
}

// E key → always English. Pair key: detected EN → pair language, else → English.
// Whisper returns "en" or the full name "english" depending on version — handle both.
String TranslatorMode::targetLanguage(const String& detectedLang, const LangKey* lk) const {
    if (strlen(lk->whisperLang) == 0) return "English";   // E key: universal → EN
    String lang = detectedLang;
    lang.toLowerCase();
    if (lang == "en" || lang == "english") return lk->pairLang; // detected EN → pair language
    return "English";                                            // detected pair → EN
}

void TranslatorMode::tick(AppState& state) {
    switch (_step) {
    case Step::Idle:      handleIdle(state);      break;
    case Step::Recording: handleRecording(state); break;
    default: break;
    }
}

void TranslatorMode::forceRedraw() {
    drawIdle();
}

void TranslatorMode::handleIdle(AppState& state) {
    char langKey = 0;
    bool held = _kb.isLangKeyHeld(langKey);

    if (held && !_keyWasHeld) {
        _keyWasHeld    = true;
        _activeLangKey = langKey;
        _step = Step::Recording;
        drawRecording(langKey);
    } else if (!held) {
        _keyWasHeld = false;
    }
}

void TranslatorMode::handleRecording(AppState& state) {
    static constexpr size_t CHUNK    = 240;
    static constexpr size_t REC_SECS = 5;

    // Single allocation: reserve WAV header space + audio samples in one block.
    // This avoids the previous peak of buf(64KB) + wav(64KB) = 128KB simultaneous,
    // which fragmented the heap and caused TLS connection failures.
    size_t maxSamples = Recorder::SAMPLE_RATE * REC_SECS;
    size_t maxWavLen  = sizeof(WavHeader) + maxSamples * sizeof(int16_t);

    uint8_t* wav = (uint8_t*) ps_malloc(maxWavLen);
    if (!wav) {
        maxSamples = Recorder::SAMPLE_RATE * 2; // 2s from DRAM
        maxWavLen  = sizeof(WavHeader) + maxSamples * sizeof(int16_t);
        wav        = (uint8_t*) malloc(maxWavLen);
    }
    if (!wav) { drawError("Out of memory"); _step = Step::Idle; drawIdle(); return; }

    // Record samples directly after the header space
    int16_t*      samples    = (int16_t*)(wav + sizeof(WavHeader));
    size_t        captured   = 0;
    char          langKey    = _activeLangKey;
    unsigned long startMs    = millis();
    unsigned long lastDrawMs = 0;

    while (captured < maxSamples) {
        size_t toRead = min(CHUNK, maxSamples - captured);
        if (M5Cardputer.Mic.record(samples + captured, toRead, Recorder::SAMPLE_RATE)) {
            captured += toRead;
        } else {
            delay(5);
        }

        // Countdown display every 250ms
        if (millis() - lastDrawMs >= 250) {
            lastDrawMs = millis();
            int secsLeft = REC_SECS - (int)((millis() - startMs) / 1000);
            _disp.clearContent();
            auto& tft = M5Cardputer.Display;
            tft.setTextColor(RED, BLACK);
            tft.setTextSize(2);
            tft.setCursor(4, Display::CONTENT_Y + 10);
            tft.printf("[ REC ] %ds", max(0, secsLeft));
            tft.setTextSize(1);
            tft.setTextColor(WHITE, BLACK);
            tft.setCursor(4, Display::CONTENT_Y + 36);
            tft.printf("Speak now... %d samples", (int)captured);
        }
    }

    if (captured == 0) {
        free(wav);
        drawError("Nothing recorded");
        delay(1500);
        _step = Step::Idle;
        drawIdle();
        return;
    }

    // Write WAV header into the reserved space at the front of the buffer
    WavHeader header = buildWavHeader(Recorder::SAMPLE_RATE, captured);
    memcpy(wav, &header, sizeof(WavHeader));
    size_t wavLen = sizeof(WavHeader) + captured * sizeof(int16_t);

    runPipeline(state, langKey, wav, wavLen); // takes ownership, frees wav after STT

    _step = Step::Idle;
    delay(2000);
    drawIdle();
}

void TranslatorMode::runPipeline(AppState& state, char langKeyChar,
                                 uint8_t* wavData, size_t wavLen) {
    const LangKey* lk = findLangKey(langKeyChar);
    if (!lk) { free(wavData); drawError("Unknown key"); return; }

    if (!state.wifiConnected) { free(wavData); drawError("No WiFi - go to Utilities"); return; }

    // STT — transcribe audio; no language hint so Whisper auto-detects the spoken
    // language. This is required for bidirectional detection: sending a hint causes
    // Whisper to report the hint language in its response even when the user speaks
    // English, which breaks the direction logic in targetLanguage().
    drawStatus("Connecting to Groq...");
    auto stt = _groq.transcribe(state.keyGroq, wavData, wavLen, "");

    // Free WAV buffer NOW — largest allocation (~64 KB), must go before Anthropic TLS
    free(wavData);
    wavData = nullptr;

    if (!stt.success) { drawError(stt.error); return; }
    Serial.printf("[STT] lang='%s' text='%s'\n", stt.detectedLang.c_str(), stt.text.substring(0,40).c_str());
    drawStatus("Got:", stt.text.substring(0, 28).c_str());

    // Translation — Anthropic TLS needs ~8 KB (reduced from 32 KB via sdkconfig)
    drawStatus("Translating...", stt.text.substring(0, 28).c_str());
    String tgtLang = targetLanguage(stt.detectedLang, lk);
    Serial.printf("[TR] detectedLang='%s' target='%s'\n", stt.detectedLang.c_str(), tgtLang.c_str());
    auto tr = _anthropic.translate(state.keyAnthropic, stt.text, tgtLang);
    if (!tr.success) { drawError(tr.error); return; }

    drawResult(stt.text, tr.text);
    _hist.append(stt.text, tr.text, String(langKeyChar));

    // TTS — attempt but non-fatal; show error briefly so we can diagnose
    drawStatus("Speaking...");
    auto tts = _tts.synthesize(state.keyOpenAI, tr.text);
    if (tts.success) {
        _player.playWav(tts.data, tts.length);
        free(tts.data);
    } else {
        Serial.printf("[TTS] failed: %s\n", tts.error.c_str());
        drawStatus("TTS err:", tts.error.substring(0, 24).c_str());
        delay(2000);
    }

    drawResult(stt.text, tr.text);
}

void TranslatorMode::drawIdle() {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 10);
    tft.print("Hold a key to translate:");
    tft.setCursor(4, Display::CONTENT_Y + 26);
    tft.setTextColor(WHITE, BLACK);
    tft.print("C=Czech  V=Viet  U=Ukr  R=Rus");
    tft.setCursor(4, Display::CONTENT_Y + 40);
    tft.print("M=Mandarin  J=Jpn  K=Korean");
    tft.setCursor(4, Display::CONTENT_Y + 54);
    tft.setTextColor(CYAN, BLACK);
    tft.print("E=Universal (any->EN)");
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 80);
    tft.print("Fn/Opt=Utilities");
}

void TranslatorMode::drawRecording(char key) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextColor(RED, BLACK);
    tft.setTextSize(2);
    tft.setCursor(4, Display::CONTENT_Y + 10);
    tft.printf("[ REC ] key=%c", toupper(key));
    tft.setTextSize(1);
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 36);
    tft.print("Listening... release to translate");
}

void TranslatorMode::drawStatus(const char* line1, const char* line2) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(YELLOW, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 10);
    tft.print(line1);
    if (line2 && strlen(line2) > 0) {
        tft.setTextColor(WHITE, BLACK);
        tft.setCursor(4, Display::CONTENT_Y + 26);
        tft.print(line2);
    }
}

void TranslatorMode::drawResult(const String& source, const String& translated) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 4);
    tft.print(source.substring(0, 36));
    tft.setTextColor(GREEN, BLACK);
    for (int i = 0, line = 0; i < (int)translated.length() && line < 3; line++) {
        String chunk = translated.substring(i, i + 36);
        tft.setCursor(4, Display::CONTENT_Y + 20 + line * 14);
        tft.print(chunk);
        i += 36;
    }
}

void TranslatorMode::drawError(const String& msg) {
    _disp.showPopup("Error", msg.substring(0, 28).c_str());
}
