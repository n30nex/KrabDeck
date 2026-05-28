# Known Issues

This document tracks currently open known issues, bugs, and missing features in SlopOS T-Deck firmware. All historically tracked issues that have been resolved are maintained in the Git history — check merged PRs and commit logs for the full record.

---

## Launcher Compatibility

### SlopOS doesn't work under bmorcelli/Launcher

[Launcher](https://github.com/bmorcelli/Launcher) is an ESP32 app launcher with explicit T-Deck support (display, touch, keyboard, SD card). A user tried running SlopOS as a Launcher-launched app and ran into problems — the keyboard doesn't work properly, and many other things break.

**Root cause:** SlopOS is built as standalone firmware that expects full hardware control at boot. Launcher initialises the display, keyboard, I2C, SPI, and LoRa pins before handing off, which leaves GPIOs, peripheral registers, and I2C bus state in an incompatible state when SlopOS starts.

**Status:** Not planned. SlopOS is designed as standalone firmware, not a Launcher app. Fixing this would require deep changes to every HAL driver to detect and handle pre-initialised peripherals.

---

## Boot/System

### Fresh unconfigured boot leaves clock APIs disabled

`src/mesh/mesh_wrapper.cpp:246-248` starts the fallback and RTC clocks, but non-debug builds return early when radio preferences are not configured at `src/mesh/mesh_wrapper.cpp:279-287` — before the wrapper reaches the later initialized state used by time APIs. As a result, `getCurrentTime()` returns `0` unless initialized at `src/mesh/mesh_wrapper.cpp:629-631`, and `setSystemTime()` refuses to set the clock unless initialized at `src/mesh/mesh_wrapper.cpp:633-638`. Settings and onboarding date/time flows call those APIs from `src/ui/screens.cpp:1314-1338` and `src/ui/onboarding_screen.cpp:212-220`, so a fresh device can show fallback dates or fail time setting until radio setup has completed.

**What's needed:** Split clock initialization from radio initialization. The RTC/fallback clock should become usable even when SX1262 is intentionally held in reset. Track separate states such as `clock_initialized` and `radio_initialized`, and update settings/onboarding tests for an unconfigured first boot.

---

## Radio/Mesh

### Ping Nearby control packets cannot match PING or PONG commands

`src/mesh/slop_mesh.h:717-720` sends Ping Nearby packets by setting the high bit on `payload[0]`. Receive-side control handling checks that bit at `src/mesh/slop_mesh.h:386-388`, but then compares the modified payload directly against `"PING:"` and `"PONG:"` at `src/mesh/slop_mesh.h:390` and `src/mesh/slop_mesh.h:413`. After setting the control bit, ASCII `'P'` is no longer `'P'`, so both `memcmp()` checks fail. The Finder screen can send pings, but peers will not recognize them as PING, and returned PONG packets will not be collected.

**What's needed:** Mask off the control bit before command parsing, or reserve a separate control flag byte that is not part of the command string. Add a unit test that sends a Ping Nearby packet through `onRecvPacket()` and verifies both PING response and PONG result collection.

---

### Partial or corrupt saved channels load uninitialized keys

`src/mesh/mesh_wrapper.cpp:754-772` loads saved channel names, secrets, and hashes from NVS. The local `name`, `secret`, and `hash` buffers at `src/mesh/mesh_wrapper.cpp:761-763` are not initialized, and the return values from `getString()` and `getBytes()` at `src/mesh/mesh_wrapper.cpp:765-769` are not checked. If `ch_cnt` exists but a secret or hash key is missing, truncated, or corrupt, `g_mesh->loadChannel()` at `src/mesh/mesh_wrapper.cpp:770` can receive random stack bytes as channel credentials. That can create unusable channels, wrong decryption keys, or nondeterministic behavior after NVS corruption or firmware migration.

**What's needed:** Zero-initialize all buffers, verify exact byte counts for secret and hash, verify non-empty and terminated names, and skip invalid channel records. Consider clearing bad channel keys or rebuilding `ch_cnt` after load. Add tests for partial NVS channel records.

---

### Finder ping countdown underflows during active ping

The Finder screen uses `pingCooldownRemaining()` while displaying the active 3-second listening window at `src/ui/screens.cpp:791-797`. That function reports the 30-second cooldown from `src/mesh/slop_mesh.h:734-738`, not the active ping window. Immediately after sending a ping, `remain` is roughly 30000 ms, so `uint32_t elapsed = 3000 - remain` underflows. The UI can display nonsensical elapsed values during the active listening period.

**What's needed:** Expose a separate active ping remaining value based on `_ping_sent_at + PING_WINDOW_MS`, or compute it in the wrapper. Keep cooldown display separate from active listening display.

---

## Settings

### Radio setup restarts immediately after flash writes

The Save & Reboot handler writes preferences, channels, and chat history at `src/ui/screens.cpp:3026-3029`, then calls `ESP.restart()` immediately at `src/ui/screens.cpp:3030`. There is no delay, flush confirmation, or failure handling after NVS/SPIFFS writes. This can lose the very settings that caused the reboot, especially on flash-backed storage under brownout, slow SPIFFS writes, or wear-leveling delays. Onboarding already uses a safer pattern with `delay(100)` before restart at `src/ui/onboarding_screen.cpp:411`, so the radio setup path is inconsistent.

**What's needed:** Check write return values where available, add a short post-save delay or explicit flush/end point, and only reboot after persistence is complete. If saving fails, keep the user on the setup screen and show an error instead of restarting.

---

### Date validation accepts impossible calendar dates

The settings date dialog accepts any day from 1 to 31 for any month at `src/ui/screens.cpp:1314-1321`. Onboarding uses the same loose validation before setting time at `src/ui/onboarding_screen.cpp:212-220`. Dates such as `2025-02-31` or `2025-04-31` pass validation and are handed to `makeEpoch()`, which normalizes them into a different real date. The UI then appears to accept one date while the system clock is set to another.

**What's needed:** Validate day-of-month using the selected month and leap-year rules before calling `makeEpoch()`. Keep the dialog open with an error message when the date is impossible.

---

## Display/UI

### Boot brightness ignores the saved display brightness

`src/hal/display.cpp:307-310` correctly applies `prefs_get().display_brightness` during display initialization. Immediately afterward, the backlight pulse at `src/hal/display.cpp:372-375` sets brightness to `0`, waits, then sets brightness to `255`. That final hardcoded `255` overrides the user's saved brightness on every boot. A user who selected a dim screen will still get maximum brightness until another path later reapplies preferences.

**What's needed:** Store the configured brightness before the pulse and restore that value afterward, or remove the pulse. Add a display mock test that verifies init ends at the saved brightness, not full scale.

---

### Theme compliance is inconsistent in several UI paths

The pixel theme requires zero-radius controls and theme constants rather than hardcoded colors. `src/ui/theme.h:104` creates circular signal dots with `LV_RADIUS_CIRCLE`, and several screens use hardcoded colors outside `theme.h`: map canvas/grid colors at `src/app/map_renderer.cpp:800-911`, settings danger/custom colors at numerous points in `src/ui/screens.cpp`, onboarding selected-region colors at `src/ui/onboarding_screen.cpp:261-277`, and remote-test UI colors at `src/test/test_controller.cpp:407-454`. Some map colors may be reasonable semantic colors, but they still bypass the central theme and make future theme changes incomplete. The circular signal dots directly violate the zero-radius rule.

**What's needed:** Move semantic colors into `src/ui/theme.h`, replace ad hoc `lv_color_hex(0x...)` values with named constants, and use zero-radius signal indicators unless a documented exception is intentionally accepted.

---

## Keyboard/Trackball/Touch

### Keyboard input uses a single-slot latch and can drop fast keys

The keyboard driver stores only one pending key in `current_key`/`latched_key` with a single `has_new_event` flag at `src/hal/keyboard.cpp:65-73`. When `slopos_keyboard_scan()` reads another key, it overwrites the pending value at `src/hal/keyboard.cpp:150-153`. This means two key events arriving before LVGL consumes the first can lose the earlier character. The 5 ms polling interval at `src/hal/keyboard.cpp:62` makes this plausible during fast typing, repeat, or serial-injected remote input.

**What's needed:** Replace the single latch with a small ring buffer. Scanning and remote injection should enqueue keys, and the LVGL input callback should dequeue one key at a time. Add tests that queue multiple keys before consumption and verify order and no loss.

---

### GT911 initialization reads and writes a 186-byte I2C block in one transaction

`src/hal/touch.cpp:128-134` reads the GT911 186-byte configuration block and writes it back in a single I2C transaction. Typical Arduino `Wire` buffers are much smaller unless explicitly resized, and this code does not chunk the transfer or check the write result. On real hardware this can silently fail the config rewrite, truncate the transaction, or depend on board-core buffer internals. It also makes touch initialization fragile across ESP32 Arduino versions.

**What's needed:** Either remove the full config rewrite if it is not needed, or chunk reads/writes according to the actual Wire buffer capacity. Check and debug-log the write result, and add a mock test that forces short-buffer failure.

---

## GPS

### NMEA checksum validation still accepts missing or malformed checksums

`src/hal/gps.cpp:146-169` implements checksum validation, but it returns true when a sentence has no `*`, has too few checksum bytes, or has non-hex checksum characters at `src/hal/gps.cpp:153-159`. `process_nmea()` then treats those sentences as valid at `src/hal/gps.cpp:171-180`. That means noisy or truncated GPS input can still update coordinates, fix state, date, speed, or heading as long as the checksum is absent or malformed.

**What's needed:** Require a valid `*XX` checksum for hardware NMEA sentences. If tests need checksum-less fixtures, gate that behind a test-only parser option instead of the production parser default.

---

### RMC void status is ignored

`src/hal/gps.cpp:125-144` parses RMC speed, heading, and date, but does not inspect the RMC status field. A `$GPRMC` or `$GNRMC` sentence with status `V` (void) can still update date, speed, and heading. GPS time sync later depends on `gps.has_fix` and `gps.year >= 2020` at `src/hal/gps.cpp:207-224`. If a previous GGA set `has_fix`, then a later void RMC updates the date, the firmware can sync RTC from invalid RMC data.

**What's needed:** Parse RMC field 2 and ignore speed, heading, and date unless status is `A`. Also consider clearing or aging fix state when GGA reports no fix so stale fix state cannot combine with later invalid date data.

---

## Terminal

### Terminal pruning deletes LVGL objects synchronously from an event callback

`term_add_line()` caps terminal output by calling `lv_obj_del(first)` while pruning old labels at `src/ui/screens.cpp:2192-2198`. The same function is called from the terminal textarea `LV_EVENT_READY` handler at `src/ui/screens.cpp:2271-2321`. Deleting LVGL objects synchronously while LVGL is dispatching an event can invalidate objects the event loop still expects to touch. This is especially reachable through the `emoji-list` command, which adds many lines from `src/ui/screens.cpp:2300-2312`.

**What's needed:** Use `lv_obj_del_async()` for terminal pruning or schedule pruning outside the active event callback. Add a terminal test that submits commands after the log reaches 64 lines.

---

## Chat Screen

### Messages are shown as sent even when mesh send fails

`chat_screen_send_current()` calls `sendMessage()` or `sendChannelMessage()` at `src/ui/chat_screen.cpp:947-948`, but ignores the returned boolean. It then appends the message as self-sent, refreshes the UI, and clears the input at `src/ui/chat_screen.cpp:950-955`. When the radio is unconfigured, the mesh is unavailable, the destination is invalid, or a channel send fails, the UI still shows a successful sent message. That makes local chat history diverge from actual mesh delivery.

**What's needed:** Check the send return value. On failure, keep the text in the input or append a visibly failed local message with retry/delete behavior. Add tests using the mesh mock to force `false` from both DM and channel sends.

---

### Conversation history is keyed by mutable channel indices

Chat state is stored in parallel fixed arrays such as `ch_msgs`, `ch_msg_count`, and `ch_meta` at `src/ui/chat_screen.cpp:108-130`. `refresh_channels()` rebuilds `dyn_channels` from the mesh export at `src/ui/chat_screen.cpp:249-261`, but it does not remap the message arrays by stable channel name. DM channels are appended dynamically at `src/ui/chat_screen.cpp:1343-1352`. After refresh, load, or channel-order changes, messages can remain attached to the same numeric index while the visible channel at that index changes, or DM conversations can disappear from the channel list.

**What's needed:** Store conversations by stable key, such as channel name or DM peer name, and rebuild the visible list from that keyed store. Remap message buffers on channel refresh and persist DMs as first-class conversations.

---

## Contacts

### Trace button crashes on allocation failure

The contact detail screen allocates an `int` for trace button user data at `src/ui/screens.cpp:751-754`, then immediately dereferences it without checking for `nullptr`. On memory pressure, `malloc()` can fail and this path will crash before the LVGL event callback is ever installed. This is a small allocation, but the firmware runs on constrained RAM and LVGL screens can already consume heap.

**What's needed:** Avoid heap allocation here by storing the small index through `intptr_t`, or check `malloc()` before dereference and skip creating the trace action if allocation fails.

---

## Map

### Tile discovery silently truncates zoom levels after 512 x-columns

`src/app/map_renderer.cpp:410-418` uses a static `MAX_XCOLS = 512` cache while scanning tile x-directories. Once `xcache_count` reaches 512, discovery stops even if more x columns exist. Large offline map sets or high zoom levels can exceed this cap. The computed coverage bounds then represent only the first 512 scanned directories, so auto-centering, clamping, and tile availability can ignore valid map data.

**What's needed:** Replace the fixed x-column cap with dynamic allocation, a streaming two-pass scan, or overflow detection that disables bounds clamping for that zoom. At minimum, report overflow in diagnostics so users know the tile set was truncated.

---

## SD Card

### Generic SD write helper appends instead of replacing files

`slopos_sdcard_write()` opens files with `FILE_WRITE` at `src/hal/sdcard.cpp:108-117`. In Arduino SD implementations, `FILE_WRITE` commonly opens for append rather than truncate. A caller expecting "write this buffer as the file contents" can leave old trailing bytes or duplicate data after repeated writes. The helper name does not communicate append semantics, so future use for settings, manifests, or logs is easy to misuse.

**What's needed:** Either rename the helper to make append semantics explicit, or implement replacement semantics by opening with a truncating write mode or removing the existing file before writing. Add an SD mock test that writes a longer buffer, then a shorter buffer, and verifies the final file length.

---

## Testing

### Remote test mode initializes the LoRa radio despite docs saying it does not

`src/main.cpp:67-73` prints "LoRa radio disabled" in `SLOPOS_REMOTE_TEST`, then immediately calls `slopos::mesh::init(spiffs_ok)`. That function has no remote-test guard: `src/mesh/mesh_wrapper.cpp:214-314` initializes the board, allocates radio objects, hard-resets SX1262, starts LoRa SPI, and calls `radio_module->std_init()`. The test controller header also says no LoRa radio is initialized at `src/test/test_controller.cpp:4-8`. This is a safety and test-validity problem. Remote test mode is documented as serial-only simulation, but the code can still bring up the radio, and debug builds broadcast an advert unconditionally at `src/mesh/mesh_wrapper.cpp:398-403`.

**What's needed:** Add a compile-time `SLOPOS_REMOTE_TEST` branch in mesh initialization that never allocates, resets, initializes, receives, or transmits through SX1262. Mesh APIs should return deterministic simulated values or false as documented. Add native tests or build checks that assert remote mode does not call radio initialization paths.

---

## How to Help

Pick any item from the list above and open a PR against the `dev` branch. See [`CONTRIBUTING.md`](./CONTRIBUTING.md) for the full contribution workflow.
