# Known Issues

This document tracks known issues, bugs, and missing features in SlopOS. Contributions welcome — pick something from the list and open a PR.

---

## Finder

### Zero-hop ping for nearby discovery
The finder feature needs a proper implementation that sends a zero-hop (TTL=1) ping to discover nearby repeaters. Currently there's no way to probe what's in immediate radio range without relying on periodic adverts.

**What's needed:**
- A "Ping Nearby" action that sends a broadcast with hop limit = 1
- A response handler that collects replies over a short window (2-3 seconds)
- Display results grouped by RSSI (strongest first)
- Cooldown of 30 seconds between pings to avoid flooding

---

## Launcher Compatibility

### SlopOS doesn't work under bmorcelli/Launcher
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
The main loop calls `slopos::mesh::loop()` (which may do blocking LoRa TX/RX taking 100-500ms) before `lv_timer_handler()`. During that period, LVGL is not serviced, causing visible UI stuttering — animations freeze, button feedback lags, and scrolling jerks.

**What's needed:** Defer long mesh operations to a separate task or state machine, interleave `lv_timer_handler()` calls during mesh loop iterations, or use FreeRTOS task priorities to keep LVGL responsive.

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
All trackball directions fire on falling edge only (one event per physical detent). LEFT fires on BOTH rising and falling edges, producing 2 events per detent. The 80ms deadtime (vs 150ms for others) doesn't prevent the double-fire — it only limits the minimum inter-event gap. In UI navigation, moving LEFT advances 2 items while other directions advance 1.

**What's needed:** Change LEFT to fire on falling edge only (same as other directions), or implement edge-agnostic debounce that fires exactly once per detent.

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
**FIXED in PR #108:** Moved SD card from SPI3_HOST (HSPI) to SPI2_HOST (FSPI). The display (LovyanGFX via ESP-IDF) retains exclusive use of SPI3_HOST. SD now shares SPI2_HOST with the LoRa radio (both via Arduino SPIClass), which has proper mutual exclusion via the SPI transaction mechanism. Also updated the incorrect comment in `sdcard.cpp`.
