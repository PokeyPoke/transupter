#pragma once

// Display
static constexpr int DISPLAY_WIDTH  = 240;
static constexpr int DISPLAY_HEIGHT = 135;
static constexpr int STATUS_BAR_H   = 18;

// Audio (Plan B)
static constexpr int SAMPLE_RATE_RECORD  = 16000;
static constexpr int SAMPLE_RATE_PLAYBACK = 22050;
static constexpr int MAX_RECORD_SECS     = 15;
static constexpr int RECORD_BUF_SAMPLES  = SAMPLE_RATE_RECORD * MAX_RECORD_SECS;

// WiFi
static constexpr int WIFI_CONNECT_TIMEOUT_MS = 10000;
static constexpr int WIFI_SKIP_TIMEOUT_MS    = 5000;

// Battery
static constexpr int BATTERY_LOW_WARN    = 15;
static constexpr int BATTERY_LOW_POPUP   = 10;

// NVS namespace
static constexpr char NVS_NS[]           = "transupter";

// NVS keys
static constexpr char NVS_WIFI_SSID[]    = "wifi_ssid";
static constexpr char NVS_WIFI_PASS[]    = "wifi_pass";
static constexpr char NVS_KEY_GROQ[]     = "key_groq";
static constexpr char NVS_KEY_ANTHROPIC[]= "key_anthropic";
static constexpr char NVS_KEY_OPENAI[]   = "key_openai";

// API endpoints
static constexpr char GROQ_HOST[]        = "api.groq.com";
static constexpr char GROQ_STT_PATH[]    = "/openai/v1/audio/transcriptions";
static constexpr char GROQ_STT_MODEL[]   = "whisper-large-v3";

static constexpr char ANTHROPIC_HOST[]   = "api.anthropic.com";
static constexpr char ANTHROPIC_PATH[]   = "/v1/messages";
static constexpr char ANTHROPIC_MODEL[]  = "claude-haiku-4-5-20251001";
static constexpr char ANTHROPIC_VER[]    = "2023-06-01";

static constexpr char OPENAI_HOST[]      = "api.openai.com";
static constexpr char OPENAI_TTS_PATH[]  = "/v1/audio/speech";
static constexpr char OPENAI_TTS_MODEL[] = "tts-1";
static constexpr char OPENAI_TTS_VOICE[] = "alloy";

// Language key map (keyboard char → language config)
// whisperLang: ISO 639-1, empty string = auto-detect (used for E key)
struct LangKey {
    char key;
    const char* whisperLang; // ISO 639-1, "" = auto
    const char* pairLang;    // the non-English partner language name (for Claude prompt)
    const char* displayName;
};

static constexpr LangKey LANG_KEYS[] = {
    { 'c', "cs", "Czech",      "Czech"     },
    { 'v', "vi", "Vietnamese", "Vietnamese"},
    { 'u', "uk", "Ukrainian",  "Ukrainian" },
    { 'r', "ru", "Russian",    "Russian"   },
    { 'm', "zh", "Mandarin",   "Mandarin"  },
    { 'j', "ja", "Japanese",   "Japanese"  },
    { 'k', "ko", "Korean",     "Korean"    },
    { 'e', "",   "",           "Universal" }, // auto-detect → English
};
static constexpr int LANG_KEY_COUNT = 8;
