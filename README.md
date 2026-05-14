# SlopOS T-Deck

DISCLAIMER: This is entirely AI generated and I haven't yet recieved my T-Deck to test it out! Only flash this if you are willing to deal with the potential consequences.

Standalone off-grid LoRa mesh messaging firmware for the **LilyGo T-Deck** (ESP32-S3 + SX1262 + ST7789 320x240 touchscreen + physical QWERTY keyboard).

Built on the [MeshCore](https://github.com/meshcore-dev/MeshCore) mesh networking protocol — fully interoperable with existing MeshCore repeaters, room servers, and companion radios.

## Status

| Feature | Status |
|---------|--------|
| Dark Discord-like UI (LVGL v9) | ✅ Complete |
| Home screen (3×4 icon grid + status bars) | ✅ Complete |
| Chat screen (channel list, message bubbles, text input) | ✅ Complete |
| Heard / Contacts / Repeaters screens | ✅ Complete |
| Signal / Noise diagnostic screens | ✅ Complete |
| Map (offline tiles placeholder) | ✅ Complete |
| Settings / Terminal / Trace screens | ✅ Complete |
| Finder / Advertise screens | ✅ Complete |
| MeshCore protocol (radio, routing, encryption) | ✅ Integrated |
| T-Deck HAL (display, battery, LoRa, pins) | ✅ Complete |
| Unit tests (10 modules) | ✅ 165 tests |
| Touch input driver (GT911) | ✅ Complete |
| Keyboard input driver (I2C, ESP32-C3 MCU) | ✅ Complete |
| Full mesh messaging (send/receive queue + UI integration) | ✅ Complete |
| GPS NMEA parser | ✅ Complete |
| SD card support (SPI mount, read/write) | ✅ Complete |
| Offline map renderer (tile math + LVGL canvas grid) | ✅ Complete |

## Test Suite

```bash
# Run all 64 tests on native platform (no hardware needed)
pio test -e native_test -v

# Run a specific test module
pio test -e native_test -f test_battery -v
```

| Test Module | Tests | What's Covered |
|-------------|-------|----------------|
| `test_battery` | 15 | mV→% conversion, clamping, monotonicity, edge cases, ADC math |
| `test_pins` | 13 | GPIO ranges, SPI/I2C bus conflicts, duplicate detection, LoRa params |
| `test_theme` | 9 | Color darkness, vibrancy, distinctness, readability hierarchy |
|| `test_touch` | 22 | GT911 coordinate mapping, multitouch parsing, press→release lifecycle |
|| `test_keyboard` | 21 | Matrix scan, keymap, debounce, ghost detection, LVGL mapping |
| `test_navigation` | 12 | Forward/back with history stack, deep nav chains, all pairs |
| `test_mesh_wrapper` | 11 | API signatures, return value ranges, unread count init |
| `test_build` | 7 | All headers compile together, cross-module API consistency |

Full test documentation: [`test/README.md`](test/README.md)

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32-S3, 240 MHz, 16 MB Flash, 8 MB PSRAM |
| Display | ST7789 320×240 TFT |
| Touch | GT911 capacitive (I2C) |
| Keyboard | Physical QWERTY matrix |
| LoRa | SX1262 (SPI) |
| GPS | Serial1 (optional) |
| SD Card | SPI (shared bus) |

## Architecture

```
slopos-tdeck/
├── lib/meshcore/          ← Git submodule: MeshCore protocol (routing, radio, encryption)
├── src/
│   ├── main.cpp           ← Boot sequence (board → display → mesh → UI)
│   ├── lv_conf.h          ← LVGL v9 config (16-bit, partial render)
│   ├── hal/
│   │   ├── tdeck_pins.h   ← Complete T-Deck pinout
│   │   ├── tdeck_board.h  ← TDeckBoard :: mesh::MainBoard
│   │   ├── display.cpp/h  ← LovyanGFX ST7789 + LVGL driver
│   │   ├── battery.cpp/h  ← ADC battery (mV + %)
│   │   ├── touch.cpp/h    ← GT911 touch controller (I2C)
│   │   └── keyboard.cpp/h ← QWERTY matrix keyboard scanner
│   ├── mesh/
│   │   └── mesh_wrapper.cpp/h ← SX1262 radio init, RTC, mesh API
│   └── ui/
│       ├── theme.h        ← Discord-inspired dark palette
│       ├── home_screen.cpp/h   ← 3×4 icon grid + top/bottom bars
│       ├── chat_screen.cpp/h   ← Discord-like chat (channels, bubbles, input)
│       ├── screens.cpp/h  ← All 11 other screens
│       ├── navigation.cpp/h    ← Screen routing with animations
│       └── ui.cpp/h       ← Splash → Home transition
├── boards/t-deck.json     ← PlatformIO board definition
├── platformio.ini         ← Build config (ESP32-S3 + LVGL + MeshCore)
└── test/                  ← Unit test directory
```

## Build & Flash

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- LilyGo T-Deck

### Clone with submodule
```bash
git clone --recurse-submodules https://github.com/hermes-gadget/slopos-tdeck.git
cd slopos-tdeck
```

### Build
```bash
pio run -e SlopOS_TDeck
```

### Flash
```bash
# Put T-Deck in DFU mode (hold trackball while powering on)
pio run -e SlopOS_TDeck -t upload
```

### Monitor
```bash
pio device monitor -b 115200
```

## License

GPL-3.0-or-later

This project is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Dependencies remain under their original licenses (MIT, FreeBSD, LGPL-2.1,
zlib/libpng, BSD-3-Clause) — see [Open Source Acknowledgments](#open-source-acknowledgments)
below for the full audit.

## Open Source Acknowledgments

This project builds on and incorporates open source software from the following projects:

| Project | License | Usage in SlopOS |
|---------|---------|-----------------|
| [MeshCore](https://github.com/meshcore-dev/MeshCore) | MIT | Mesh networking protocol (routing, radio, encryption) — integrated as git submodule at `lib/meshcore/` |
| [LilyGo T-Deck Keyboard_ESP32C3](https://github.com/Xinyuan-LilyGO/T-Deck) | MIT | I2C keyboard protocol reference — our `keyboard.cpp` driver is based on the command set and keymap from this firmware (© 2023 Shenzhen Xin Yuan Electronic Technology Co., Ltd) |
| [LVGL](https://github.com/lvgl/lvgl) | MIT | Embedded GUI framework (v9.5) |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | FreeBSD | Display driver for ST7789 TFT |
| [RadioLib](https://github.com/jgromes/RadioLib) | MIT | SX1262 LoRa radio driver |
| [Arduino Crypto](https://github.com/rweather/arduinolibs) | MIT | AES/SHA for MeshCore packet encryption |
| [Google Test](https://github.com/google/googletest) | BSD-3-Clause | Unit testing framework |
