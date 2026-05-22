#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Responsive layout helpers — adapt UI to any display size/orientation.
// All functions are constexpr where possible for zero runtime overhead.

#include "hal/tdeck_pins.h"
#include <algorithm>

namespace slopos::responsive {

// ── Display metadata ────────────────────────────────────
constexpr int DISPLAY_W = TFT_WIDTH;
constexpr int DISPLAY_H = TFT_HEIGHT;
constexpr bool IS_PORTRAIT  = (DISPLAY_H > DISPLAY_W);
constexpr bool IS_LANDSCAPE = (DISPLAY_W > DISPLAY_H);
constexpr bool IS_SQUARE    = (DISPLAY_W == DISPLAY_H);

// ── Safe-area insets (tune for round/cutout displays) ────
constexpr int SAFE_LEFT   = 0;
constexpr int SAFE_RIGHT  = 0;
constexpr int SAFE_TOP    = 0;
constexpr int SAFE_BOTTOM = 0;

// ── Bar heights — proportional, with minimums ────────────
// Scaled to ~9% of display height, clamped for readability.
constexpr int bar_height() {
    return (DISPLAY_H / 11 < 12) ? 12 : (DISPLAY_H / 11 > 28) ? 28 : DISPLAY_H / 11;
}

constexpr int TOP_BAR_H  = bar_height();
constexpr int BOT_BAR_H  = bar_height() - 2;  // slightly smaller
constexpr int DIVIDER_H  = 1;

// ── Content area ─────────────────────────────────────────
constexpr int CONTENT_Y  = TOP_BAR_H + DIVIDER_H + SAFE_TOP;
constexpr int CONTENT_H  = DISPLAY_H - CONTENT_Y - DIVIDER_H - BOT_BAR_H - SAFE_BOTTOM;
constexpr int CONTENT_X  = SAFE_LEFT;
constexpr int CONTENT_W  = DISPLAY_W - SAFE_LEFT - SAFE_RIGHT;

// ── Hashtag label width — available space in top bar ─────
// Leaves room for hamburger icon (left) and time label (right).
inline int HASHTAG_LABEL_W() {
    return DISPLAY_W - 60;  // hamburger(20) + time(32) + margins(8)
}

// ── Grid layout — adaptive columns/rows ──────────────────
struct GridLayout { int cols; int rows; int tile_w; int tile_h; };

inline GridLayout compute_grid(int grid_pad = 3) {
    GridLayout g{};
    int avail_h = CONTENT_H - (grid_pad * 2);
    int avail_w = CONTENT_W - (grid_pad * 2);

    // Choose column count based on available width per tile.
    // Tiles need ~75px minimum for icon (18px symbol) + label (12px text).
    if (avail_w >= 300)      g.cols = 4;   // ≥300px: 4 cols (~75px each)
    else if (avail_w >= 230) g.cols = 3;   // ≥230px: 3 cols
    else if (avail_w >= 170) g.cols = 2;   // ≥170px: 2 cols
    else                     g.cols = 1;   // narrow: single column

    // Remove one column's worth of flex gaps
    int cols_w = avail_w - (grid_pad * (g.cols - 1));
    g.tile_w = cols_w / g.cols;

    // Choose row count based on available height per tile.
    // Tiles need ~40px minimum for icon (20px) + label (12px) + padding.
    int min_tile_h = 40;
    g.rows = avail_h / (min_tile_h + grid_pad);

    // Clamp: use at least 1 row, and try to fill available space
    if (g.rows < 1) g.rows = 1;
    if (g.rows > 12) g.rows = 12;  // max 12 icons (current set)

    // Enforce minimum: at least g.cols rows of tiles visible
    if (g.rows < 3 && avail_h >= 120) g.rows = 3;

    // Calculate actual tile height
    int rows_h = avail_h - (grid_pad * (g.rows - 1));
    g.tile_h = rows_h / g.rows;

    // Ensure all 12 icons can be displayed via scrolling if needed
    // (grid may need LV_DIR_VER if g.cols * g.rows < 12)

    return g;
}

// ── Proportional column widths ───────────────────────────
// Distributes 'total_w' among 'n' columns using given weights.
// Returns each column's x-offset in out_offsets (must be size n).
template<int N>
inline void column_offsets(const int (&weights)[N], int total_w,
                               int (&out_x)[N], int start_x = 0) {
    int sum = 0;
    for (int i = 0; i < N; i++) sum += weights[i];
    int x = start_x;
    for (int i = 0; i < N; i++) {
        out_x[i] = x;
        x += (total_w * weights[i]) / sum;
    }
}

// ── Dialog size — capped to display ──────────────────────
struct DialogSize { int w; int h; DialogSize() : w(0), h(0) {} DialogSize(int w_, int h_) : w(w_), h(h_) {} };

inline DialogSize dialog_size(int desired_w, int desired_h,
                              int margin = 16) {
    return DialogSize(
        std::min(desired_w, DISPLAY_W - margin * 2),
        std::min(desired_h, DISPLAY_H - margin * 2));
}

// ── Widget width — capped as percentage of display ────────
inline int capped_width(int desired_w, int max_pct = 90) {
    int cap = (DISPLAY_W * max_pct) / 100;
    return std::min(desired_w, cap);
}

// ── Bar-height widget — scaled proportionally ────────────
inline int bar_widget_h(int pct_of_content = 33) {
    return (CONTENT_H * pct_of_content) / 100;
}

} // namespace slopos::responsive
