# Known Issues

This document tracks known issues, bugs, and missing features in SlopOS. Contributions welcome — pick something from the list and open a PR.

---

## Finder

### Zero-hop ping for nearby discovery
**IMPLEMENTED in PR #112:** Added a "Ping Nearby" feature that sends a zero-hop (TTL=1) CONTROL broadcast to discover nearby nodes:

- **Send**: `sendPingNearby()` creates a `PAYLOAD_TYPE_CONTROL` packet with `"PING:<tag>"` payload and sends via `sendZeroHop()`
- **Response**: Receiving nodes auto-respond with `"PONG:<tag>:<name>:<rssi>"` via `sendZeroHop()`
- **Collection**: Pinger collects PONG responses tagged with its unique ping tag over a 3-second window
- **UI**: Finder screen now has a "Ping Nearby" button; shows responders with RSSI after collection window ends
- **Cooldown**: 30-second cooldown between pings
- **Protocol**: Uses standard MeshCore `PAYLOAD_TYPE_CONTROL` packets, interoperable with any MeshCore node that implements `onControlDataRecv`

---

## Launcher Compatibility

### SlopOS doesn't work under bmorcelli/Launcher
**DEFERRED — needs further investigation into bmorcelli/Launcher's init sequence and partition layout before scoping the work.**

[Launcher](https://github.com/bmorcelli/Launcher) is an ESP32 app launcher with explicit T-Deck support (display, touch, keyboard, SD card). A user tried running SlopOS as a Launcher-launched app and ran into problems — the keyboard doesn't work properly, and many other things break.

**Root cause:** SlopOS is built as standalone firmware that expects full hardware control at boot. Launcher initialises the display, keyboard, I2C, SPI, and LoRa pins before handing off, which leaves GPIOs, peripheral registers, and I2C bus state in an incompatible state when SlopOS starts.

**What's needed — a `launcher-compatible` build target or a compatibility layer:**
- Ensure all peripheral init (keyboard I2C, display, LoRa SPI, GPIOs, PSRAM) is safe to re-init even if already configured by a bootloader or launcher
- Partition table must coexist with Launcher's OTA partition scheme — likely needs a unified partition layout
- Display handoff: ST7789 registers may be in an unknown state — LovyanGFX handles this on init but needs testing
- LoRa radio: Launcher disables LoRa CS for SD card access — SlopOS needs to fully reset and re-init the SX1262 on boot
- Keyboard: the I2C keyboard MCU (0x55) may already be claimed or in key mode — must force re-init via Wire + backlight commands
- Trackball GPIOs may have internal pull states changed by Launcher — need explicit `pinMode` re-init
- LoRa SPI bus (shared with display) must be safely re-initialised without conflicting with any prior configuration

---

## How to Help

Pick any item from the list above and open a PR against the `dev` branch. See [`CONTRIBUTING.md`](./CONTRIBUTING.md) for the full contribution workflow.

---

## UI Performance

### LVGL tick starvation during LoRa TX
**FIXED in PR #110:** Two-part fix: (1) reordered main loop so `slopos_display_loop()` (which calls `lv_timer_handler()`) runs before `slopos::mesh::loop()`. (2) Added periodic `lv_timer_handler()` calls inside `mesh_wrapper.cpp::loop()` with a 20ms guard (~50 Hz), ensuring UI stays responsive even during sustained mesh activity. Uses an `extern "C"` declaration to avoid pulling LVGL headers into the mesh layer.

---

## Mesh Networking

### No contact expiry / eviction
**FIXED in PR #109:** Replaced the silent `return` in `onAdvertRecv` with LRU eviction. When the contact list is full (`SLOP_MAX_CONTACTS=64`) and a new contact arrives, the contact with the oldest `last_seen` timestamp is evicted and the new contact takes its slot. Also resets `out_path_len` on evicted slots. Added 9 unit tests: fill-to-max, LRU eviction, dedup after eviction, re-add after eviction, bulk eviction preservation, and multi-continent integrity.

---

## Map Screen

### LRU cache clock uint32_t wrap
**FIXED in PR #107:** Changed `cache_clock` and `last_used` from `uint32_t` to `uint64_t` — safe for 584 million years of continuous use at 1kHz. Also extracted the tile cache into a reusable module (`tile_cache.h`/`.cpp`) with 11 unit tests covering init, lookup, eviction ordering, and the memory-free contract.

---

## Touch / Input

### Trackball LEFT fires on both edges
**FIXED in PR #111:** Removed the LEFT-specific exception in `scan_direction` — LEFT now fires on falling edge only, matching UP/DOWN/RIGHT. Aligned `LEFT_DEADTIME_MS` (80 → 150) with `DIRECTION_DEADTIME_MS` since the shorter deadtime was only needed to partially mitigate the double-fire. Updated the `DirectionIdleLevelIsCalibratedAtInit` test to verify falling-edge-only behavior.

---

## Screen Navigation

### Navigation history stack is broken
**FIXED in PR #106:** Replaced the circular buffer with a linear stack. Back-navigation now works correctly regardless of navigation depth — dropping the oldest entry by shifting when full instead of wrapping.

---

## Chat Screen

### REPEATERS tile navigates to Packets screen instead of a nodes/repeaters view
**FIXED in PR #99:** REPEATERS now redirects to `Screen::Network` (same as FINDER), which surfaces nearby nodes by signal strength.

**Remaining:** A dedicated repeaters/nodes screen with RSSI-sorted list would be better than borrowing FINDER's screen, but the duplicate-icon bug is fixed.

---

## Onboarding

### ESP.restart() without flash write completion
**FIXED in PR #102:** Added `delay(100)` between save calls and `ESP.restart()` in the onboarding screen Done button handler to allow SPIFFS writes to flush.

---

## GPS

### No NMEA checksum validation
**FIXED in PR #101:** Added `nmea_checksum_valid()` — validates the `*XX` XOR checksum on NMEA sentences before parsing. Corrupted sentences are silently discarded. Sentences without checksums are still accepted.

---

## Terminal

### Unbounded label accumulation
**FIXED in PR #104:** Added `MAX_TERM_LINES=64` with oldest-line pruning in `term_add_line()`. Labels no longer accumulate indefinitely.

---

## SPI / Display

### Display and SD card share the same SPI host
**FIXED in PR #108 + follow-up:** The original fix (PR #108) moved the SD card from SPI3_HOST to SPI2_HOST, but this created a GPIO matrix conflict — the LoRa mesh init called `lora_spi.begin()` on SPI2_HOST, which remapped pins 40/41/38 away from SPI3_HOST, silently disconnecting the display.

**Real fix:** Moved the display from SPI3_HOST to SPI2_HOST in `display.cpp`, so all three bus-sharing devices (display, LoRa, SD) use the same SPI2_HOST with different CS lines. No cross-host pin contention.
