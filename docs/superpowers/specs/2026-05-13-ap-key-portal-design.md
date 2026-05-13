# AP Web Portal for API Key Entry — Design Spec

## Context

API keys (Groq, Anthropic, OpenAI) are long strings that are impractical to type on the
Cardputer's small physical keyboard. The existing `ApiKeySetupMode` uses keyboard entry;
this spec replaces it entirely with a WiFi Access Point + web portal that the user accesses
from any phone or computer.

## Approach

ESP32 AP mode + Arduino WebServer library (no extra dependencies — built into ESP32 Arduino
core). No password on the AP. Fixed IP `192.168.4.1` (always the default ESP32 soft-AP
address — reliable on all OSes). Mobile-friendly HTML form served from PROGMEM.

## Interface Contract

`ApiKeySetupMode` interface is unchanged: `tick(AppState&)` returns `true` once all three
keys are saved. `main.cpp` requires no changes.

---

## Architecture

### Files Modified

| File | Change |
|------|--------|
| `src/modes/api_key_setup.h` | Rewrite: replace keyboard FSM with AP + WebServer |
| `src/modes/api_key_setup.cpp` | Rewrite: AP setup, WebServer routes, NVS save |

No other files change.

---

## Behaviour

### On Construction

1. `WiFi.softAP("Transupter-Setup")` — open AP, no password
2. `WebServer server(80)` started
3. Two routes registered:
   - `GET /` → serve HTML key-entry form
   - `POST /save` → parse args, validate non-empty, save to NVS, redirect to `/done`
   - `GET /done` → serve "Keys saved" confirmation page
4. Screen shows connection instructions (SSID + IP)

### On Each `tick()` Call

- `_server.handleClient()` — processes any pending HTTP request
- If `_saved` flag set (by POST handler): stop server, stop AP, return `true`
- Otherwise return `false`

### POST /save Handler

1. Read `groq`, `anthropic`, `openai` args from the POST body
2. If any are empty: respond with 400 and error message
3. Open NVS, write all three keys, close NVS
4. Update `AppState` fields and set `apiKeysSet = true`
5. Set `_saved = true`
6. Redirect to `/done`

### HTML Form

Served from a PROGMEM `const char[]` string. Requirements:
- Mobile-friendly (viewport meta, large input fields, readable font)
- Three `<input type="password">` fields: Groq, Anthropic, OpenAI
- "Show / Hide" toggle per field so user can verify what they typed
- Submit button POSTs to `/save`
- Dark background to match Cardputer aesthetic (optional but nice)

---

## Screen Display

While the portal is active, the Cardputer screen shows:

```
┌─────────────────────────────┐  ← 18px status bar
│ NoWi  SETUP          100%   │
├─────────────────────────────┤
│                             │
│  Connect to WiFi:           │
│  > Transupter-Setup         │
│                             │
│  Then open browser:         │
│  > 192.168.4.1              │
│                             │
│  Waiting for keys...        │
└─────────────────────────────┘
```

---

## Error Handling

- If any submitted field is empty → HTTP 400, form shows error, user re-submits
- If NVS write fails (storage full) → HTTP 500, message on form
- WebServer and AP are always stopped when `tick()` returns `true` — no resource leak

---

## What Is NOT Changed

- `main.cpp` — no changes
- WiFi Setup Mode — unaffected
- All translator/utilities/history code — unaffected
- NVS key names — unchanged (`key_groq`, `key_anthropic`, `key_openai`)

---

## Verification

1. Flash updated firmware
2. First boot with no saved API keys: screen shows "Transupter-Setup" + "192.168.4.1"
3. Connect phone/laptop to "Transupter-Setup" WiFi
4. Open `192.168.4.1` in browser — form appears
5. Enter all three keys, submit
6. Browser shows "Keys saved — you can close this tab"
7. Cardputer screen transitions to Translator mode
8. Reboot — keys persist (loaded from NVS, no setup shown)
9. Test with one empty field — form rejects with error
