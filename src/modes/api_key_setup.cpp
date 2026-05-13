#include "api_key_setup.h"
#include "../config.h"
#include <M5Cardputer.h>

ApiKeySetupMode::ApiKeySetupMode(Display& disp, Keyboard& kb, NvsStore& nvs)
    : _disp(disp), _kb(kb), _nvs(nvs) {
    drawEntry("Groq API Key", _input);
}

bool ApiKeySetupMode::tick(AppState& state) {
    auto ks = _kb.poll();

    if (ks.isDel && _input.length() > 0) {
        _input.remove(_input.length() - 1);
    } else if (ks.ch && !ks.isEnter) {
        _input += ks.ch;
    }

    // Redraw on any input
    if (ks.ch || ks.isDel) {
        switch (_step) {
            case Step::EnterGroq:       drawEntry("Groq API Key", _input); break;
            case Step::EnterAnthropic:  drawEntry("Anthropic API Key", _input); break;
            case Step::EnterOpenAI:     drawEntry("OpenAI API Key", _input); break;
            default: break;
        }
    }

    if (!ks.isEnter || _input.isEmpty()) return false;

    _nvs.begin();
    switch (_step) {
        case Step::EnterGroq:
            _nvs.putString(NVS_KEY_GROQ, _input);
            state.keyGroq = _input;
            _input = "";
            _step  = Step::EnterAnthropic;
            drawEntry("Anthropic API Key", _input);
            break;
        case Step::EnterAnthropic:
            _nvs.putString(NVS_KEY_ANTHROPIC, _input);
            state.keyAnthropic = _input;
            _input = "";
            _step  = Step::EnterOpenAI;
            drawEntry("OpenAI API Key", _input);
            break;
        case Step::EnterOpenAI:
            _nvs.putString(NVS_KEY_OPENAI, _input);
            state.keyOpenAI  = _input;
            state.apiKeysSet = true;
            _nvs.end();
            return true;
        default: break;
    }
    _nvs.end();
    return false;
}

void ApiKeySetupMode::drawEntry(const char* label, const String& current) {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);
    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 4);
    tft.printf("Enter %s:", label);
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 18);
    // Show last 28 chars of current input so it fits
    String disp = current.length() > 28 ? current.substring(current.length() - 28) : current;
    tft.print(disp + "_");
    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 34);
    tft.print("Enter=confirm  Del=backspace");
}
