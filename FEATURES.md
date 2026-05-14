# Transupter — Feature Specification

A pocket-sized handheld voice translator. Hold a key, speak in any supported language,
release, and within a few seconds the original speech appears on screen alongside its
translation. The device speaks the translation aloud.

---

## 1. Core Interaction Model

### 1.1 Hold-to-Translate
- Each language is assigned to a physical key on the device keyboard.
- The user holds the assigned key and speaks. Recording begins immediately on key press.
- When the key is released (or a maximum recording time is reached), the device
  processes the audio and returns the result.
- No additional button presses are required — one hold is a complete translation request.

### 1.2 Bidirectional Auto-Detection
- Each language key represents a pair: the assigned language and English.
- The device automatically detects which language was spoken and translates to the other.
  - Speak Czech → get English output.
  - Speak English → get Czech output.
- No direction toggle or shift key is needed. The user simply speaks.

### 1.3 Universal English Key
- One key is designated "universal": hold it and speak in any language, and the output
  is always English.
- This is the "what did they just say?" key. It works for any language, including
  languages without dedicated keys.

---

## 2. Supported Languages

Eight keys are available by default:

| Key | Language Pair |
|-----|---------------|
| C   | Czech ↔ English |
| V   | Vietnamese ↔ English |
| U   | Ukrainian ↔ English |
| R   | Russian ↔ English |
| M   | Mandarin ↔ English |
| J   | Japanese ↔ English |
| K   | Korean ↔ English |
| E   | Universal → English (any language) |

The set of supported languages and key bindings is intended to be configurable.

---

## 3. Processing Pipeline

When the user releases the key after speaking, the device performs these steps in order:

1. **Transcription** — The recorded audio is sent to a speech-to-text service. The
   service returns both the transcript text and the detected spoken language.
2. **Direction resolution** — The detected language determines the translation direction
   (e.g., if Czech was detected on the C key, translate to English; if English was
   detected, translate to Czech).
3. **Translation** — The transcript is sent to a translation service with the target
   language specified. The translation is returned as plain text.
4. **Display** — Both the original transcript and the translation are shown on screen
   simultaneously.
5. **Text-to-speech** — The translated text is sent to a speech synthesis service, and
   the resulting audio is played through the device speaker.
6. **History** — The transcript/translation pair is appended to the on-device history log.

If any step fails, an error is shown on screen. The device returns to the ready state
and is immediately usable again without rebooting.

---

## 4. Display

### 4.1 Status Bar (always visible)
A persistent bar at the top of the screen shows:
- Current mode name
- WiFi connection status and signal strength
- Battery level and charging indicator
- Clock (time of day, once synced)

### 4.2 Translator Screen (main screen)
- Idle state: shows the list of language key bindings as a quick reference.
- Recording state: shows a countdown timer and a live sample count to confirm the
  microphone is active.
- Processing state: shows a progress indicator for each pipeline step
  ("Connecting…", "Transcribing…", "Translating…", "Speaking…").
- Result state: shows the source transcript on one line and the translated text below
  it. Long translations wrap across multiple lines.

### 4.3 Error Display
- Errors are shown in a distinct visual style (popup or highlighted region).
- The error message includes enough detail to distinguish connection failures,
  authentication failures, and empty/silent recordings.
- The device returns to idle automatically after a short error display period.

---

## 5. Audio

### 5.1 Recording
- Recording starts immediately when a language key is held.
- A maximum recording time cap prevents unbounded recordings (recommended: 5 seconds).
- The device falls back gracefully if preferred high-capacity memory is unavailable,
  using a shorter recording window (minimum: 2 seconds) rather than failing entirely.
- Silent or near-silent recordings are detected and reported as "Nothing recorded"
  rather than sending empty audio to the transcription service.

### 5.2 Playback
- The translated text is synthesized into speech and played through the built-in speaker.
- Playback does not block subsequent recordings — after playback completes, the device
  is immediately ready for the next translation.
- TTS failure is non-fatal: if synthesis or playback fails, the translation text is
  still shown on screen and the device remains functional.

---

## 6. Connectivity

