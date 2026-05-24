# SlopOS T-Deck — Agent Onboarding

**Do not modify this file in PRs.** It is AI context. Only the repo owner updates it directly.

---

## What Is This

Standalone T-Deck LVGL firmware that runs in the MeshCore mesh network. Think "Discord UI on a LoRa radio." Full mesh protocol compatibility — interoperates with any MeshCore node.

Three repos form the SlopOS ecosystem:

| Repo | What | Stack |
|------|------|-------|
| `hermes-gadget/SlopOS` | MeshCore fork (core library) | C++/PlatformIO, ESP32 |
| **`hermes-gadget/SlopOS-tdeck`** ← **you are here** | T-Deck LVGL firmware | C++/PlatformIO, LVGL v9, LovyanGFX |
| `hermes-gadget/SlopOS-client` | Flutter mobile app | Dart/Flutter, BLE/USB/TCP |

---

## Quick Start

```bash
git clone --recurse-submodules git@github.com:hermes-gadget/SlopOS-tdeck.git
cd SlopOS-tdeck

# Run all native tests (no hardware, fast — do this repeatedly)
pio test -e native_test -v

# Run one test module
pio test -e native_test -f test_keyboard -v

# Build firmware
pio run -e SlopOS_TDeck
```

MeshCore is at `lib/meshcore/` — a git submodule. `git submodule update --init` if you cloned without `--recurse-submodules`.

---

## Architecture

```
src/
├── main.cpp               # Boot: board→display→mesh→UI
├── lv_conf.h              # LVGL v9 config (16-bit, partial render)
├── hal/
│   ├── tdeck_pins.h       # Full T-Deck pinout + SPI/I2C aliases
│   ├── tdeck_board.h      # MainBoard impl (sleep, battery, power)
│   ├── display.cpp/h      # LovyanGFX ST7789 + LVGL + touch/keyboard callbacks
│   ├── battery.cpp/h      # ADC mV→%
│   ├── touch.cpp/h        # GT911 capacitive touch (I2C, 400 kHz)
│   ├── keyboard.cpp/h     # I2C keyboard (ESP32-C3 MCU at 0x55)
│   ├── prefs.cpp/h        # NodePrefs persisted in NVS (freq, SF, power, etc.)
│   └── gps.cpp/h          # GPS NMEA parser
├── mesh/
│   ├── slop_mesh.h        # Mesh subclass — routing, channels, message handling
│   └── mesh_wrapper.cpp/h # Public API for the UI layer
├── ui/
│   ├── theme.h            # Colors, pixel helpers (apply_pixel_*)
│   ├── responsive.h       # Display-size-agnostic layout helpers
│   ├── home_screen.cpp/h  # 4x3 icon grid, top/bottom bars
│   ├── chat_screen.cpp/h  # Channels, DM, message bubbles
│   ├── screens.cpp/h      # All 11+ other screens
│   ├── navigation.cpp/h   # Screen routing with slide transitions
│   └── ui.cpp/h           # Splash→Home transition
├── app/
│   └── map_renderer.cpp/h # Offline map (SD card tiles, TJpgDec, PSRAM canvas)
└── lib/meshcore/          # Git submodule → MeshCore
```

---

## Hardware (LilyGo T-Deck)

| Peripheral | Interface | Key Details |
|------------|-----------|-------------|
| **ESP32-S3** | MCU | 240 MHz, 16 MB flash, 8 MB PSRAM |
| **LoRa SX1262** | SPI (shared) | NSS=9, SCK=40, MISO=38, MOSI=41, DIO1=45, RST=17, BUSY=13 |
| **Display ST7789** | SPI (shared) | CS=12, DC=11, backlight=42, 320x240. Native orientation = 240x320 portrait + rotation(1) |
| **Touch GT911** | I2C (0x5D) | 400 kHz, coordinate transform: SWAP_XY + MIRROR_X for rotation(1) |
| **Keyboard** | I2C (0x55) | ESP32-C3 MCU, 100 kHz. Key mode returns ASCII. Backlight via 0x01/0x02 commands |
| **Trackball** | GPIO | UP=3, DOWN=15, LEFT=1, RIGHT=2, CLICK=0. 5-direction + center press |
| **Battery ADC** | GPIO 4 | Voltage divider, ADC_MULTIPLIER = 2 × 3.3 × 1000 |
| **Peripheral Power** | GPIO 10 | HIGH = peripherals on |
| **SD Card** | SDMMC | CMD=21, CLK=14, D0=47, D1=48, D2=16, D3=15 |

**Shared SPI bus:** Display and LoRa share SCK(40)/MOSI(41)/MISO(38) with different CS lines. SPI must be initialized once, not re-begun.

---

## UI Conventions

