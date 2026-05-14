#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SlopOS-TDeck.
//
// SlopOS-TDeck is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SlopOS-TDeck is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SlopOS-TDeck.  If not, see <https://www.gnu.org/licenses/>.

#include <lvgl.h>

// SlopOS Dark Theme — Discord-inspired color palette
namespace slopos::theme {

// ── Backgrounds ─────────────────────────────────────────
constexpr uint32_t BG_PRIMARY   = 0x1a1a2e;  // deep navy
constexpr uint32_t BG_SECONDARY = 0x16213e;  // slightly lighter navy
constexpr uint32_t BG_TERTIARY  = 0x0f3460;  // card/panel background
constexpr uint32_t BG_INPUT     = 0x1e2a4a;  // input field

// ── Accent ──────────────────────────────────────────────
constexpr uint32_t ACCENT       = 0x5865f2;  // Discord blurple
constexpr uint32_t ACCENT_HOVER = 0x4752c4;
constexpr uint32_t ACCENT_GREEN = 0x3ba55d;  // online / success
constexpr uint32_t ACCENT_RED   = 0xed4245;  // notification / error
constexpr uint32_t ACCENT_ORANGE= 0xfaa61a;  // warning
constexpr uint32_t ACCENT_YELLOW= 0xfee75c;  // signal

// ── Text ────────────────────────────────────────────────
constexpr uint32_t TEXT_PRIMARY   = 0xdcddde;
constexpr uint32_t TEXT_SECONDARY = 0x8e9297;
constexpr uint32_t TEXT_MUTED     = 0x5c6067;
constexpr uint32_t TEXT_LINK      = 0x00aff4;

// ── Channel colors ──────────────────────────────────────
constexpr uint32_t CHANNEL_HASH   = 0x8e9297;  // #hashtag text
constexpr uint32_t CHANNEL_ACTIVE = 0xffffff;

// ── Apply dark theme to a screen ────────────────────────
inline void apply_dark_bg(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(BG_PRIMARY), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

inline void apply_card_style(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);
}

} // namespace slopos::theme
