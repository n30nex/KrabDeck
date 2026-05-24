# SlopOS T-Deck Firmware Binaries

Pre-built firmware for the LilyGo T-Deck (ESP32-S3, 16 MB flash).

| File | Use |
|------|-----|
| `slopos-tdeck-merged.bin` | Full flash — flash at 0x0 (first install) |
| `slopos-tdeck.bin` | App update only — flash at 0x10000 (preserves bootloader/partitions) |

## Flash with esptool (no PlatformIO needed)

```bash
# Install esptool if you don't have it
pip install esptool

# Full flash (first install)
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode qio --flash_freq 80m --flash_size 16MB \
  0x0 slopos-tdeck-merged.bin

# App update only (keep settings)
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
  write_flash 0x10000 slopos-tdeck.bin
```

## Flash with PlatformIO

```bash
pio run -e SlopOS_TDeck -t upload
```

## Flash with web flasher (flasher.meshcore.io)

Use the **Custom Firmware** option and upload `slopos-tdeck-merged.bin`.  
The flasher will flash the merged binary at offset 0x0.

## What's in the merged binary

| Offset | Component |
|--------|-----------|
| 0x0000 | Bootloader |
| 0x8000 | Partition table |
| 0xe000 | Boot app0 |
| 0x10000 | SlopOS firmware (`beta-0.1.30`) |

Flash the merged binary at offset 0x0 — it contains everything needed to boot.
