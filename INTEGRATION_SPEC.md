# SlopOS-TDeck Integration Spec — beta-0.1.27 Pre-Ship Review

## T-Deck Hardware (LilyGo T-Deck Plus)
- **MCU:** ESP32-S3 @ 240MHz, 16MB Flash, 8MB PSRAM
- **Display:** ST7789 320×240 (native 240×320 portrait, rotation(1) → landscape)
- **Input:** GT911 capacitive touch (I2C 0x5D), ESP32-C3 keyboard MCU (I2C 0x55), trackball button (GPIO 0)
- **Radio:** SX1262 LoRa (SPI: NSS=9, SCK=40, MISO=38, MOSI=41, DIO1=45, RST=17, BUSY=13)
- **GPS:** L76K GNSS on Serial1 (RX=43, TX=44, 38400 baud, $GN prefix)
- **SD:** SPI (CS=21), max 32GB FAT32
- **Peripheral power:** GPIO 10 (HIGH to enable)
- **Battery:** ADC GPIO 4, voltage divider

## 1. Boot Sequence (main.cpp)
- [ ] Serial at 115200 baud, boot step prints (1-8)
- [ ] Board init: peripheral power ON, SPI, Wire, ADC
- [ ] SPIFFS mount (graceful fallback if fails — ephemeral identity mode with warning)
- [ ] GPS init (non-blocking, works without fix)
- [ ] SD card init (non-blocking, works without card)
- [ ] Display init: LovyanGFX ST7789 + LVGL v9 with tick source
  - Backlight attached to panel (`setLight(&_light)`)
  - Rotation(1) for landscape, `lv_tick_set_cb()` registered
  - Auto-off after 30s inactivity, wake on touch/keyboard
- [ ] Radio init: SX1262 with explicit RST toggle + 10ms TCXO stabilization
  - Uses compile-time defaults (869.525 MHz, BW 62.5, SF8) if no saved config
  - Radio MUST NOT transmit until user explicitly configures (configured=false gate)
  - Identity loaded from SPIFFS or generated fresh
- [ ] UI splash screen shown (SlopOS logo, subtitle, loading bar, 2s auto-transition)
- [ ] Low-battery check every 30s in loop() — deep sleep if critical
- [ ] Map init deferred until after splash (if SD mounted)

## 2. Display & Input (hal/)
- [ ] ST7789: backlight PWM works (0-255), brightness controllable
- [ ] Auto-off timer: 30s idle → backlight 0, any touch/key → backlight restored
- [ ] GT911 touch: coordinates correctly transformed for rotation(1)
  - SWAP_XY=false, MIRROR_X=true for rotation(1) landscape
  - Boundary clamping to 0-319 x 0-239
  - Multitouch parsing with sentinel filtering (0xFFFF, 0x0000)
- [ ] Keyboard: I2C probe at 0x55, backlight + key mode init
  - Key events consumed, no stuck-key repeats
  - LVGL keypad indev registered with group

## 3. Mesh Networking (mesh/)
- [ ] SlopMesh subclass: overrides onPeerDataRecv, onAdvertRecv, searchPeersByHash, getPeerSharedSecret
- [ ] Direct text messaging: sendMessage() → createDatagram() → sendFlood()
  - Payload: [4-byte LE timestamp][null-terminated text]
  - Total <= 150 chars (safe margin below MAX_PACKET_PAYLOAD - CIPHER_MAC_SIZE)
- [ ] Channel/group messaging: sendChannelMessage() → createGroupDatagram() → sendFlood()
  - Channel PSK: base64 decode → SHA256 → channel hash
- [ ] Contact discovery: onAdvertRecv() parses AdvertDataParser (getName, getLat, getLon)
- [ ] Contact list: exportContactsFull() returns up to 128 contacts with name, RSSI, last_seen
  - last_seen is RTC timestamp, not millis() — age calc uses getCurrentTime()
- [ ] Identity persistence: loadIdentity() on boot, saveState() every ~5 min
- [ ] Message queue: ring buffer in mesh_wrapper, pollMessages() drains
- [ ] Trace route: sendTrace() uses createTrace, onPathRecv() stores path per contact
  - Path size clamped to MAX_PATH_SIZE, validity checked
- [ ] Radio safety: configured=false gate prevents TX until Radio Setup saved
  - Startup advert suppressed when not configured
- [ ] Null termination: all incoming peer/group text forced null-terminated
- [ ] Payload caps: text buffers capped at 150 chars

## 4. All 14 Screens (ui/)
### Splash → Home
- [ ] 2s delay with loading bar, then auto-transition to Home

### Home Screen (home_screen.cpp)
- [ ] Top bar: hamburger (LV_SYMBOL_LIST), channel pills (dynamic from mesh), 24h time
- [ ] Bottom bar: device name "SlopOS-TDeck", signal bars (cyan), battery % (cyan/red)
- [ ] 4×3 icon grid, no scroll (LV_DIR_NONE), 12 tiles all visible
- [ ] Icons: LVGL FontAwesome symbols, uppercase labels
- [ ] Order per spec: Chat, Contacts, Repeaters, Finder, Heard, Map, Advertise, Settings, Trace, Terminal, Noise, Signal

