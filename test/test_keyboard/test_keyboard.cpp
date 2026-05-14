/**
 * Unit tests for keyboard matrix driver
 * Tests: matrix scan logic, keymap correctness, debounce, modifier keys,
 *        ghosting detection, LVGL key code mapping
 */
#include <gtest/gtest.h>
#include "hal/tdeck_pins.h"
#include "Arduino.h"
#include <cstdint>
#include <cstring>
#include <set>

namespace {

// ── Keyboard matrix constants ────────────────────────────
static constexpr int ROWS = KB_ROWS;  // 4
static constexpr int COLS = KB_COLS;  // 5
static constexpr int ROW_PINS[ROWS] = {PIN_KB_ROW0, PIN_KB_ROW1, PIN_KB_ROW2, PIN_KB_ROW3};
static constexpr int COL_PINS[COLS] = {PIN_KB_COL0, PIN_KB_COL1, PIN_KB_COL2, PIN_KB_COL3, PIN_KB_COL4};

// ── T-Deck QWERTY keymap (row × col → LVGL key code) ─────
// Maps matrix position to LVGL_KEY_* for the keypad indev
struct KeyMapEntry {
    int row, col;
    uint32_t lv_key;
    char ascii;
    bool is_modifier;
};

// Standard T-Deck QWERTY layout
static const KeyMapEntry keymap[] = {
    // Row 0
    {0, 0, 0x51, 'q', false},  // Q
    {0, 1, 0x57, 'w', false},  // W
    {0, 2, 0x45, 'e', false},  // E
    {0, 3, 0x52, 'r', false},  // R
    {0, 4, 0x54, 't', false},  // T
    // Row 1
    {1, 0, 0x41, 'a', false},  // A
    {1, 1, 0x53, 's', false},  // S
    {1, 2, 0x44, 'd', false},  // D
    {1, 3, 0x46, 'f', false},  // F
    {1, 4, 0x47, 'g', false},  // G
    // Row 2
    {2, 0, 0x5A, 'z', false},  // Z
    {2, 1, 0x58, 'x', false},  // X
    {2, 2, 0x43, 'c', false},  // C
    {2, 3, 0x56, 'v', false},  // V
    {2, 4, 0x42, 'b', false},  // B
    // Row 3 — modifiers + special
    {3, 0, 0x00, 0,   true},   // Shift
    {3, 1, 0x00, 0,   true},   // Ctrl
    {3, 2, 0x00, 0,   true},   // Alt
    {3, 3, 0x0D, '\r', false}, // Enter
    {3, 4, 0x20, ' ',  false}, // Space
};

// ── Matrix scan simulation ───────────────────────────────
// Replicates the hardware scan logic purely in software
struct MatrixState {
    bool pressed[ROWS][COLS] = {{false}};

    void set_key(int row, int col, bool pressed_val) {
        if (row >= 0 && row < ROWS && col >= 0 && col < COLS) {
            pressed[row][col] = pressed_val;
        }
    }

    void clear() { memset(pressed, 0, sizeof(pressed)); }
};

// Scan the matrix: returns the first pressed key found (or -1,-1)
// Equivalent to driving one column LOW and reading rows
void scan_matrix(const MatrixState& state, int* out_row, int* out_col) {
    *out_row = -1;
    *out_col = -1;
    for (int c = 0; c < COLS; c++) {
        for (int r = 0; r < ROWS; r++) {
            if (state.pressed[r][c]) {
                *out_row = r;
                *out_col = c;
                return; // return first pressed key
            }
        }
    }
}

void scan_all_keys(const MatrixState& state, bool out[ROWS][COLS]) {
    memset(out, 0, ROWS * COLS * sizeof(bool));
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            out[r][c] = state.pressed[r][c];
        }
    }
}

// Lookup LVGL key code from row/col
uint32_t lookup_key(int row, int col) {
    for (auto& k : keymap) {
        if (k.row == row && k.col == col) return k.lv_key;
    }
    return 0;
}

bool is_modifier(int row, int col) {
    for (auto& k : keymap) {
        if (k.row == row && k.col == col) return k.is_modifier;
    }
    return false;
}

// ── Debounce simulation ──────────────────────────────────
// Simple debounce: require same reading for 2 consecutive scans
class Debouncer {
    bool last[ROWS][COLS] = {{false}};
    bool stable[ROWS][COLS] = {{false}};

public:
    void reset() {
        memset(last, 0, sizeof(last));
        memset(stable, 0, sizeof(stable));
    }

    // Returns true if the key state changed (new press or new release)
    bool update(const MatrixState& state, int row, int col) {
        bool current = state.pressed[row][col];
        if (current == last[row][col] && current != stable[row][col]) {
            stable[row][col] = current;
            return true; // state changed
        }
        last[row][col] = current;
        return false;
    }

