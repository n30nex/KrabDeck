#pragma once
#include <cstdint>

// ── T-Deck Keyboard (ESP32-C3 via I2C) ─────────────────
// The T-Deck keyboard is a separate ESP32-C3 MCU on I2C address 0x55.
// It handles matrix scanning, debouncing, modifier keys, and backlight.
// The main ESP32-S3 reads key codes over I2C.

// Initialize communication with the keyboard MCU
// Must be called after I2C bus is configured (Wire.begin)
// Returns true if keyboard is detected
bool slopos_keyboard_init();

// Poll the keyboard for new keypresses (call each frame)
// Reads 1 byte from I2C — non-zero means a key was pressed
void slopos_keyboard_scan();

// Get the key code of the last keypress (ASCII char, or 0 if none)
uint32_t slopos_keyboard_get_key();

// Returns true if a new key event is available (one-shot, consumed on read)
bool slopos_keyboard_has_new_event();

// ── Backlight control ──────────────────────────────────
// Set keyboard backlight brightness (0-255, 0=off)
void slopos_keyboard_set_brightness(uint8_t duty);

// Set the default brightness used when toggling with Alt+B (30-255)
void slopos_keyboard_set_default_brightness(uint8_t duty);

// ── Modifier state (derived from key codes, not I2C) ───
bool slopos_keyboard_is_shift();
bool slopos_keyboard_is_ctrl();
bool slopos_keyboard_is_alt();

// Reset internal scan state (poll timer, key buffer, modifier flags).
// Does NOT reset the `initialized` flag or I2C communication.
// Useful for testing and on device wake from deep sleep.
void slopos_keyboard_reset_scan_state();
