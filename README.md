# SlopOS T-Deck

Standalone off-grid LoRa mesh messaging firmware for the **LilyGo T-Deck** (ESP32-S3 + SX1262 + ST7789 320x240 touchscreen + physical QWERTY keyboard).

## Features (in development)

- [ ] Dark, Discord-like LVGL GUI — 3×4 icon grid home screen, top/bottom status bars
- [ ] Multi-hop LoRa mesh networking (SX1262)
- [ ] AES-128 end-to-end encryption
- [ ] Direct messaging + public/group channels
- [ ] Offline maps (bundled tiles, zoom/pan)
- [ ] Repeater and Room Server modes
- [ ] Terminal/debug access
- [ ] SD card support
- [ ] Frequency presets (EU 868, US 915, etc.)

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

## Build & Flash

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- LilyGo T-Deck

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

## Project Structure

```
src/
├── main.cpp          # Entry point
├── lv_conf.h         # LVGL configuration
├── hal/              # Hardware abstraction layer
│   ├── tdeck_pins.h  # Pin definitions
│   └── display.cpp   # LVGL + LovyanGFX display driver
├── ui/               # LVGL UI layer (theme, screens)
├── mesh/             # Mesh networking protocol
├── app/              # Application logic (chat, channels, repeater)
└── utils/            # Storage, power, helpers
```

## License

MIT