### Pixel / Blocky Theme

Deep black `#0F0F0F` background, cyan `#00BFFF` accents, zero radius, 2px minimum borders.

Theme helpers in `src/ui/theme.h`:
- `apply_dark_bg(obj)` — sets full-screen black
- `apply_pixel_card(obj)` — `BG_TERTIARY`, 0 radius, 2px border
- `apply_pixel_btn(obj)` — filled `ACCENT` button
- `apply_pixel_btn_outline(obj)` — transparent with `ACCENT` border
- `apply_pixel_input(obj)` — input field, `BG_INPUT`, 2px border
- `apply_topbar_icon_btn(btn)` — themed back button styling, state-aware

All screens MUST call `apply_dark_bg()` and use helpers from `theme.h`. Do not hardcode colors.

### Screen Structure

Each screen has:
1. **Top bar** — `BG_SECONDARY`, 22px high, ← back button, channel hashtag snapshot, 24h time
2. **Content area** — between top bar and bottom bar
3. **Bottom bar** — `BG_SECONDARY`, 20px high, device name/signal/battery

The `make_screen_full(title)` helper in `screens.cpp` creates all of this. Use it.

### Navigation

- `navigate_to(Screen)` — switches screens with slide animation, tracks previous
- `go_back()` — returns to previous screen (nav stack, 8-entry circular buffer)
- `show_screen(scr)` — loads a screen created with `lv_obj_create(nullptr)`
- `can_go_back()` — returns true if there's a previous screen to return to

### Icons

Use `LV_SYMBOL_*` (FontAwesome bundle built into LVGL v9):

| Screen | Symbol |
|--------|--------|
| Chat | `LV_SYMBOL_ENVELOPE` |
| Contacts | `LV_SYMBOL_CALL` |
| Heard | `LV_SYMBOL_BELL` |
| Map | `LV_SYMBOL_GPS` |
| Settings | `LV_SYMBOL_SETTINGS` |
| Terminal | `LV_SYMBOL_KEYBOARD` |
| Hamburger (≡) | `LV_SYMBOL_LIST` |
| Back (←) | `LV_SYMBOL_LEFT` |

### All Screens

| # | Screen | Source | Status |
|---|--------|--------|--------|
| 0 | Splash | `ui.cpp` | ✅ |
| 1 | Home (4x3 grid) | `home_screen.cpp` | ✅ |
| 2 | Chat (channels + DM) | `chat_screen.cpp` | ✅ |
| 3 | Contacts (tap→DM) | `screens.cpp` | ✅ |
| 4 | Repeaters (→Heard) | `screens.cpp` | ✅ |
| 5 | Finder | `screens.cpp` | ✅ |
| 6 | Heard (live RSSI) | `screens.cpp` | ✅ |
| 7 | Map (touch pan, SD tiles) | `screens.cpp` | ✅ |
| 8 | Advertise | `screens.cpp` | 🟡 status never updates |
| 9 | Settings (radio, keyboard BL, date/time) | `screens.cpp` | ✅ |
| 10 | Trace | `screens.cpp` | ✅ |
| 11 | Terminal | `screens.cpp` | ✅ |
| 12 | Noise floor bar | `screens.cpp` | ✅ |
| 13 | Signal (RSSI, SNR, params) | `screens.cpp` | 🟡 radio params hardcoded |

---

## Mesh Integration

MeshCore is a submodule at `lib/meshcore/`. The UI never touches MeshCore directly — all calls go through `slopos::mesh::*` in `mesh_wrapper.h`.

**Key mesh wrapper API:**

```cpp
slopos::mesh::init(spiffs_ok)          // Boot — load identity, start radio
slopos::mesh::loop()                   // Call from main loop
slopos::mesh::sendMessage(name, text)  // Direct message
slopos::mesh::sendChannelMessage(ch, text)  // Group message
slopos::mesh::addChannel(name, psk_b64)     // PSK channel
slopos::mesh::addHashtagChannel(name)       // Hash-of-name channel
slopos::mesh::exportContacts(out, max)      // Name list
slopos::mesh::exportChannels(out, max)      // Channel name list
slopos::mesh::getNoiseFloor()              // Current noise floor dBm
```

**Messages arrive** via `chat_screen_add_msg(channel, sender, text, is_self)`. The chat screen maintains per-channel message caches (8 messages each, 16 channels max).

**Channel protocol:**
- **PSK channels:** "Public" uses PSK `izOH6cXN6mrJ5e26oRXNcg==`. Any node with the matching PSK derives the same channel hash.
- **Hashtag channels:** No PSK needed. Channel hash = SHA256(SHA256(name)). Any node using the same hashtag name creates the same hash.
- Group messages embed `"<sender_name>: <text>"` for interop with MeshCore companion radio firmware.

