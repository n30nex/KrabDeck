# Known Issues

This document tracks known issues, bugs, and missing features in SlopOS. Contributions welcome — pick something from the list and open a PR.

---

## Boot / Board Support

### Duplicate TDeckBoard instances — radio driver board never initialised (`mesh_wrapper.cpp:33`)
Two `static slopos::TDeckBoard board;` instances exist: one in `main.cpp:24` and one in `mesh_wrapper.cpp:33`. Only the `main.cpp` instance has `begin()` called on it. The mesh/radio driver (`CustomSX1262Wrapper`) uses the `mesh_wrapper.cpp` instance (passed at construction via `radio_module, board`). When `Dispatcher::begin()` calls `_radio->begin()`, the radio wrapper calls `_board->getStartupReason()` — which reads the `mesh_wrapper` board, not the `main.cpp` one.

The `startup_reason` defaults to `BD_STARTUP_NORMAL` in the constructor, so this doesn't crash, but the DIO1 wake-from-deep-sleep detection is never triggered on the radio driver's board. A LoRa packet received during deep sleep that wakes the device is silently dropped — `setFlag()` is never called because `getStartupReason()` never returns `BD_STARTUP_RX_PACKET`.

**What's needed:** Either remove the `mesh_wrapper.cpp` board instance and pass the `main.cpp` board reference into `slopos::mesh::init()`, or call `board.begin()` on the wrapper instance too before the mesh init path.

### `new Module(...)` at static init time with no null-check (`mesh_wrapper.cpp:35`)
The RadioLib `Module` object is heap-allocated with `new` at file scope (static init order), before `main()` runs and before PSRAM is available. ESP32 Arduino builds don't use exceptions — a failed `new` returns `nullptr`. The returned pointer is used unconditionally by all downstream RadioLib calls. DRAM fragmentation at static-init time is possible on ESP32-S3.

**What's needed:** Move the `Module` allocation into `mesh::init()` after PSRAM init. Add a null-check and halt with serial diagnostic on failure.

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
**FIXED in PR #110:** Two-part fix: (1) reordered main loop so `slopos_display_loop()` (which calls `lv_timer_handler()`) runs before `slopos::mesh::loop()`. (2) Added periodic `lv_timer_handler()` calls inside `mesh_wrapper.cpp::loop()` with a 20ms guard (~50 Hz), ensuring UI stays responsive even during sustained mesh activity. Uses an `extern "C"` declaration to avoid pulling LVGL headers into the mesh layer.

### `lv_obj_del()` called synchronously inside LV_EVENT_CLICKED handler (`screens.cpp:1452`)
The Trace result screen deletes `trace_result_label` with `lv_obj_del()` directly inside an event callback. LVGL's event dispatch loop may still hold references to the object after the callback returns. Other handlers in the same file correctly use `lv_obj_del_async()`, making this an isolated omission. This can cause use-after-free crashes in the LVGL event system.

**What's needed:** Replace with `lv_obj_del_async(trace_result_label)` and immediately set `trace_result_label = nullptr` after scheduling the async delete.

### Static local variables in map screen lambda persist across re-entries (`screens.cpp:690-691`)
The map screen touch handler uses `static int drag_start_x/y` and `static uint32_t map_last_render_ms`. These values survive navigation away from and back to the map screen. On re-entry, `map_last_render_ms` retains its old value, which can cause the first map re-render to be skipped if the elapsed-time guard (`now - map_last_render_ms >= 200`) evaluates as not-yet-expired.

**What's needed:** Reset all three statics to sentinel values (0 for coordinates, 0 for timestamp) in an `LV_EVENT_SCREEN_LOADED` handler, or convert them to non-static function-local variables that are re-initialised on every entry.

---

## Mesh Networking

### Channel hash lookup only checks first byte
The `searchChannelsByHash` function in `slop_mesh.h` compares only the first byte of the 32-byte channel hash to find a matching channel. With 8 channels and uniformly random hashes, there is an ~11% collision probability. When a collision occurs, an encrypted group message is decrypted with the wrong channel key, producing garbage text and displaying the wrong channel name.

**What's needed:** Replace the single-byte `hash[0] == _channels[i].channel.hash[0]` comparison with a full `memcmp` of the entire hash array.

### No contact expiry / eviction
**FIXED in PR #109:** Replaced the silent `return` in `onAdvertRecv` with LRU eviction. When the contact list is full (`SLOP_MAX_CONTACTS=64`) and a new contact arrives, the contact with the oldest `last_seen` timestamp is evicted and the new contact takes its slot. Also resets `out_path_len` on evicted slots. Added 9 unit tests: fill-to-max, LRU eviction, dedup after eviction, re-add after eviction, bulk eviction preservation, and multi-continent integrity.

