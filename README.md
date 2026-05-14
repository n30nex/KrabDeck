# SlopOS T-Deck

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
| Unit tests | 🔲 TODO |
| Touch input driver (GT911) | 🔲 TODO |
| Keyboard input driver (matrix) | 🔲 TODO |
| GPS support | 🔲 TODO |
| SD card support | 🔲 TODO |
| Full mesh messaging (send/receive) | 🔲 TODO |
| Offline map rendering | 🔲 TODO |

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
│   │   └── battery.cpp/h  ← ADC battery (mV + %)
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

MIT