---

## Testing

```bash
pio test -e native_test -v       # All 171 tests (no hardware)
pio test -e native_test -f test_touch -v     # One module
```

**Critical rules:**
- Tests use `test/test_<name>/` dirs with `main.cpp` entry points. Wrong naming = not discovered.
- Hardware is mocked in `test/mocks/` (lvgl.h, Arduino.h, RadioLib.h, etc.)
- `MockMeshMessage.channel` field added recently — tests match production struct layout.
- Always run tests before pushing. A PR with failing tests is rejected.

**New code = new tests.** Minimum:
- HAL changes → add mock + test
- New screen → navigation test + message display test
- API changes → update existing mock stubs

---

## Build & Tools

```bash
# Build firmware
pio run -e SlopOS_TDeck

# Monitor
pio device monitor -b 115200

# Debug build (extra serial output)
pio run -e SlopOS_TDeck_debug
```

---

## Versioning & Release

Main + dev branch model:
- `dev` — integration branch. All PRs merge here.
- `main` — stable releases only.
- Tags: `beta-0.1.XX` (zero-padded for correct sort: `beta-0.1.09` not `beta-0.1.9`)

**Release flow:**
1. Update `SLOPOS_VERSION` in `tdeck_pins.h`
2. `pio run -e SlopOS_TDeck`
3. `cp .pio/build/SlopOS_TDeck/firmware-merged.bin firmware/firmware-merged.bin`
4. Commit, tag, push, `gh release create`

---

## PR & Review Workflow

1. List PRs: `gh pr list --repo hermes-gadget/SlopOS-tdeck --state open`
2. Check diff: `gh pr diff N` or `git fetch origin pull/N/head:pr-N && git diff dev...pr-N`
3. Build: `pio run -e SlopOS_TDeck`
4. Test: `pio test -e native_test -v` (must pass 171/171)
5. Review: check logic, edge cases, buffer overflow, strncpy null termination, `\\n` literal bugs, missing scroll disables, dangling pointers after auto-delete
6. Merge: `gh pr merge N --squash --delete-branch --repo hermes-gadget/SlopOS-tdeck`
7. If merge fails (conflicts): cherry-pick new commits only, or squash-merge locally
8. If PR branch has stale commits: cherry-pick new commits onto dev, close PR

### Rejection triggers
- Unconditional `Serial.printf` without `#if defined(SLOPOS_DEBUG)` guard
- Test suite not passing
- Hardcoded colors instead of theme constants
- Missing `apply_dark_bg()` on screen backgrounds
- `\\\\n` literal instead of real newline
- Stack-local arrays used as LVGL event user_data

---

## Gotchas & Pitfalls

| Gotcha | Details |
|--------|---------|
| Display init | ST7789 native = 240x320 portrait. Set panel 240x320, then `rotation(1)` for 320x240 landscape |
| Backlight | `_panel.setLight(&_light)` before `setPanel(&_panel)` — missing = black screen |
| LVGL tick | `lv_tick_set_cb()` after `lv_init()` — missing = refresh never fires |
| Touch coords | After rotation(1): SWAP_XY=false, MIRROR_X=true |
| Keyboard | `Wire.read()` returns `int`, store in `int` not `char` — 0xFF vs -1 collision |
| strncpy | Does NOT null-terminate if source >= n. Always `dest[n-1] = '\0'` |
| Auto-delete | `lv_scr_load_anim(..., true)` deletes old screen + all children. Register `LV_EVENT_DELETE` to null globals |
| `lv_obj_del` in handler | Use `lv_obj_del_async()` — event loop may dereference freed object |
| Stack in user_data | `lv_obj_add_event_cb(btn, handler, LV_EVENT_CLICKED, (void*)&local_array[i])` — garbage on click. Use `static` arrays |
| Radio init | `P_LORA_*` prefix, `Module*` constructor, SX126X_DIO3_TCXO_VOLTAGE=1.8f and other defines from variant |
| PSRAM LVGL | `LV_MEM_CUSTOM` with `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` — default 48 KB won't render maps |
| Webflasher bootloop | `flash_mode: "keep"` in manifest — the bootloader header is DIO (0x02), esptool.js writing "qio" breaks it |
| constexpr on ESP32 | xtensa GCC rejects multi-statement constexpr. Use `inline` for any non-trivial function |
| SPIFFS failure | If SPIFFS fails, identity is ephemeral — don't try to save on every boot |
| Radio first boot | Init radio with compile-time defaults for receive. Gate TRANSMIT behind `configured == true` |
| Wire.endTransmission | Always check return value — NACK on keyboard init means keyboard MCU is dead |
| saveState | Call every ~5 min from UI loop. Crashes lose new contacts if not saved |
