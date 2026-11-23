# USB protocol v3 — PC is the display source

115200, 8N1; LF-terminated commands, one request in flight.

- `state`: `STATE {"protocol":3,"s":{...},"t":{...}}`.
  Includes sensor, effective brightness and whether the remote-frame lease
  is active. Wi-Fi password is omitted.
- `pixels <768 hexadecimal digits>`: **PC -> clock**, 128 raw RGB triples
  in physical LED order. The clock applies its existing brightness/power
  protection. `ok pixels` is sent only after `strip.show()`.
- `set <key> <value>` updates an existing setting; `ok` means accepted in RAM.
- `save` writes the settings to LittleFS; `ok` means the write succeeded.
  A failed write reports `error save`; the desktop must not claim persistence.
- `stop` ends remote mode and saves settings, then replies `ok`.
- `time <unix_seconds> <UTC_offset_minutes>` updates time/date/timezone.
- `frame`: read-only diagnostic dump of the actual output, `FRAME <768 hex>`.
  Not used to drive the desktop preview.
- `sample <hour> <minute> <milliseconds>`: deterministic diagnostic renderer,
  returns `SAMPLE <768 hex>` for PC/firmware equivalence tests.
- Original `?`, `T HH:MM`, `W` and `set` remain available.

## Disconnect behaviour

Remote frames renew a 1500 ms lease. Once frames stop arriving the normal
firmware renderer resumes using the last persisted settings. Frames never
erase or reset those settings. The desktop auto-saves changed fields with
a short debounce and flushes before a normal close/disconnect.

## Shared renderer

The native PC DLL compiles the same `render.cpp`, geometry and fonts with
the real FastLED 3.10.9 implementation. The device retains autonomous
rendering; the desktop does not ask the device to generate its preview.
The `sample`/`frame` diagnostics let explicit tests verify byte-for-byte
renderer equivalence and delivery of the PC-generated frame.

Telemetry writes use nonblocking chunks. Incoming frame assembly is also
nonblocking; partial/invalid frames are not displayed. Desktop COM I/O uses
Windows OVERLAPPED reads/writes to avoid blocking the Qt event loop.

## Flash safety

Use the existing `eagle.flash.4m1m.ld` layout. Preserve the filesystem at
0x300000..0x3FB000; do not erase_flash or uploadfs. A full 4 MB pre-upgrade
backup is in ../backups, with its SHA256 and privacy warning.