### Advert rate limiting at mesh layer only
The 10-second advert cooldown is only enforced in the UI (button disabled state). The `sendAdvert()` function in `mesh_wrapper.cpp` has no rate-limiting of its own — it can be called programmatically (e.g. from the Terminal's `advert` command) without protection, potentially flooding the mesh.

**What's needed:** Add a timestamp check in `sendAdvert()` that rejects calls within `ADVERT_COOLDOWN_SECONDS`.

### Missing null-termination on short payloads
In `slop_mesh.h`, the payload text null-termination is conditional: `if (len > 1) data[len - 1] = '\0'`. For `len == 1`, no null byte is written, so the C string read from `data` may run past the buffer, causing undefined behavior or leaking stack data.

**What's needed:** Always null-terminate: `data[len - 1] = '\0';` unconditionally.

### `sendTrace()` indentation anomaly suggesting merge artifact (`slop_mesh.h:265-271`)
Lines 265–271 inside `sendTrace()` are at column 0 (8-space indent would match the rest of the function) — a pattern that indicates a botched merge or rebase conflict resolution. The code compiles and appears logically correct, but the anomalous indentation suggests a developer may have accidentally removed or misplaced braces during conflict resolution. A future edit that relies on alignment for structure comprehension risks introducing a logic error.

**What's needed:** Fix the indentation of lines 265–271 to match the method body (use 8-space indent). Verify the function compiles and passes trace tests.

---

## Map Screen

### LRU cache clock uint32_t wrap
The tile LRU cache uses a `uint32_t cache_clock` that increments monotonically. After ~4 billion increments (or ~50 days of continuous use at 1kHz), it wraps to 0, breaking cache eviction comparisons — newly cached tiles have lower `last_used` values than old ones, causing premature eviction of recently used tiles.

**What's needed:** Use `uint64_t` for `cache_clock`, or implement a wrapping-aware comparison.

---

## Touch / Input

### I2C bus speed race — touch runs at 100kHz instead of 400kHz
The I2C bus is shared between the GT911 touch controller (400kHz capable) and the keyboard MCU (100kHz). The init sequence sets 400kHz for touch, then overwrites it to 100kHz during keyboard init. After that, ALL subsequent I2C operations (including touch reads) run at 100kHz. Touch is functional but reads at 1/4 speed, increasing touch latency by ~3-4x.

**What's needed:** Restore `Wire.setClock(400000)` after keyboard init completes, or use 100kHz for both (GT911 works at any speed up to 400kHz).

### Trackball LEFT fires on both edges
**FIXED in PR #111:** Removed the LEFT-specific exception in `scan_direction` — LEFT now fires on falling edge only, matching UP/DOWN/RIGHT. Aligned `LEFT_DEADTIME_MS` (80 → 150) with `DIRECTION_DEADTIME_MS` since the shorter deadtime was only needed to partially mitigate the double-fire. Updated the `DirectionIdleLevelIsCalibratedAtInit` test to verify falling-edge-only behavior.

### `slopos_keyboard_has_new_event()` clears its event flag as a side effect (`keyboard.cpp:191`)
`slopos_keyboard_has_new_event()` returns true and simultaneously clears the internal `has_new_event` flag. A second call before `slopos_keyboard_consume_key()` returns false even though the key value has not been read. The `has_*` naming convention universally implies a non-mutating predicate, so this violates caller expectations without documentation.

**What's needed:** Either rename to `slopos_keyboard_consume_event()` to make the side effect explicit, or split into `slopos_keyboard_has_event()` (non-destructive) + `slopos_keyboard_clear_event()` (explicit clear).

### GT911 INT-pin-HIGH release check may drop buffered touch events on rapid taps (`touch.cpp:158`)
The touch polling loop treats an INT pin HIGH as an immediate release and stops reading. The GT911 controller buffers multiple touch scan intervals internally; if the user taps twice rapidly and the INT pin de-asserts between scans, the second tap is never read from the FIFO. This manifests as intermittent missed taps on fast-response buttons (Send in chat, number keys in settings).

**What's needed:** After reading a touch point, re-read the GT911 point count register before reporting release. Only report `LV_INDEV_STATE_RELEASED` when the point count confirms zero active touches, rather than relying solely on the INT pin level.

---

## Screen Navigation

### Navigation history stack is broken
The navigation system uses a circular buffer with MAX_HISTORY=8. `push_history` wraps around and overwrites the oldest entry when full. `pop_history` decrements `history_top` without wrapping — after wrapping occurs, the stack has no way to distinguish new entries from overwritten ones. The stack can only hold 8 items but there are 14+ screen types.

**What's needed:** Replace the circular buffer with a simple linear stack: drop the oldest entry when full instead of wrapping. Fix `pop_history` to handle wrap-around correctly.

---

## Chat Screen

### REPEATERS tile navigates to Packets screen instead of a nodes/repeaters view
**FIXED:** REPEATERS now navigates to `Screen::Network` (the Finder screen) instead of `Screen::Heard`. Both REPEATERS and FINDER open the same network view, which surfaces known contacts sorted by RSSI.

**Remaining:** A dedicated repeaters/nodes screen with RSSI-sorted list would be better than borrowing FINDER's screen, but the duplicate-icon bug is fixed.

### Contact name retrieved by hardcoded child index instead of user data (`screens.cpp:551`)
When the user taps a contact row to open a DM, the contact name label is extracted via `lv_obj_get_child(target, 1)` — a hardcoded positional index. If any future change adds or reorders children in the contact row (e.g. an online indicator icon, signal strength badge), this silently retrieves the wrong widget, resulting in a DM opened with a garbage name or a crash.

**What's needed:** At row-creation time, store the contact name string as `user_data` on the row object via `lv_obj_set_user_data(row, name_ptr)`. In the event handler, retrieve it with `lv_obj_get_user_data(target)`. This eliminates all positional coupling.

---

## Onboarding

### ESP.restart() without flash write completion
The onboarding screen's Done button calls `ESP.restart()` immediately after `chat_save_messages()`. If the SPIFFS write hasn't completed (due to write caching), the save data is lost after reboot.

**What's needed:** Add a small delay (`delay(100)`) between the save and the restart, or set a flag for the main loop to handle the restart.

---

## GPS

### No NMEA checksum validation
Raw GPS NMEA sentences from the L76K module include a `*XX` checksum suffix that is never validated (`gps.cpp`). Corrupted sentences from noisy GPS reception are parsed as valid data, potentially giving incorrect coordinates, altitude, or fix status.

**What's needed:** Implement NMEA checksum validation — extract the checksum from after the `*` in the sentence, compute XOR of all bytes between `$` and `*`, and compare. Discard sentences that don't match.

---

## Terminal

### Unbounded label accumulation
Each command in the Terminal screen creates a new LVGL label widget (`screens.cpp:1221-1227`). There is no upper bound or pruning — after hundreds of commands, thousands of label widgets accumulate in the LVGL object tree, consuming heap. Labels are only freed when the user navigates away.

**What's needed:** Cap the number of visible terminal lines (e.g. 64), deleting the oldest label when the cap is reached. A `lv_obj_clean()` on the log container before adding the new line would also work but is more disruptive to the scroll state.

---

## SPI / Display

### Display and SD card share the same SPI host
**FIXED in PR #108 + follow-up:** The original fix (PR #108) moved the SD card from SPI3_HOST to SPI2_HOST, but this created a GPIO matrix conflict — the LoRa mesh init called `lora_spi.begin()` on SPI2_HOST, which remapped pins 40/41/38 away from SPI3_HOST, silently disconnecting the display.

**Real fix:** Moved the display from SPI3_HOST to SPI2_HOST in `display.cpp`, so all three bus-sharing devices (display, LoRa, SD) use the same SPI2_HOST with different CS lines. No cross-host pin contention.

---

## Code Quality / Maintenance

### Screen dispatch switch duplicated in `navigate_to()` and `go_back()` (`navigation.cpp`)
The 14+-case switch that maps `Screen` enum values to their `*_show()` functions appears identically in both `navigate_to()` and `go_back()`. Any new screen type requires updating both copies. Currently REPEATERS and PACKETS both dispatch to `Screen::Heard` in one block — a fix must be applied in two places.

**What's needed:** Extract the switch into a `static void dispatch_screen(Screen s)` helper function and call it from both `navigate_to()` and `go_back()`.

### `debug.h` declares symbols only implemented under `#if defined(SLOPOS_DEBUG)` (`debug.h`)
`debug.h` declares the `slopos::debug` namespace and all its functions unconditionally, while `debug.cpp` wraps all implementations in `#if defined(SLOPOS_DEBUG)`. In a non-debug build, any call to a debug function compiles but produces an unresolved-symbol linker error. The current build works because debug calls are also guarded at call sites, but this is a fragile implicit assumption.

**What's needed:** Either (a) wrap the declarations in `debug.h` in `#if defined(SLOPOS_DEBUG)` to match the implementation, or (b) provide stub no-op inline implementations in the header for non-debug builds.

### `makeEpoch()` uses non-thread-safe `setenv`/`tzset` for UTC conversion (`mesh_wrapper.cpp`)
`makeEpoch()` temporarily sets the process TZ to `"UTC0"` via `setenv()` + `tzset()`, calls `mktime()`, then restores the original TZ. `setenv` and `tzset` modify global state and are not thread-safe. Any concurrent code path reading localtime (GPS NMEA timestamp parsing, UI clock display, NTP sync) during this window silently receives UTC instead of the device's configured timezone.

**What's needed:** Replace with `timegm()` (available as a GNU extension in ESP32 newlib). `timegm()` interprets `struct tm` as UTC without touching TZ and is reentrant.