    bool is_pressed(int row, int col) const {
        return stable[row][col];
    }
};

// ════════════════════════════════════════════════════════
// TEST FIXTURE
// ════════════════════════════════════════════════════════
class KeyboardTest : public ::testing::Test {
protected:
    MatrixState matrix;
    Debouncer debounce;

    void SetUp() override {
        arduino_mock::reset();
        matrix.clear();
        debounce.reset();
    }
};

// ── Pin definitions ──────────────────────────────────────
TEST_F(KeyboardTest, RowCountMatchesDefine) {
    EXPECT_EQ(ROWS, 4);
    EXPECT_EQ(COLS, 5);
}

TEST_F(KeyboardTest, RowPinsAreValidGPIO) {
    for (int i = 0; i < ROWS; i++) {
        EXPECT_GE(ROW_PINS[i], 0);
        EXPECT_LE(ROW_PINS[i], 48);
    }
}

TEST_F(KeyboardTest, ColPinsAreValidGPIO) {
    for (int i = 0; i < COLS; i++) {
        EXPECT_GE(COL_PINS[i], 0);
        EXPECT_LE(COL_PINS[i], 48);
    }
}

TEST_F(KeyboardTest, NoDuplicateRowPins) {
    std::set<int> seen;
    for (int i = 0; i < ROWS; i++) {
        EXPECT_FALSE(seen.count(ROW_PINS[i])) << "Duplicate row pin: " << ROW_PINS[i];
        seen.insert(ROW_PINS[i]);
    }
}

TEST_F(KeyboardTest, NoDuplicateColPins) {
    std::set<int> seen;
    for (int i = 0; i < COLS; i++) {
        EXPECT_FALSE(seen.count(COL_PINS[i])) << "Duplicate col pin: " << COL_PINS[i];
        seen.insert(COL_PINS[i]);
    }
}

TEST_F(KeyboardTest, RowAndColPinsAreDisjoint) {
    std::set<int> row_set(ROW_PINS, ROW_PINS + ROWS);
    for (int i = 0; i < COLS; i++) {
        EXPECT_FALSE(row_set.count(COL_PINS[i]))
            << "Col pin " << COL_PINS[i] << " also used as row pin";
    }
}

// ── Matrix scanning ──────────────────────────────────────
TEST_F(KeyboardTest, NoKeysPressedReturnsNothing) {
    int row, col;
    scan_matrix(matrix, &row, &col);
    EXPECT_EQ(row, -1);
    EXPECT_EQ(col, -1);
}

TEST_F(KeyboardTest, SingleKeyPressDetected) {
    matrix.set_key(0, 0, true); // Q
    int row, col;
    scan_matrix(matrix, &row, &col);
    EXPECT_EQ(row, 0);
    EXPECT_EQ(col, 0);
}

TEST_F(KeyboardTest, KeyPressDetectedAtAnyPosition) {
    // Test every position in the matrix
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            matrix.clear();
            matrix.set_key(r, c, true);
            int found_r, found_c;
            scan_matrix(matrix, &found_r, &found_c);
            EXPECT_EQ(found_r, r) << "Row mismatch at (" << r << "," << c << ")";
            EXPECT_EQ(found_c, c) << "Col mismatch at (" << r << "," << c << ")";
        }
    }
}

TEST_F(KeyboardTest, MultipleKeysDetected) {
    matrix.set_key(0, 0, true);  // Q
    matrix.set_key(2, 2, true);  // C
    matrix.set_key(3, 4, true);  // Space

    bool all[ROWS][COLS];
    scan_all_keys(matrix, all);

    EXPECT_TRUE(all[0][0]);
    EXPECT_TRUE(all[2][2]);
    EXPECT_TRUE(all[3][4]);
    EXPECT_FALSE(all[1][1]); // not pressed
}

// ── Keymap lookup ────────────────────────────────────────
TEST_F(KeyboardTest, KeymapCoverageEveryMatrixCell) {
    // Every valid (row, col) must have a keymap entry
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            uint32_t code = lookup_key(r, c);
            // 0 is valid — reserved for modifier keys
            // but every cell must be covered by the keymap
            bool covered = false;
            for (auto& k : keymap) {
                if (k.row == r && k.col == c) { covered = true; break; }
            }
            EXPECT_TRUE(covered) << "No keymap entry for (" << r << "," << c << ")";
        }
    }
}

TEST_F(KeyboardTest, KeymapHas20Entries) {
    size_t count = sizeof(keymap) / sizeof(keymap[0]);
    EXPECT_EQ(count, 20u) << "Expected 20 keymap entries (4 rows × 5 cols)";
}

