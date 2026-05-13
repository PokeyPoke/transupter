#pragma once
#include "../app.h"
#include "../ui/display.h"
#include "../ui/keyboard.h"
#include "../audio/recorder.h"
#include "../audio/player.h"
#include "../audio/wav.h"
#include "../api/groq.h"
#include "../api/anthropic.h"
#include "../api/openai_tts.h"
#include "../storage/history.h"
#include "../config.h"

class TranslatorMode {
public:
    TranslatorMode(Display& disp, Keyboard& kb, History& hist);

    void tick(AppState& state);

private:
    enum class Step {
        Idle,
        Recording,
        Transcribing,
        Translating,
        Synthesizing,
        Playing,
        ShowResult,
        Error,
    };

    void handleIdle(AppState& state);
    void handleRecording(AppState& state);
    void runPipeline(AppState& state, char langKey,
                     const uint8_t* wavData, size_t wavLen);

    void drawIdle();
    void drawRecording(char key);
    void drawStatus(const char* line1, const char* line2 = "");
    void drawResult(const String& source, const String& translated);
    void drawError(const String& msg);

    const LangKey* findLangKey(char ch) const;
    String targetLanguage(const String& detectedLang, const LangKey* lk) const;

    Display&     _disp;
    Keyboard&    _kb;
    History&     _hist;

    Recorder     _rec;
    Player       _player;
    GroqApi      _groq;
    AnthropicApi _anthropic;
    OpenAiTts    _tts;

    Step   _step          = Step::Idle;
    char   _activeLangKey = 0;
    bool   _keyWasHeld    = false;
};
