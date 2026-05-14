#pragma once
#include <cstdint>

// Initialize the keyboard matrix scanner
// Must be called after GPIO pins are configured
void slopos_keyboard_init();

// Scan the keyboard matrix each frame (called from display loop)
// Debounces inputs and tracks key state changes
void slopos_keyboard_scan();

// Get the currently pressed key as an LVGL key code (0 if none)
// Returns the key code that should be sent to LVGL's keypad indev
uint32_t slopos_keyboard_get_key();

// Check if a modifier key is currently held
bool slopos_keyboard_is_shift();
bool slopos_keyboard_is_ctrl();
bool slopos_keyboard_is_alt();

// Check if any key changed state this scan (for LVGL indev)
bool slopos_keyboard_has_new_event();
