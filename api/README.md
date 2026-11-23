# BallClock HTTP API v1

The desktop and a future Android/iOS client can use the same API. It is
independent of Qt and the USB protocol. OpenAPI: [`openapi.yaml`](openapi.yaml).

Background modes: `bmode=0` off, `1` gradient, `2` noise, `3` solid.
Solid uses independent six-digit RGB `bcolor` and existing `bbright` (0..255).
These settings persist like other visual settings and never reconfigure Wi-Fi.
Active night mode intentionally disables all backgrounds, including solid.

### Clock-owned named gradients (Android 1.6 / matching firmware)

State/settings contain eight slots `grad0`..`grad7`. An empty string is a free
slot; otherwise the encoding is `Name|rrggbb,rrggbb,rrggbb,rrggbb` (four evenly
spaced color stops). Names use valid UTF-8, 1..64 bytes, without `|`, control or
bidi-control characters, or leading/trailing ASCII spaces. Presets are bounded
to 95 bytes including data (actual maximum 92 bytes with a 64-byte name).

Save a preset through the regular settings endpoint, select `pal=9+slot` and
`bmode=1`, then POST `/api/v1/save` and require `saved=true`. `pal=0..8` still
selects the original built-in palettes. The custom palette also supports noise
mode (`bmode=2`). To delete, write an empty slot value and select another palette.
Reading `/api/v1/state` on any client returns the shared clock-owned library;
it is not stored only in a phone. Same-slot concurrent writes are last-writer-wins.
The settings file is written to a temporary LittleFS file then atomically
renamed over the previous settings. Failed writes/renames return save failure.
Wi-Fi mode is never reapplied by preset/color edits.

The APK and embedded browser page now share `source/android/assets/mobile.html`;
`firmware/tools/gen_page.py` compresses that source during firmware builds.

### Drawing pictures (Android 1.7 / matching firmware)

`bmode=4` selects drawing; `drawSlot=0..7` picks a clock-owned picture, and
`drawDigits=0/1` hides/shows the configured clock font above it. Night mode
still suppresses the drawing background. `bbright` scales picture brightness;
the existing global brightness/current limit applies after compositing digits.

Normal state has top-level `art.names` (8 names, empty means free) and, only
in drawing mode, `art.data` for the selected slot. This stays below the existing
8192-byte USB receive limit, including a full gradient library. Read another
slot without selecting it: `GET /api/v1/art?slot=N` -> `{slot,name,data}`.
`POST /api/v1/art` accepts form fields `slot`, `name`, `data`, validates the
complete picture and atomically saves `/artN.txt` using `/art.tmp` + rename.
Require `saved:true`; this does not by itself select the picture. Apply/save
`drawSlot` and `bmode` through normal settings. Names use the gradient UTF-8
restrictions (64 bytes maximum). `POST /api/v1/art/delete` accepts `slot` and
confirms deletion. Writes require the normal X-BallClock-Client header/origin
and respect the active remote-frame controller lease. Wi-Fi is not reconfigured.

`data` is 128 records in physical LED index order, each 29 ASCII characters:
`epphhRRGGBBRRGGBBRRGGBBRRGGBB`. `e=0` off, `1` static first color,
`2` cyclic smooth interpolation through four colors, `3` step/blink through
four colors. `pp` is a two-digit hexadecimal full-cycle duration in tenths of
a second (1..255 for animation); `hh` is a two-digit phase offset (0..255).
Four RGB stops follow, six hex digits each. Repeat A,B,A,B for two-color blink.
Animation uses device uptime modulo each pixel's period; the phase is relative,
not a battery-backed timeline across a power loss. Empty slots render black.

USB v3 additive commands: `art put N Name|<3712 chars>` and `art delete N`,
acknowledged by `ok` only after persistence. Input is bounded to 4096 chars;
oversized commands return `error command_too_long` rather than truncating into
a valid partial picture. Qt generates drawing frames through the shared codec
and renderer; Android edits a local draft and never streams frames to the clock.

## Connection and security

- Base URL: `http://<clock-ip>` or `http://ballclock.local` on the same LAN.
- Direct mode (default): connect the PC/phone Wi-Fi to the open `BallClock`
  AP, then use `http://192.168.4.1`. No router or internet is needed. Keep the
  clocks powered. The AP is open and nearby clients can control it.
- `apOnly=1` selects direct AP without background router scans. `apOnly=0`
  selects router+AP mode: BallClock remains enabled even after router connection.
  Save and POST `/api/v1/wifi/connect` (or reboot) to
  apply a changed network mode. SSID/password fields refer to the router,
  not to an AP password. USB control remains available in either mode.
- Healthy network applies do not restart AP/DHCP. Direct-AP mode rejects
  legacy `/api/scan` with HTTP 409 to avoid channel hopping. Router association
  shares the ESP8266 radio and may change the AP channel to the router's channel.
  No software can guarantee RF availability during power/hardware failures.
- ESP8266 supports 2.4 GHz Wi-Fi. The PC may use Ethernet to the same router.
- This release is for a **trusted private network**, not the public internet.
  HTTP is unencrypted; there is no user authentication. Do not forward port 80
  from the router. Internet access needs a VPN or a separate authenticated gateway.
