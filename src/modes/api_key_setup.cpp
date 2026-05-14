#include "api_key_setup.h"
#include "../config.h"
#include <WiFi.h>
#include <M5Cardputer.h>

// ── HTML pages stored in flash ─────────────────────────────────────────────

static const char PORTAL_HTML[] = R"HTML(<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Transupter Setup</title>
<style>
body{font-family:sans-serif;background:#1a1a2e;color:#eee;max-width:480px;margin:40px auto;padding:20px}
h1{color:#00d4ff;margin-bottom:6px}
p{color:#aaa;font-size:14px;margin-bottom:24px}
label{display:block;margin-bottom:4px;font-size:13px;color:#aaa}
.field{position:relative;margin-bottom:16px}
input{width:100%;padding:12px;background:#16213e;border:1px solid #0f3460;
      border-radius:6px;color:#eee;font-size:14px;box-sizing:border-box;font-family:monospace}
input:focus{outline:none;border-color:#00d4ff}
.tog{position:absolute;right:10px;top:50%;transform:translateY(-50%);
     background:none;border:none;color:#aaa;cursor:pointer;font-size:12px;padding:4px}
button[type=submit]{width:100%;padding:14px;background:#00d4ff;color:#000;border:none;
     border-radius:6px;font-size:16px;font-weight:bold;cursor:pointer;margin-top:8px}
button[type=submit]:active{background:#00aabb}
</style></head>
<body>
<h1>Transupter Setup</h1>
<p>Enter your API keys. They are stored on the device and never leave it.</p>
<form method="POST" action="/save">
  <div class="field">
    <label>Groq API Key</label>
    <input type="password" name="groq" id="g" placeholder="gsk_..." required>
    <button type="button" class="tog" onclick="t('g',this)">Show</button>
  </div>
  <div class="field">
    <label>Anthropic API Key</label>
    <input type="password" name="anthropic" id="a" placeholder="sk-ant-..." required>
    <button type="button" class="tog" onclick="t('a',this)">Show</button>
  </div>
  <div class="field">
    <label>OpenAI API Key</label>
    <input type="password" name="openai" id="o" placeholder="sk-..." required>
    <button type="button" class="tog" onclick="t('o',this)">Show</button>
  </div>
  <button type="submit">Save Keys &#8594;</button>
</form>
<script>
function t(id,btn){var i=document.getElementById(id);
  i.type=i.type==='password'?'text':'password';
  btn.textContent=i.type==='password'?'Show':'Hide';}
</script>
</body></html>)HTML";

static const char DONE_HTML[] = R"HTML(<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Transupter</title>
<style>
body{font-family:sans-serif;background:#1a1a2e;color:#eee;
     max-width:480px;margin:40px auto;padding:20px;text-align:center}
h1{color:#00ff88}p{color:#aaa;line-height:1.6}
</style></head>
<body>
<h1>&#10003; Keys Saved</h1>
<p>Your API keys have been saved to the device.<br>You can close this tab.</p>
</body></html>)HTML";

static const char ERROR_HTML[] = R"HTML(<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Transupter</title>
<style>
body{font-family:sans-serif;background:#1a1a2e;color:#eee;
     max-width:480px;margin:40px auto;padding:20px}
h1{color:#ff4444}a{color:#00d4ff}
</style></head>
<body>
<h1>&#10007; All fields required</h1>
<p><a href="/">&#8592; Go back and fill in all three keys</a></p>
</body></html>)HTML";

// ── Constructor ─────────────────────────────────────────────────────────────

ApiKeySetupMode::ApiKeySetupMode(Display& disp, Keyboard& kb, NvsStore& nvs)
    : _disp(disp), _kb(kb), _nvs(nvs) {

    WiFi.mode(WIFI_AP_STA);  // keep station mode if already connected
    WiFi.softAP("Transupter-Setup");

    _server.on("/",     HTTP_GET,  [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.on("/done", HTTP_GET,  [this]() { handleDone(); });
    _server.begin();

    drawPortalScreen();
}

// ── tick ────────────────────────────────────────────────────────────────────

bool ApiKeySetupMode::tick(AppState& state) {
    _state = &state;
    _server.handleClient();

    if (_saved) {
        _server.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        return true;
    }
    return false;
}

// ── Route handlers ──────────────────────────────────────────────────────────

void ApiKeySetupMode::handleRoot() {
    _server.send(200, "text/html", PORTAL_HTML);
}

void ApiKeySetupMode::handleSave() {
    String groq      = _server.arg("groq");      groq.trim();
    String anthropic = _server.arg("anthropic"); anthropic.trim();
    String openai    = _server.arg("openai");    openai.trim();

    if (groq.isEmpty() || anthropic.isEmpty() || openai.isEmpty()) {
        _server.send(400, "text/html", ERROR_HTML);
        return;
    }

    _nvs.begin();
    _nvs.putString(NVS_KEY_GROQ,      groq);
    _nvs.putString(NVS_KEY_ANTHROPIC, anthropic);
    _nvs.putString(NVS_KEY_OPENAI,    openai);
    _nvs.end();

    if (_state) {
        _state->keyGroq      = groq;
        _state->keyAnthropic = anthropic;
        _state->keyOpenAI    = openai;
        _state->apiKeysSet   = true;
    }

    _saved = true;
    _server.sendHeader("Location", "/done");
    _server.send(302, "text/plain", "");
}

void ApiKeySetupMode::handleDone() {
    _server.send(200, "text/html", DONE_HTML);
}

// ── Screen ──────────────────────────────────────────────────────────────────

void ApiKeySetupMode::drawPortalScreen() {
    _disp.clearContent();
    auto& tft = M5Cardputer.Display;
    tft.setTextSize(1);

    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 8);
    tft.print("Connect to WiFi:");
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 22);
    tft.print("> Transupter-Setup");

    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 42);
    tft.print("Then open browser:");
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 56);
    tft.print("> 192.168.4.1");

    tft.setTextColor(DARKGREY, BLACK);
    tft.setCursor(4, Display::CONTENT_Y + 80);
    tft.print("Waiting for keys...");
}