### 6.1 WiFi
- The device connects to a saved WiFi network on boot.
- If no network is saved, a setup flow is offered on first boot.
- Connection is attempted with a countdown; the user can skip WiFi and use the device
  in offline mode (history browsing still works; translation requires connectivity).
- WiFi credentials are stored on-device and persist across reboots.

### 6.2 API Services
- Three external services are used: speech-to-text, translation, and text-to-speech.
  Each requires an API key.
- API keys are entered once via a setup flow and stored on-device; they are not
  re-entered on subsequent boots.
- The setup flow is accessible via a browser-based portal hosted by the device itself
  (the device temporarily acts as a WiFi access point), making it easy to enter long
  keys from a phone or laptop rather than the device keyboard.
- Keys can be updated at any time via the Utilities mode.

---

## 7. Utilities Mode

A secondary mode accessible from the translator screen via a dedicated key combination.
Contains:

### 7.1 Translation History
- A scrollable log of the last 50 translation results (source text + translated text +
  language key used + timestamp).
- History persists across reboots.
- History can be cleared from within the Utilities menu.

### 7.2 WiFi Setup
- Scans for available networks and displays them in a list with signal strength.
- The user can select a network and enter the password.
- Alternatively, the user can trigger the browser-based portal to configure WiFi from
  a phone or laptop.

### 7.3 API Key Setup
- Re-launches the browser-based portal so keys can be updated without factory reset.

### 7.4 Device Info
- Shows current IP address, WiFi signal strength, battery level, free memory, and
  firmware version.

---

## 8. First-Boot Setup Flow

1. **WiFi** — If no WiFi credentials are saved, the user is prompted to connect.
   The device can host a temporary WiFi access point with a browser portal for easy
   credential entry from another device.
2. **API Keys** — If any required API keys are missing, the user is prompted to enter
   them. Same browser portal mechanism.
3. **Clock sync** — After WiFi connects, the device attempts to sync the clock from
   the internet. If sync fails, the clock is left unset; the device is still usable.
4. **Main screen** — Once setup is complete, the translator screen is shown.

On subsequent boots, all saved settings are loaded and the translator screen appears
directly (with a brief WiFi connection attempt that can be skipped with any key press).

---

## 9. Battery Management

- Battery level is shown in the status bar and updated frequently.
- A warning popup appears when the battery falls below a low threshold (e.g., 10%).
- The warning is shown once per session and does not block usage.
- (Optional) A critical-battery shutdown at a very low threshold (e.g., 5%) to
  prevent data loss from unclean power-off.

---

## 10. Robustness Requirements

- **No hangs**: Every network operation has an explicit timeout. The device always
  returns to the ready state within a bounded time, even if all retries fail.
- **Automatic retry**: Transient network failures trigger up to 2 automatic retries
  before reporting an error to the user.
- **Crash recovery**: A hardware watchdog ensures the device reboots and reconnects
  if a true hang occurs (target: never needed in practice).
- **Memory bounds**: All audio and response buffers have explicit size limits.
  Oversized responses are truncated and reported rather than crashing the device.
- **Offline graceful degradation**: If WiFi is not connected, the translator screen
  is still shown with a clear "offline" indicator. History browsing works; translation
  attempts show a clear "no WiFi" error rather than hanging.

---

## 11. Security Notes (for implementors)

- API keys are sensitive credentials and should be stored in a protected region of
  device storage, not in plain flash accessible without authentication.
- HTTPS connections to all external services should validate server certificates
  rather than accepting any certificate. Certificate pinning or a trusted root CA
  bundle is preferred.
- The browser-based setup portal (AP mode) should be served only while in setup mode
  and shut down immediately afterward; it should not be running during normal operation.
- SSID names and other user-visible strings from external sources (WiFi scan results,
  API responses) must be sanitised before being rendered in any HTML or interpreted context.

---

## 12. Out of Scope (explicit exclusions)

The following are intentionally not included in this feature set:

- **Games or entertainment modes** — the device is a translator, not a gaming device.
- **Phrase caching / offline translation** — all translation goes through cloud services;
  there is no local language model.
- **Conversation mode** — the device is not a two-way live interpreter; it translates
  one utterance at a time.
- **Account management** — there is no user account, login, or cloud sync.
- **Over-the-air firmware updates** — firmware is updated by direct connection to a
  development computer.