### Chat Screen (chat_screen.cpp)
- [ ] Horizontal scrollable channel pill bar (dynamic from mesh, MRU-sorted)
- [ ] Outgoing messages: solid cyan (#00BFFF) bubbles, right-aligned, white text
- [ ] Incoming messages: light blue-gray (#3A4560) bubbles, left-aligned, white text
- [ ] HH:MM timestamps on every message
- [ ] Sender names in small text above incoming bubbles
- [ ] Textarea + Send button at bottom, Enter-to-send
- [ ] Auto-scroll to bottom on new message
- [ ] 50-message trim (ring buffer)
- [ ] Fallback: single "#general" channel if no mesh channels discovered

### Heard Screen (screens.cpp)
- [ ] 5-column table: Name (40%), Sig (15%), Dist (15%), Area (15%), Time (15%)
- [ ] Proportions calculated from responsive.h helpers
- [ ] Age display: <60s → "Xs", <3600s → "Xm", else "Xh"
- [ ] Uses getCurrentTime() for accurate age, not millis()

### Map Screen (app/map_renderer.cpp)
- [ ] Touch pan and +/- zoom
- [ ] JPEG tiles from SD card (/maps/{z}/{x}/{y}.jpg)
- [ ] ESP32 TJpgDec decoder
- [ ] PSRAM allocator via lv_malloc (canvas 153KB, tile buffers 131KB each, LRU cache 524KB)
- [ ] Map deinit on screen delete (frees canvas, JPEG buffers, LRU cache)
- [ ] Image descriptor properly initialized (magic, src, dimensions)

### Settings Screen (screens.cpp)
- [ ] All values pulled from mesh::prefs() or build defines, NOT hardcoded strings
- [ ] Node name, frequency, SF, power, SD status, GPS status, version
- [ ] Version: reads SLOPOS_VERSION from tdeck_pins.h

### Radio Setup (accessible from Settings)
- [ ] Frequency selector: regional presets (UK 869.525/869.618, EU 868.000/433.500, US 915.000)
- [ ] SF selector: 7-12
- [ ] Power selector: 2-22 dBm
- [ ] Save & Reboot button
- [ ] All widgets fit within 240px height (content band y=22 to y=218)
- [ ] configured=true after save

### Signal Screen
- [ ] Live RSSI, SNR, noise floor from radio
- [ ] Frequency, BW, SF, CR, TX power from mesh::prefs()

### Noise Screen
- [ ] Live noise floor bar (green < -120, orange -120 to -90, red > -90)

### Trace Screen
- [ ] Contact list with [has path]/[no path] indicators
- [ ] Send trace on tap → result display with hop count + SNR
- [ ] Dangling pointer guard: trace_result_label reset on screen delete

### Terminal Screen
- [ ] Green-on-black, help/status/advert/ping commands
- [ ] No "\\n" literal bug (single backslash in C strings)

### Advertise Screen
- [ ] Send advert button calls mesh::sendAdvert()
- [ ] Includes GPS lat/lon when fix available

### Contacts/Repeaters/Finder Screens
- [ ] Each shows live data from exportContacts()
- [ ] No duplicate content across screens (or clearly differentiated)

## 5. GPS (hal/gps.cpp)
- [ ] Parser accepts both $GP and $GN NMEA prefixes
- [ ] Serial1.begin() with explicit pin arguments (RX=43, TX=44, 38400)
- [ ] Production parser function (not test copy) — verified no off-by-one strtok bug
- [ ] GGA: time, lat, lon, altitude parsed correctly
- [ ] RMC: date, time, speed, course parsed correctly

## 6. SD Card (hal/sdcard.cpp)
- [ ] SPI at safe speed, CS=21
- [ ] FAT32 only, directory listing, file read
- [ ] Map tile lookup: /maps/{z}/{x}/{y}.jpg

## 7. Power Management
- [ ] Battery ADC: raw → mV → % (with clamping)
- [ ] Peripheral power: GPIO 10 HIGH during operation, LOW before deep sleep
- [ ] Display backlight: OFF before deep sleep
- [ ] Deep sleep: esp_deep_sleep_start() after powering down peripherals

## 8. Error Handling & Edge Cases
- [ ] SPIFFS failure → ephemeral identity, warning on serial, device still boots
- [ ] SD card missing → map screen shows "no SD" placeholder
- [ ] GPS no fix → screens show "--" for coordinates
- [ ] Radio init failure → warning on serial, device still boots with mesh disabled
- [ ] PSRAM alloc failure → map init fails gracefully
- [ ] No mesh contacts → screens show "no nodes nearby"
- [ ] No channels → chat shows "#general" placeholder
- [ ] RTC returns 0 → Network screen shows "--" not "0s ago"

## 9. Build & Test
- [ ] `pio run -e SlopOS_TDeck` compiles with 0 errors, 0 warnings
- [ ] All 160 tests pass: `pio test -e native_test` (1 skipped OK)
- [ ] No TODOs, FIXMEs, HACKs in source
- [ ] CLAUDE.md is gitignored
- [ ] No personal info in committed files
- [ ] `-Wall -Wextra` (not `-w`) in platformio.ini

## 10. On-Device Validation (when T-Deck arrives)
- [ ] Screen backlight pulse visible on boot
- [ ] Splash screen → Home screen transition (2s)
- [ ] Touch works across all 4 corners
- [ ] Keyboard types into chat textarea
- [ ] Trackball button navigates
- [ ] Radio receives packets (visible in Heard screen)
- [ ] Send message works (peer receives)
- [ ] GPS acquires fix if outdoors
- [ ] SD card lists map tiles
- [ ] Auto-off dims screen after 30s idle
- [ ] Low battery triggers deep sleep
