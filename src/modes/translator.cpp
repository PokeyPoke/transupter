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
String TranslatorMode::targetLanguage(const String& detectedLang, const LangKey* lk) const {
    if (strlen(lk->whisperLang) == 0) return "English";   // E key: universal → EN
    if (detectedLang == "en")         return lk->pairLang; // detected EN → pair language
    return "English";                                       // detected pair → EN
}

void TranslatorMode::tick(AppState& state) {
    switch (_step) {
    case Step::Idle:      handleIdle(state);      break;
    case Step::Recording: handleRecording(state); break;
    default: break;
    }
}

void TranslatorMode::handleIdle(AppState& state) {
    char langKey = 0;
    bool held = _kb.isLangKeyHeld(langKey);

    if (held && !_keyWasHeld) {
        _keyWasHeld    = true;
        _activeLangKey = langKey;
        _rec.startRecord(15);
        _step = Step::Recording;
        drawRecording(langKey);
    } else if (!held) {
        _keyWasHeld = false;
    }
}

void TranslatorMode::handleRecording(AppState& state) {
    // Tight synchronous recording loop — matches the official M5Cardputer mic example.
    // record() blocks per-chunk; M5Cardputer.update() lets us check the keyboard.
    static constexpr size_t CHUNK      = 240;   // one I2S DMA buffer
    static constexpr size_t MAX_SECS   = 10;
    static constexpr size_t MAX_SAMPLES = Recorder::SAMPLE_RATE * MAX_SECS;

    int16_t* buf = (int16_t*) ps_malloc(MAX_SAMPLES * sizeof(int16_t));
    if (!buf) { drawError("Out of memory"); _step = Step::Idle; drawIdle(); return; }

    size_t        captured    = 0;
    char          langKey     = _activeLangKey;
    unsigned long lastCheckMs = millis();

    drawRecording(langKey);

    while (captured < MAX_SAMPLES) {
        // Record one DMA chunk (~15ms of audio)
        size_t toRead = min(CHUNK, MAX_SAMPLES - captured);
        if (M5Cardputer.Mic.record(buf + captured, toRead, Recorder::SAMPLE_RATE)) {
            captured += toRead;
        }

        // Check key state every 100ms — don't scan keyboard on every chunk
        // (over-scanning causes debounce to briefly show key as released)
        if (millis() - lastCheckMs >= 100) {
            lastCheckMs = millis();
            M5Cardputer.update();
            char k = 0;
            if (!_kb.isLangKeyHeld(k)) break;
            drawRecording(langKey);
        }
    }

    if (captured == 0) {
        free(buf);
        drawError("Nothing recorded");
        delay(1500);
        _step = Step::Idle;
        drawIdle();
        return;
    }

    WavHeader header  = buildWavHeader(Recorder::SAMPLE_RATE, captured);
    size_t    wavLen  = sizeof(WavHeader) + captured * sizeof(int16_t);
    uint8_t*  wav     = (uint8_t*) ps_malloc(wavLen);

    if (!wav) {
        free(buf);
        drawError("Out of memory");
        delay(1500);
        _step = Step::Idle;
        drawIdle();
        return;
    }

    memcpy(wav, &header, sizeof(WavHeader));
    memcpy(wav + sizeof(WavHeader), buf, captured * sizeof(int16_t));
    free(buf);

    runPipeline(state, langKey, wav, wavLen);
    free(wav);

    _step = Step::Idle;
    delay(2000);
    drawIdle();
}

void TranslatorMode::runPipeline(AppState& state, char langKeyChar,
                                 const uint8_t* wavData, size_t wavLen) {
    const LangKey* lk = findLangKey(langKeyChar);
    if (!lk) { drawError("Unknown key"); return; }

    // STT
    drawStatus("Transcribing...");
    auto stt = _groq.transcribe(state.keyGroq, wavData, wavLen, lk->whisperLang);
    if (!stt.success) { drawError(stt.error); return; }

    // Translation
    drawStatus("Translating...", stt.text.substring(0, 28).c_str());
    String tgtLang = targetLanguage(stt.detectedLang, lk);
    auto tr = _anthropic.translate(state.keyAnthropic, stt.text, tgtLang);
    if (!tr.success) { drawError(tr.error); return; }

    drawResult(stt.text, tr.text);
    _hist.append(stt.text, tr.text, String(langKeyChar));

    // TTS
    drawStatus("Speaking...");
    auto tts = _tts.synthesize(state.keyOpenAI, tr.text);
    if (!tts.success) { drawError(tts.error); return; }

    _player.playWav(tts.data, tts.length);
    free(tts.data);

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
