# SlopOS T-Deck — Features Overview

**Standalone LoRa mesh messaging firmware for the LilyGo T-Deck (ESP32-S3 + SX1262).**

This document catalogs every feature in the firmware — the 12-grid home screen tiles, system infrastructure, and hardware drivers. Each entry includes a one-line description and a cross-reference to the relevant source file(s) or detail doc.

---

## Table of Contents

- [Overview](#overview)
- [Home Screen — 12-Grid Tiles](#home-screen--12-grid-tiles)
  - [CHATS](#1-chats)
  - [CONTACTS](#2-contacts)
  - [REPEATERS](#3-repeaters)
  - [FINDER](#4-finder)
  - [PACKETS](#5-packets)
  - [MAP](#6-map)
  - [ADVERTISE](#7-advertise)
  - [SETTINGS](#8-settings)
  - [TRACE](#9-trace)
  - [TERMINAL](#10-terminal)
  - [SETUP](#11-setup)
  - [SIGNAL](#12-signal)
- [System Features](#system-features)
  - [Display & LVGL](#display--lvgl)
  - [Mesh Networking](#mesh-networking)
  - [Screen Navigation](#screen-navigation)
  - [Pixel Theme System](#pixel-theme-system)
  - [Responsive Layout](#responsive-layout)
  - [Emoji Font Support](#emoji-font-support)
  - [NVS Preferences](#nvs-preferences)
  - [Diagnostics & Debug](#diagnostics--debug)
  - [Packet Log](#packet-log)
  - [RTC & System Time](#rtc--system-time)
  - [SPIFFS Persistence](#spiffs-persistence)
  - [Web Flasher Support](#web-flasher-support)
  - [Remote Test Controller](#remote-test-controller)
- [Hardware Features](#hardware-features)
  - [ST7789 Display](#st7789-display)
  - [GT911 Touch](#gt911-touch)
  - [I2C Keyboard](#i2c-keyboard)
  - [5-Direction Trackball](#5-direction-trackball)
  - [Battery ADC](#battery-adc)
  - [GPS NMEA Parser](#gps-nmea-parser)
  - [SD Card Storage](#sd-card-storage)
  - [LoRa SX1262 Radio](#lora-sx1262-radio)
  - [Buzzer](#buzzer)
  - [Peripheral Power](#peripheral-power)
- [Offline Map Renderer](#offline-map-renderer)

---

## Overview

| Layer | Description | Key Sources |
|-------|-------------|-------------|
| **UI** | Discord-inspired dark pixel interface, 12-tile home grid, chat, settings, and diagnostics screens | `src/ui/*` |
| **Mesh** | Full MeshCore protocol stack — routing, encryption, group channels, direct messages | `src/mesh/*`, `lib/meshcore/` |
| **HAL** | All T-Deck peripherals — display, touch, keyboard, trackball, GPS, battery, SD, buzzer, LoRa | `src/hal/*` |
| **Apps** | Offline map renderer with PNG tile decode and LRU PSRAM cache | `src/app/*` |
| **Boot** | Sequenced startup: board → display → mesh → UI → peripherals | `src/main.cpp` |

---

## Home Screen — 12-Grid Tiles

The home screen is a 4×3 adaptive icon grid with a top bar (channel hashtags, 24h time) and a bottom bar (device name, signal bars, battery %). Navigation uses the 5-direction trackball or capacitive touch.

See [`src/ui/home_screen.cpp`](../src/ui/home_screen.cpp), [`src/ui/home_screen.h`](../src/ui/home_screen.h).

### 1. CHATS
Full multi-channel chat with message bubbles, channel tabs, and text input. Supports hashtag channels and direct messages. Loads and persists per-channel message history from SPIFFS.
**Sources:** [`src/ui/chat_screen.cpp`](../src/ui/chat_screen.cpp), [`src/ui/chat_screen.h`](../src/ui/chat_screen.h)

### 2. CONTACTS
List of discovered mesh nodes (up to 64) with LRU eviction. Shows contact names and RSSI. Tapping opens a direct message conversation.
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp), [`src/mesh/slop_mesh.h`](../src/mesh/slop_mesh.h)

### 3. REPEATERS
Network / signal view showing nearby nodes sorted by signal strength. Routes to the Network screen.
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp)

### 4. FINDER
Active node discovery via zero-hop "Ping Nearby" (TTL=1 CONTROL packets). Collects PONG responses over a 3-second window. Shows responders with RSSI. 30-second cooldown between pings.
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp), [`src/mesh/slop_mesh.h`](../src/mesh/slop_mesh.h) (lines 65–390), [`docs/KNOWN_ISSUES.md`](KNOWN_ISSUES.md#finder)

### 5. PACKETS
Heard packets log — a running list of all received radio packets with timestamp, source, RSSI, SNR, and packet type (ADVERT, DM, GRP, TRACE, etc.).
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp), [`src/mesh/mesh_wrapper.h`](../src/mesh/mesh_wrapper.h) (lines 27–33, 77–79)

### 6. MAP
Offline map view using PNG tiles from SD card. Renders a tile grid on an LVGL canvas with pan and zoom. Uses PSRAM-backed tile cache (LRU, 4 entries, 64-bit timestamps). Shows own GPS position.
**Sources:** [`src/app/map_renderer.cpp`](../src/app/map_renderer.cpp), [`src/app/map_renderer.h`](../src/app/map_renderer.h), [`src/app/tile_cache.h`](../src/app/tile_cache.h)

### 7. ADVERTISE
Send a manual mesh advert broadcast to announce your node's presence on the network. Optionally includes GPS coordinates if a fix is available.
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp), [`src/mesh/mesh_wrapper.h`](../src/mesh/mesh_wrapper.h) (lines 61–64)

### 8. SETTINGS
Device configuration screen. Node name, radio parameters, about/version info. Access to Radio Setup (frequency, bandwidth, SF, CR, TX power) and Custom RF config.
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp), [`src/hal/prefs.h`](../src/hal/prefs.h)

### 9. TRACE
Network trace route — sends a trace packet along a known path to a contact and displays the per-hop SNR and node hashes returned.
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp), [`src/mesh/mesh_wrapper.h`](../src/mesh/mesh_wrapper.h) (lines 86–91), [`src/mesh/slop_mesh.h`](../src/mesh/slop_mesh.h) (lines 292–316)

### 10. TERMINAL
Serial-like terminal screen for raw text I/O. Accepts keyboard input, displays sent/received text. Max 64 lines with oldest-line pruning. Includes `dump`, `clear`, inject logs.
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp), [`src/ui/screens.h`](../src/ui/screens.h) (lines 29–33)

### 11. SETUP
First-boot onboarding wizard. Guides the user through node name, radio configuration, and channel setup before first use. Saves prefs and reboots when complete.
**Sources:** [`src/ui/onboarding_screen.cpp`](../src/ui/onboarding_screen.cpp), [`src/ui/onboarding_screen.h`](../src/ui/onboarding_screen.h), [`docs/KNOWN_ISSUES.md`](KNOWN_ISSUES.md#onboarding)

### 12. SIGNAL
Signal diagnostics screen showing current RSSI, noise floor, SNR, and signal quality metrics. Visual bar chart representation.
**Sources:** [`src/ui/screens.cpp`](../src/ui/screens.cpp), [`src/ui/theme.h`](../src/ui/theme.h) (lines 59–108)

---

## System Features

### Display & LVGL
- **LovyanGFX** driver for ST7789 240×320 TFT via SPI
- **LVGL v9.3.0** — full widget toolkit with partial renderer, 16-bit color, animation support
- **Auto-off** backlight timeout (30s default) with wake on touch/keyboard input
- **Programmable brightness** via PWM (0–255)
- **Screen capture** via serial hex dump (`lv_snapshot_take_to_draw_buf`)
- **Tick-starvation fix** — periodic `lv_timer_handler()` inside mesh loop (20ms guard) keeps UI responsive
**Sources:** [`src/hal/display.cpp`](../src/hal/display.cpp), [`src/hal/display.h`](../src/hal/display.h), [`src/lv_conf.h`](../src/lv_conf.h), [`docs/KNOWN_ISSUES.md`](KNOWN_ISSUES.md#ui-performance)

### Mesh Networking
- **Full MeshCore protocol** — interoperable with any MeshCore node
- **SX1262 LoRa radio** on shared SPI bus
- **Direct messages** (peer-to-peer encrypted) with path-aware routing (direct or flood)
- **Group channels** — hashtag channels with shared PSK (up to 8 channels)
- **Automatic contact discovery** via advert broadcasts (max 64 contacts, LRU eviction)
- **Path learning** — learns and stores outbound paths for direct routing
- **Trace route** — per-hop SNR and node hash reporting
- **Ping Nearby** — zero-hop active discovery with tagged PING/PONG exchange
- **Packet logging** — per-packet RX log with type, RSSI, SNR
- **RTC time sync** — mesh-synchronised clock for message timestamps
- **Advert broadcast** — manual send with optional GPS coordinates
**Sources:** [`src/mesh/mesh_wrapper.cpp`](../src/mesh/mesh_wrapper.cpp), [`src/mesh/mesh_wrapper.h`](../src/mesh/mesh_wrapper.h), [`src/mesh/slop_mesh.h`](../src/mesh/slop_mesh.h), [`lib/meshcore/`](../lib/meshcore/)

### Screen Navigation
- **Screen enum** with 14 screen IDs (Home, Chat, Contacts, Channels, Network, Heard, Map, Advertise, Settings, Trace, Terminal, Signal, RadioSetup, Onboarding)
- **Slide transitions** — configurable `lv_scr_load_anim` with direction and duration
- **Back stack** — linear stack (drops oldest when full, no wrapping), with `can_go_back()` and `go_back()`
- **Universal back-swipe** — two-swipe commit pattern: first Left neutralises, second Left triggers back
- **Top bar** — ← back button, channel hashtag snapshot, 24h time
- **Bottom bar** — device name, signal bars, battery percentage
**Sources:** [`src/ui/navigation.cpp`](../src/ui/navigation.cpp), [`src/ui/navigation.h`](../src/ui/navigation.h), [`src/ui/screens.h`](../src/ui/screens.h)

### Pixel Theme System
- **Discord-inspired dark palette** — deep black `#0F0F0F` background, cyan `#00BFFF` accents
- **Color constants** — 7 background levels, 7 accent colors, 4 text colors, channel colors
- **Style helpers** — `apply_dark_bg()`, `apply_pixel_card()`, `apply_pixel_btn()`, `apply_pixel_btn_outline()`, `apply_pixel_input()`, `apply_pixel_badge()`, `apply_topbar_icon_btn()`
- **Signal bars** — `create_signal_bars()` draws 1–5 blocky bar widgets, bottom-aligned, growing left to right
- **Focus style** — yellow accent border for keyboard/trackball focus state
- **Zero radius** on all elements, 2px minimum borders
**Sources:** [`src/ui/theme.h`](../src/ui/theme.h), [`test/test_theme/`](../test/test_theme/)

### Responsive Layout
- **Display-agnostic** — all layout proportional to `TFT_WIDTH`/`TFT_HEIGHT`
- **Adaptive grid** — `compute_grid()` selects 1–4 columns based on available width
- **Proportional bars** — top/bottom bar heights scaled to ~9% of display height (clamped 12–28px)
- **Dialog sizing** — `dialog_size()` caps to display minus margin, `capped_width()` limits as percentage
- **Column distribution** — `column_offsets()` template distributes width by proportional weights
**Sources:** [`src/ui/responsive.h`](../src/ui/responsive.h)

### Emoji Font Support
- **Noto Emoji subset** — compiled as LVGL font (16px, 4bpp grayscale AA)
- **Montserrat wrappers** — 8 size variants (10–28px) with emoji fallback registered
- **Fallback registration** — `emoji_font_register_fallback()` callable once at init
- **Indexed access** — `emoji_font_get_count()` / `emoji_font_get_by_index()` for enumeration
**Sources:** [`src/fonts/emoji_font.h`](../src/fonts/emoji_font.h), [`src/fonts/emoji_font_setup.cpp`](../src/fonts/emoji_font_setup.cpp), [`src/fonts/emoji_data.h`](../src/fonts/emoji_data.h)

### NVS Preferences
- **NodePrefs struct** — persisted in ESP32 NVS: node name, frequency, bandwidth, SF, CR, TX power, keyboard backlight, message cap, configured flag
- **load/save/exists** API — `prefs_load()`, `prefs_save()`, `prefs_exists()`
- **Safe defaults** — radio defaults to unconfigured (0 values) so device won't transmit until explicitly set
- **Global accessor** — `prefs_get()` / `prefs_set()` for runtime read/write
**Sources:** [`src/hal/prefs.cpp`](../src/hal/prefs.cpp), [`src/hal/prefs.h`](../src/hal/prefs.h)

### Diagnostics & Debug
- **Compile-time flag** — `SLOPOS_DEBUG=1` build enables the full debug subsystem
- **Runtime levels** — 1 (quiet), 2 (normal, periodic 5s stats), 3 (verbose)
- **Per-feature toggles** — independent runtime on/off for display, mesh, UI, map, diagnostics
- **Dump functions** — `dump_system()`, `dump_lvgl_rendering()`, `dump_trackball_state()`, `dump_home_screen_layout()`, `dump_memory()`, `dump_display_config()`, `dump_mesh_state()`
- **Test controller integration** — remote test mode can change debug level and feature masks at runtime
**Sources:** [`src/diagnostics/debug.cpp`](../src/diagnostics/debug.cpp), [`src/diagnostics/debug.h`](../src/diagnostics/debug.h), [`src/diagnostics/debug_cfg.h`](../src/diagnostics/debug_cfg.h)

### Packet Log
- **Circular packet buffer** — logs every received radio frame with timestamp, source, RSSI, SNR, and payload type string
- **Type classification** — ADVERT_RX, DM_RX, GRP_RX, ANON_RX, ACK, TRACE, PKT_RX
- **Query API** — `getPacketLogCount()`, `getPacketLogEntry()` for UI consumption
**Sources:** [`src/mesh/mesh_wrapper.cpp`](../src/mesh/mesh_wrapper.cpp), [`src/mesh/mesh_wrapper.h`](../src/mesh/mesh_wrapper.h) (lines 27–33, 77–79)

### RTC & System Time
- **MeshCore RTC clock** — network-synchronised time for message timestamps and contact `last_seen`
- **Local date/time** — `getCurrentLocalDateTime()` returns year/month/day/hour/minute
- **Epoch helpers** — `makeEpoch()` / `setSystemTime()` for GPS-based or manual time setting
**Sources:** [`src/mesh/mesh_wrapper.h`](../src/mesh/mesh_wrapper.h) (lines 70–74), [`src/mesh/mesh_wrapper.cpp`](../src/mesh/mesh_wrapper.cpp)

### SPIFFS Persistence
- **State storage** — SPIFFS filesystem for persisting identity keys, contact list, channel config, and message history
- **Graceful fallback** — if SPIFFS mount fails at boot, device continues without persistence (warning logged)
- **Chat persistence** — `chat_save_messages()` / `chat_load_messages()` per-channel history
**Sources:** [`src/main.cpp`](../src/main.cpp) (line 42), [`src/mesh/mesh_wrapper.cpp`](../src/mesh/mesh_wrapper.cpp), [`src/ui/chat_screen.h`](../src/ui/chat_screen.h) (lines 48–49)

### Web Flasher Support
- **Pre-built binaries** in `webflasher/` — bootloader, partitions, boot_app0, firmware, and merged full image
- **Manifest JSON** — versioned metadata for `flasher.meshcore.io` custom firmware installer
- **4-partition flash layout** — bootloader (0x0000), partitions (0x8000), boot_app0 (0xe000), firmware (0x10000)
**Sources:** [`webflasher/manifest.json`](../webflasher/manifest.json), [`firmware/README.md`](../firmware/README.md), [`webflasher/`](../webflasher/)

### Remote Test Controller
- **`SLOPOS_REMOTE_TEST` build mode** — disables LoRa radio, enables simulated input
- **Inject capabilities** — `keyboard_inject()`, `trackball_inject()`, `test_set_touch()`, `injectMessage()`
- **Remote test loop** — runs alongside normal display/UI loop for automated QA
**Sources:** [`src/test/test_controller.h`](../src/test/test_controller.h), [`src/main.cpp`](../src/main.cpp) (lines 67–73, 127)

---

## Hardware Features

### ST7789 Display
- **Driver:** LovyanGFX on shared SPI bus (SPI2_HOST, CS=12, DC=11, BL=42)
- **Resolution:** 320×240 (landscape via rotation 1)
- **Color:** 16-bit RGB565, LVGL partial renderer
- **Backlight:** PWM via GPIO 42, auto-off timeout, programmable brightness
- **Framebuffer capture** for debugging and screenshots (PSRAM full-buffer mode)
- **Coexistence** with LoRa + SD on same SPI bus (different CS lines)
**Sources:** [`src/hal/display.cpp`](../src/hal/display.cpp), [`src/hal/display.h`](../src/hal/display.h), [`src/hal/tdeck_pins.h`](../src/hal/tdeck_pins.h)

### GT911 Touch
- **Interface:** I2C at 0x5D (SDA=18, SCL=8, INT=16), 400 kHz
- **Coordinate transform:** SWAP_XY=true, MIRROR_X=false, MIRROR_Y=true (matches rotation 1)
- **Polling:** `slopos_touch_loop()` called each display frame
- **Multitouch:** Supports multi-point read from GT911 register map
- **Press→release lifecycle** with proper touch-down/touch-up detection
**Sources:** [`src/hal/touch.cpp`](../src/hal/touch.cpp), [`src/hal/touch.h`](../src/hal/touch.h), [`test/test_touch/`](../test/test_touch/)

### I2C Keyboard
- **MCU:** Separate ESP32-C3 slave on I2C address 0x55, 100 kHz
- **Mode:** Returns ASCII characters for each keypress
- **Backlight:** I2C commands 0x01/0x02 for brightness control (0–255)
- **Modifiers:** `is_shift()`, `is_ctrl()`, `is_alt()` — tracked from key codes
- **Debouncing:** MCU handles matrix scanning and debounce internally
- **Inject API:** `keyboard_inject()` for simulated input in test mode
**Sources:** [`src/hal/keyboard.cpp`](../src/hal/keyboard.cpp), [`src/hal/keyboard.h`](../src/hal/keyboard.h), [`test/test_keyboard/`](../test/test_keyboard/)

### 5-Direction Trackball
- **GPIO:** UP=3, DOWN=15, LEFT=1, RIGHT=2, CLICK=0 (BOOT button)
- **Debounce:** Per-direction configurable deadtime (150ms default), falling-edge only
- **Event queue:** `trackball_next_event()` returns queued `SlopOSTrackballEvent` (None, Up, Down, Left, Right, Click)
- **Idle calibration** — reads and stores idle level at init
- **Resettable** — `trackball_reset_scan_state()` for test and wake recovery
- **Inject API** — `trackball_inject()` for remote testing
- **Home screen navigation** — wraps within the 4×3 grid, wrapping at edges
- **Universal back-swipe** — two-Left-commits pattern for go_back()
**Sources:** [`src/hal/trackball.cpp`](../src/hal/trackball.cpp), [`src/hal/trackball.h`](../src/hal/trackball.h), [`src/ui/navigation.h`](../src/ui/navigation.h), [`test/test_trackball/`](../test/test_trackball/)

### Battery ADC
- **Pin:** GPIO 4 (voltage divider)
- **Math:** ADC_MULTIPLIER = 2 × 3.3 × 1000, 12-bit resolution, 8-sample averaging
- **Outputs:** `battery_mv()` in millivolts, `battery_pct()` clamped 0–100%
- **Ranges:** BAT_MIN_MV = 3000 (0%), BAT_MAX_MV = 4200 (100%)
- **Charging detection:** `battery_charging()` status
- **Auto-shutdown:** `TDeckBoard::isBatteryCritical()` triggers deep sleep at < 3200 mV
- **UI:** Battery % shown in bottom bar, turns red when ≤ 20%
**Sources:** [`src/hal/battery.cpp`](../src/hal/battery.cpp), [`src/hal/battery.h`](../src/hal/battery.h), [`src/hal/tdeck_board.h`](../src/hal/tdeck_board.h), [`test/test_battery/`](../test/test_battery/)

### GPS NMEA Parser
- **Interface:** Serial1 (RX=43, TX=44) at 38400 baud
- **Parsing:** NMEA sentence parser with `$GPGGA`, `$GPRMC`, `$GPGLL` support
- **Checksum:** `nmea_checksum_valid()` validates `*XX` XOR checksum; corrupt sentences silently dropped
- **Data:** Latitude, longitude, altitude (m), speed (kn), heading, satellite count, fix quality (0=none, 1=GPS, 2=DGPS, 4=RTK)
- **Time:** UTC hour/minute/second from GPS
- **Fix state:** `gps_has_fix()` predicate
**Sources:** [`src/hal/gps.cpp`](../src/hal/gps.cpp), [`src/hal/gps.h`](../src/hal/gps.h), [`test/test_gps/`](../test/test_gps/)

### SD Card Storage
- **Interface:** Shared SPI bus (CS=39, SCK=40, MISO=38, MOSI=41) with FATFS VFS at `/sdcard`
- **Bus sharing:** Coexists with LoRa and display on SPI2_HOST (different CS lines)
- **Mount check:** `sdcard_mounted()` for UI gating
- **Capacity:** `capacity_bytes()`, `free_bytes()` with `format_size()` helper
- **File I/O:** `sdcard_read()`, `sdcard_write()`, `sdcard_exists()` for map tile loading
- **Init order:** Initialised after radio init so SPI bus is configured
**Sources:** [`src/hal/sdcard.cpp`](../src/hal/sdcard.cpp), [`src/hal/sdcard.h`](../src/hal/sdcard.h), [`test/test_sdcard/`](../test/test_sdcard/)

### LoRa SX1262 Radio
- **Interface:** SPI (NSS=9, SCK=40, MISO=38, MOSI=41, DIO1=45, RST=17, BUSY=13)
- **Defaults:** 869.618 MHz, BW 62.5 kHz, SF 8, CR 4/5, TX power 22 dBm
- **Driver:** RadioLib-based wrappers via MeshCore
- **Wake-on-radio:** DIO1 wake from deep sleep for RX packet reception
- **Configurable:** Frequency, bandwidth, spreading factor, coding rate, TX power via Radio Setup screen
**Sources:** [`src/hal/tdeck_pins.h`](../src/hal/tdeck_pins.h), [`src/mesh/mesh_wrapper.cpp`](../src/mesh/mesh_wrapper.cpp), [`lib/meshcore/`](../lib/meshcore/)

### Buzzer
- **Pin:** GPIO 46 (active low)
- Pin is defined in the HAL pinout but no active software driver module exists yet.
**Sources:** [`src/hal/tdeck_pins.h`](../src/hal/tdeck_pins.h) (line 107)

### Peripheral Power
- **Control:** GPIO 10 — `PIN_PERIPH_PWR`
- **Default:** Set HIGH in `TDeckBoard::begin()` to enable all peripherals
- **Sleep:** Set LOW before deep sleep to conserve battery; GPIO hold re-enabled on wake
**Sources:** [`src/hal/tdeck_board.h`](../src/hal/tdeck_board.h) (lines 51–52, 120)

---

## Offline Map Renderer

A dedicated app-level feature bridging the display, SD card, and GPS systems.

- **Tile source:** PNG format map tiles from SD card (`/sdcard/map/tiles/`)
- **Coordinate system:** Slippy-map tile math (lat/lon → tile X/Y at zoom levels)
- **Rendering:** LVGL canvas grid overlaid with decoded tile pixels
- **Cache:** PSRAM-backed LRU tile cache (4 entries @ 256×256 RGB565 ≈ 524 KB)
- **Cache internals:** `tile_cache_init()`, `tile_cache_lookup()`, `tile_cache_evict_slot()` — 64-bit monotonic clock, safe for 584M years
- **Interaction:** Pan by pixel delta, zoom in/out by one level
- **Position overlay:** Renders own GPS position as a marker on the map
**Full documentation:** [`docs/MAP_SCREEN.md`](MAP_SCREEN.md)
**Sources:** [`src/app/map_renderer.cpp`](../src/app/map_renderer.cpp), [`src/app/map_renderer.h`](../src/app/map_renderer.h), [`src/app/tile_cache.h`](../src/app/tile_cache.h), [`src/app/lodepng_alloc.cpp`](../src/app/lodepng_alloc.cpp), [`test/test_map/`](../test/test_map/)

---

## Test Suite

While not a user-facing feature, the comprehensive test suite (171+ tests across 13 modules) validates every subsystem:

| Module | Tests | What's Covered |
|--------|-------|----------------|
| `test_touch` | 22 | GT911 coordinate mapping, multitouch, press→release lifecycle |
| `test_keyboard` | 20 | Matrix scan, keymap, debounce, ghost detection, LVGL mapping |
| `test_battery` | 16 | mV→%, clamping, monotonicity, ADC math, edge cases |
| `test_sdcard` | 15 | SPI init, mount, read/write, directory listing, edge cases |
| `test_mesh_messaging` | 15 | Message queue, send/receive, channel ops, contact export |
| `test_map` | 14 | Tile math (lat/lon→tile), zoom levels, bounding box |
| `test_mesh_wrapper` | 13 | API signatures, return ranges, unread count init |
| `test_navigation` | 12 | Forward/back, history stack, deep nav chains, all pairs |
| `test_gps` | 12 | NMEA parsing, coordinate conversion, fix detection |
| `test_trackball` | 9 | Direction debounce, deadtime, click detection, idle calibration |
| `test_pins` | 9 | GPIO ranges, SPI/I2C bus conflicts, duplication, LoRa params |
| `test_theme` | 7 | Color darkness, vibrancy, distinctness, readability hierarchy |
| `test_build` | 7 | All headers compile together, cross-module API consistency |

See [`test/README.md`](../test/README.md) for full documentation.

---

## Related Documents

| Document | Description |
|----------|-------------|
| [`README.md`](../README.md) | Project overview, quick start, hardware table |
| [`AGENTS.md`](../AGENTS.md) | Full architecture guide, conventions, pitfalls (agent context) |
| [`CONTRIBUTING.md`](../CONTRIBUTING.md) | Contribution workflow, PR checklist, coding standards |
| [`docs/KNOWN_ISSUES.md`](KNOWN_ISSUES.md) | Tracked bugs, fixes, and workarounds |
| [`docs/MAP_SCREEN.md`](MAP_SCREEN.md) | Map screen and tile cache system documentation |
| [`docs/MISSING_FEATURES.md`](MISSING_FEATURES.md) | MeshCore protocol features not yet implemented, with effort estimates |
| [`test/README.md`](../test/README.md) | Test suite structure, mock guidelines, running tests |
| [`firmware/README.md`](../firmware/README.md) | Flash instructions, binary layout, web flasher |