- `X-BallClock-Client: <unique-id>` is required on v1 writes. Use a UUID containing
  letters/digits/hyphens/underscores (1..64 characters). It is a controller ID,
  **not a password or security token**.
- During a remote-frame lease, another controller receives HTTP 409 instead of
  overwriting the frame/settings. Stop the old controller or wait for its lease.
- No wildcard CORS. Cross-origin browser writes are rejected. Native mobile
  clients do not need CORS and normally do not send an `Origin` header.
- Passwords are never returned by state/info, logged by Qt or saved in its profile.

## Requests

All write bodies are `application/x-www-form-urlencoded`, encoded as UTF-8.
Percent-encode values correctly: literal `+` must be `%2B`, `&` must be `%26`.

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/v1/info` | Identity, API version, LED count and frame format |
| GET | `/api/v1/state` | Current settings `s` and live telemetry `t` |
| POST | `/api/v1/settings` | Validate the whole batch, then update supplied settings |
| POST | `/api/v1/save` | Persist settings to LittleFS; `saved:true` confirms the write |
| POST | `/api/v1/frame` | Display 128 raw RGB triples supplied by the client |
| GET | `/api/v1/frame` | Read-only actual displayed RGB for the Android preview; no streaming lease |
| POST | `/api/v1/stop` | End this client's stream and persist settings |
| POST | `/api/v1/time` | Set Unix seconds and UTC offset in minutes |
| POST | `/api/v1/wifi/connect` | Save and connect using the stored SSID/password |

State telemetry additionally reports `uptimeMs`, `freeHeap` and `apStarts`.
The last value counts AP start/recovery attempts; ordinary visual settings and
reapplying a healthy network mode do not increment it. Combine with uptime to
distinguish AP reconfiguration from a device reboot or an Android-only disconnect.

Settings are exactly the names from firmware `FIELDS`: `font`, `color`, `lead`,
`blink`, `bmode`, `pal`, `bbright`, `speed`, `scale`, `angle`, `autoB`, `bright`,
`bmin`, `bmax`, `adcMin`, `adcMax`, `night`, `ncolor`, `nthr`, `gmt`, `ntp`,
`showIp`, `apOnly`, `ssid`, `pass`. Colours are six hex digits without `#`.
`pass` is write-only. Empty password explicitly clears the stored password;
omit it to keep the old value. Qt's empty password field omits it.

### Settings-only mobile client

1. GET state and verify `device:"ballclock"`, `api_version:1`.
2. POST only edited fields to settings.
3. POST save and require `ok:true,saved:true` before claiming persistence.
4. Let the clock render autonomously. A mobile client does **not** have to stream
   frames merely to change the font, gradient, brightness or other settings.

```powershell
curl.exe http://192.168.1.50/api/v1/state
curl.exe -H "X-BallClock-Client: mobile-example" -d "bmode=1&pal=1&scale=50&bright=120" http://192.168.1.50/api/v1/settings
curl.exe -H "X-BallClock-Client: mobile-example" -X POST http://192.168.1.50/api/v1/save
```

### PC/mobile frame source

POST frame with one field: `rgb=<768 hexadecimal characters>`: R,G,B for LED 0,
then LED 1, ... LED 127. Order and geometry are in `firmware/src/geometry.cpp`.
The clock keeps its brightness and current-limit protection. `displayed:true`
is returned only after output to the strip. Frames renew a 1500 ms lease.
After a clean stop or a lost connection, autonomous rendering resumes with
the last saved settings. A stopped PC/mobile app is not required to keep
the selected settings active.

### Android preview without PC-style streaming

The Android app v1.1 targets GET `/api/v1/frame` at up to 20 frames/second, paints the
same 128-circle staggered geometry as the Qt widget, and uses the clock's
web page for settings. The response includes identity, version, LED count,
and 768-character `rgb`. Unlike POST frame, these colors are **already**
brightness/current limited: do not apply brightness a second time.
Version 1.1 interpolates received colors only in the phone UI for smoother
transitions at display refresh. Actual network rate depends on response latency.
This GET never takes ownership, changes settings, or sets time. Closing the
app stops preview reads but leaves clock rendering and timekeeping autonomous.
`GET /api/v1/info` advertises `display_readback:true` on supporting firmware.

## Errors

- 400: bad field, value, frame, time, empty settings or missing client ID.
- 403: cross-origin browser mutation denied.
- 409: another controller currently owns the stream.
- 500: settings could not be written to flash.

Errors have `{"ok":false,"error":"<machine-readable-code>"}`. A timeout is
not a persistence acknowledgement; read state and retry deliberately.
Telemetry `t.saved=1` means there are no pending settings writes. The firmware
saves received edits after a two-second idle period without needing any client
to stay connected. A client must still flush its own unsent edits before leaving.
The built-in phone page offers an explicit save button and initializes the clock
from phone time if unsynchronized. Network disconnect does not stop the internal
clock; power loss does reset time without a battery-backed RTC.
Old `/api/state`, `/api/set`, Wi-Fi web routes and wired USB commands remain
available for compatibility.
