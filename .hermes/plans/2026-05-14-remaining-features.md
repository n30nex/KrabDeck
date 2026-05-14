# Remaining Features Implementation Plan

> **Goal:** Complete all TODO features in priority order, each with tests.

**Priority:** Touch → Keyboard → Mesh messaging → GPS → SD card → Maps

---

### Feature 1: GT911 Touch Driver

**Goal:** Make the touchscreen work so UI elements are tappable.

**Files to create:**
- `src/hal/touch.h` — GT911 driver API
- `src/hal/touch.cpp` — I2C GT911 implementation
- `test/unit/test_touch.cpp` — Touch coordinate mapping tests

**Files to modify:**
- `src/hal/display.cpp` — Wire in real touch callback
- `src/hal/tdeck_pins.h` — Already has GT911 pins defined
- `platformio.ini` — Add Wire lib dep if needed
- `test/integration/test_build.cpp` — Add touch header include

**Architecture:**
- GT911 communicates over I2C at address 0x5D
- Interrupt on GPIO 16 signals new touch data
- Read 5-point touch registers (0x814E+)
- Convert raw coords to display coords (swap XY if needed)
- Feed LVGL pointer indev in `lvgl_touch_cb()`

**Key functions:**
```cpp
bool slopos_touch_init();      // Init I2C + GT911 config
void slopos_touch_loop();      // Poll touch state (called from display loop)
bool slopos_touch_get(int* x, int* y, bool* pressed);
```

**Tests:** Coordinate mapping, boundary clamping, multitouch parsing, touch→release→touch lifecycle.

---

### Feature 2: Keyboard Matrix Driver

**Goal:** Physical QWERTY keyboard sends key events to LVGL.

**Files to create:**
- `src/hal/keyboard.h`
- `src/hal/keyboard.cpp`
- `test/unit/test_keyboard.cpp`

**Files to modify:**
- `src/hal/display.cpp` — Wire in real keyboard callback
- `test/integration/test_build.cpp`

**Architecture:**
- 4 rows (GPIO 4,5,6,7), 5 cols (GPIO 14,15,21,47,48)
- Scan: drive one col LOW at a time, read rows
- Keymap: row×col → LVGL key code
- Debounce: require 2 consecutive identical reads

**Test matrix scan logic, keymap correctness, debounce.**

---

### Feature 3: Full Mesh Messaging

**Goal:** Send/receive text messages over MeshCore protocol.

**Files to create:**
- `test/unit/test_mesh_messaging.cpp`

**Files to modify:**
- `src/mesh/mesh_wrapper.cpp` — Implement send_direct, send_channel, message receive queue
- `src/mesh/mesh_wrapper.h` — Add inbound message polling API
- `src/ui/chat_screen.cpp` — Wire real mesh messages into chat
- `src/ui/ui.cpp` — Poll for new mesh messages in loop()

**Architecture:**
- Send: construct MeshCore PathPacket, encrypt, dispatch via radio
- Receive: radio_driver.loop() delivers packets → parse → add to receive queue
- UI integration: poll queue in ui::loop(), feed to chat_screen_add_msg()

**New mesh API:**
```cpp
struct MeshMessage { char sender[32]; char text[256]; uint32_t timestamp; };
int  mesh_get_pending(MeshMessage* out, int max);  // drain receive queue
void mesh_set_own_name(const char* name);          // set our node name
```

---

### Feature 4: GPS Support

**File:** `src/hal/gps.h` / `src/hal/gps.cpp`
Parse NMEA sentences from Serial1 (RX=43, TX=44, 38400 baud).

---

### Feature 5: SD Card Support

**File:** `src/hal/sdcard.h` / `src/hal/sdcard.cpp`
SPI init, mount, file read/write. Used for maps, logs.

---

### Feature 6: Offline Map Rendering

**File:** `src/app/map_renderer.h` / `src/app/map_renderer.cpp`
Parse .mbtiles or raster tiles, render to LVGL canvas. Low priority.
