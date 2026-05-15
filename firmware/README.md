# SlopOS T-Deck Firmware Binaries

Pre-built firmware for the LilyGo T-Deck (ESP32-S3, 16 MB flash, QIO mode).

## Flash with esptool (no PlatformIO needed)

```bash
# Install esptool if you don't have it
pip install esptool

# Flash (replace COM21 with your port)
esptool.py --chip esp32s3 --port COM21 --baud 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode qio --flash_freq 80m --flash_size 16MB \
  0x0 slopos-tdeck-merged.bin
```

## Flash with PlatformIO

```bash
pio run -e SlopOS_TDeck -t upload
```

## What's in the merged binary

| Offset | Component |
|--------|-----------|
| 0x0000 | Bootloader |
| 0x8000 | Partition table |
| 0xe000 | Boot app0 |
| 0x10000 | SlopOS firmware |

Flash the merged binary at offset 0x0 — it contains everything needed to boot.