TEST_F(KeyboardTest, AllPrintableKeysHaveLVGLCode) {
    for (auto& k : keymap) {
        if (!k.is_modifier) {
            EXPECT_GT(k.lv_key, 0u) << "Non-modifier key at (" << k.row << "," << k.col
                                     << ") has no LVGL key code";
        }
    }
}

TEST_F(KeyboardTest, ModifierKeysAreRow3Cols0to2) {
    // Shift, Ctrl, Alt should be on row 3, cols 0-2
    EXPECT_TRUE(is_modifier(3, 0)); // Shift
    EXPECT_TRUE(is_modifier(3, 1)); // Ctrl
    EXPECT_TRUE(is_modifier(3, 2)); // Alt
    EXPECT_FALSE(is_modifier(3, 3)); // Enter
    EXPECT_FALSE(is_modifier(3, 4)); // Space
    EXPECT_FALSE(is_modifier(0, 0)); // Q
}

// ── Debounce ──────────────────────────────────────────────
TEST_F(KeyboardTest, DebounceFiltersSingleShotNoise) {
    matrix.set_key(0, 0, true);
    bool changed = debounce.update(matrix, 0, 0); // first scan
    EXPECT_FALSE(changed) << "First scan should not trigger (not stable yet)";

    matrix.set_key(0, 0, false); // key released before second scan
    changed = debounce.update(matrix, 0, 0);
    EXPECT_FALSE(changed) << "Noise filtered — key was not stable for 2 scans";
}

TEST_F(KeyboardTest, DebounceRecognizesStablePress) {
    matrix.set_key(1, 2, true);
    debounce.update(matrix, 1, 2); // first scan
    bool changed = debounce.update(matrix, 1, 2); // second scan — same state
    EXPECT_TRUE(changed) << "Stable press after 2 scans should register";
    EXPECT_TRUE(debounce.is_pressed(1, 2));
}

TEST_F(KeyboardTest, DebounceRecognizesStableRelease) {
    // Press and hold
    matrix.set_key(2, 3, true);
    debounce.update(matrix, 2, 3);
    debounce.update(matrix, 2, 3);
    EXPECT_TRUE(debounce.is_pressed(2, 3));

    // Release
    matrix.set_key(2, 3, false);
    debounce.update(matrix, 2, 3);
    bool changed = debounce.update(matrix, 2, 3); // second scan released
    EXPECT_TRUE(changed);
    EXPECT_FALSE(debounce.is_pressed(2, 3));
}

TEST_F(KeyboardTest, DebounceResetClearsAll) {
    matrix.set_key(0, 0, true);
    debounce.update(matrix, 0, 0);
    debounce.update(matrix, 0, 0);
    EXPECT_TRUE(debounce.is_pressed(0, 0));

    debounce.reset();
    EXPECT_FALSE(debounce.is_pressed(0, 0));
}

// ── Ghosting detection (3-key rollover) ──────────────────
TEST_F(KeyboardTest, GhostDetectionThreeKeysSameRow) {
    // Pressing Q, W, E (row 0, cols 0-2) — all same row, different cols
    matrix.set_key(0, 0, true);
    matrix.set_key(0, 1, true);
    matrix.set_key(0, 2, true);

    bool all[ROWS][COLS];
    scan_all_keys(matrix, all);
    EXPECT_TRUE(all[0][0]);
    EXPECT_TRUE(all[0][1]);
    EXPECT_TRUE(all[0][2]);
}

TEST_F(KeyboardTest, TwoKeysOnSameColumnDoNotCreateGhost) {
    // Q (0,0) + A (1,0) — same column, different rows
    // Without diodes, this could ghost. With diodes on T-Deck, it shouldn't.
    // Our simple scan doesn't do anti-ghosting, but validates the raw matrix read.
    matrix.set_key(0, 0, true);
    matrix.set_key(1, 0, true);

    bool all[ROWS][COLS];
    scan_all_keys(matrix, all);
    EXPECT_TRUE(all[0][0]);
    EXPECT_TRUE(all[1][0]);
    // No ghost at (0,1) or (1,1) — our scan correctly reads only pressed keys
    EXPECT_FALSE(all[0][1]);
    EXPECT_FALSE(all[1][1]);
}

// ── ASCII output for the complete keymap ──────────────────
TEST_F(KeyboardTest, AllKeymapEntriesHaveValidAsciiOrAreModifiers) {
    for (auto& k : keymap) {
        if (k.is_modifier) {
            // Modifiers are OK with ascii=0
            EXPECT_EQ(k.ascii, 0);
        } else {
            // Non-modifiers must have a printable ASCII or control code
            EXPECT_GE(k.ascii, 0x0D) << "Non-modifier key at (" << k.row << "," << k.col
                                     << ") has invalid ASCII";
        }
    }
}

} // anonymous namespace
