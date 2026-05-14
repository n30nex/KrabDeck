#pragma once
#include <cstdint>

// Initialize the GT911 touch controller over I2C
// Must be called after Wire.begin() and before LVGL init
// Returns true on successful initialization
bool slopos_touch_init();

// Call this each frame to poll for new touch data
// (called from slopos_display_loop)
void slopos_touch_loop();

// Get the current touch state
// Returns true if a touch is active, and fills x/y with position
// Coordinates are already mapped to display space (0-319, 0-239)
bool slopos_touch_get(int* out_x, int* out_y, bool* out_pressed);

// Is the touch controller initialized and responding?
bool slopos_touch_ready();
