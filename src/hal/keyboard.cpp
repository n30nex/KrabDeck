#include "keyboard.h"
#include "tdeck_pins.h"
#include <Arduino.h>
#include <cstring>

// ════════════════════════════════════════════════════════
// Keyboard matrix constants
// ════════════════════════════════════════════════════════
static constexpr int ROWS             = KB_ROWS;  // 4
static constexpr int COLS             = KB_COLS;  // 5
static constexpr int DEBOUNCE_SCANS   = 2;        // consecutive scans to confirm
static constexpr uint32_t SCAN_DELAY  = 5;       // ms between full scans

static constexpr int ROW_PINS[ROWS] = {
    PIN_KB_ROW0, PIN_KB_ROW1, PIN_KB_ROW2, PIN_KB_ROW3
};
static constexpr int COL_PINS[COLS] = {
    PIN_KB_COL0, PIN_KB_COL1, PIN_KB_COL2, PIN_KB_COL3, PIN_KB_COL4
};

// ── QWERTY keymap ────────────────────────────────────────
// LVGL key codes for each matrix position (row × col)
// Modifier keys use 0x00 for key code (handled via modifier state)
static constexpr uint32_t KEYMAP[ROWS][COLS] = {
    // Col 0       Col 1       Col 2       Col 3       Col 4
    {  0x51/*Q*/,  0x57/*W*/,  0x45/*E*/,  0x52/*R*/,  0x54/*T*/  },  // Row 0
    {  0x41/*A*/,  0x53/*S*/,  0x44/*D*/,  0x46/*F*/,  0x47/*G*/  },  // Row 1
    {  0x5A/*Z*/,  0x58/*X*/,  0x43/*C*/,  0x56/*V*/,  0x42/*B*/  },  // Row 2
    {  0x00/*⇧*/,  0x00/*⌃*/,  0x00/*⌥*/,  0x0D/*↵*/,   0x20/*␣*/  },  // Row 3
};

// ── Debounce state ───────────────────────────────────────
static int   debounce_count[ROWS][COLS] = {{0}};
static bool  stable_keys[ROWS][COLS]   = {{false}};
static bool  prev_keys[ROWS][COLS]     = {{false}};
static uint32_t last_scan_ms = 0;
static bool  has_new_event   = false;

// Current key to report to LVGL (first pressed key)
static uint32_t current_key = 0;
static bool  shift_held = false;
static bool  ctrl_held  = false;
static bool  alt_held   = false;

// ════════════════════════════════════════════════════════
// PUBLIC API
// ════════════════════════════════════════════════════════

void slopos_keyboard_init()
{
    // Configure row pins as inputs with pullup
    for (int r = 0; r < ROWS; r++) {
        pinMode(ROW_PINS[r], INPUT_PULLUP);
    }

    // Configure column pins — start as inputs
    for (int c = 0; c < COLS; c++) {
        pinMode(COL_PINS[c], INPUT_PULLUP);
    }

    memset(debounce_count, 0, sizeof(debounce_count));
    memset(stable_keys, 0, sizeof(stable_keys));
    memset(prev_keys, 0, sizeof(prev_keys));
    current_key = 0;
    shift_held = false;
    ctrl_held  = false;
    alt_held   = false;
    has_new_event = false;
}

void slopos_keyboard_scan()
{
    uint32_t now = millis();
    if (now - last_scan_ms < SCAN_DELAY) return;
    last_scan_ms = now;

    bool raw[ROWS][COLS] = {{false}};

    // ── Matrix scan ────────────────────────────────────
    // Drive one column LOW at a time, read all rows
    for (int c = 0; c < COLS; c++) {
        // Set current column to OUTPUT LOW
        pinMode(COL_PINS[c], OUTPUT);
        digitalWrite(COL_PINS[c], LOW);

        // Small delay for signal settling
        delayMicroseconds(10);

        // Read all rows
        for (int r = 0; r < ROWS; r++) {
            raw[r][c] = (digitalRead(ROW_PINS[r]) == LOW);
        }

        // Return column to INPUT_PULLUP (high-Z)
        pinMode(COL_PINS[c], INPUT_PULLUP);
    }

    // ── Debounce ───────────────────────────────────────
    has_new_event = false;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (raw[r][c]) {
                if (debounce_count[r][c] < DEBOUNCE_SCANS) {
                    debounce_count[r][c]++;
                    if (debounce_count[r][c] == DEBOUNCE_SCANS && !stable_keys[r][c]) {
                        stable_keys[r][c] = true;
                        has_new_event = true; // key press detected
                    }
                }
            } else {
                if (debounce_count[r][c] > 0) {
                    debounce_count[r][c]--;
                    if (debounce_count[r][c] == 0 && stable_keys[r][c]) {
                        stable_keys[r][c] = false;
                        has_new_event = true; // key release detected
                    }
                }
            }
        }
    }

    // ── Determine current key and modifiers ──────────────
    current_key = 0;
    shift_held = false;
    ctrl_held  = false;
    alt_held   = false;

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (stable_keys[r][c]) {
                uint32_t k = KEYMAP[r][c];

                // Modifier keys (row 3, cols 0-2)
                if (r == 3 && c == 0) shift_held = true;
                else if (r == 3 && c == 1) ctrl_held = true;
                else if (r == 3 && c == 2) alt_held = true;
                else if (k != 0 && current_key == 0) {
                    current_key = k; // first non-modifier key
                }
            }
        }
    }

    // Track previous state for edge detection
    memcpy(prev_keys, stable_keys, sizeof(prev_keys));
}

uint32_t slopos_keyboard_get_key()
{
    return current_key;
}

bool slopos_keyboard_is_shift()  { return shift_held; }
bool slopos_keyboard_is_ctrl()   { return ctrl_held; }
bool slopos_keyboard_is_alt()    { return alt_held; }

bool slopos_keyboard_has_new_event()
{
    if (has_new_event) {
        has_new_event = false;
        return true;
    }
    return false;
}
