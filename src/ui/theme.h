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

// SlopOS Pixel Theme — Discord-inspired palette with blocky pixel styling
namespace slopos::theme {

// ── Backgrounds ─────────────────────────────────────────
constexpr uint32_t BG_PRIMARY   = 0x0f0f0f;  // deep black
constexpr uint32_t BG_SECONDARY = 0x181818;  // status bars
constexpr uint32_t BG_TERTIARY  = 0x1e1e1e;  // card/icon tile background
constexpr uint32_t BG_INPUT     = 0x252525;  // input field

// ── Accent ──────────────────────────────────────────────
constexpr uint32_t ACCENT       = 0x00bfff;  // bright cyan
constexpr uint32_t ACCENT_HOVER = 0x00a5e0;
constexpr uint32_t ACCENT_GREEN = 0x3ba55d;
constexpr uint32_t ACCENT_RED   = 0xed4245;
constexpr uint32_t ACCENT_ORANGE= 0xfaa61a;
constexpr uint32_t ACCENT_YELLOW= 0xfee75c;

// ── Message bubbles ──────────────────────────────────────
constexpr uint32_t MSG_INCOMING = 0x3a4560;

// ── Text ────────────────────────────────────────────────
constexpr uint32_t TEXT_PRIMARY   = 0xf2f3f5;
constexpr uint32_t TEXT_SECONDARY = 0x949ba4;
constexpr uint32_t TEXT_MUTED     = 0x6b7078;
constexpr uint32_t TEXT_LINK      = 0x00aff4;

// ── Channel colors ──────────────────────────────────────
constexpr uint32_t CHANNEL_HASH   = 0x00bfff;
constexpr uint32_t CHANNEL_ACTIVE = 0xffffff;

// ── Structural ───────────────────────────────────────────
constexpr uint32_t DIVIDER        = 0x2a2a2a;

// ── Pixel border width ───────────────────────────────────
constexpr int32_t PIXEL_BORDER    = 2;

// ── Apply dark background to an object ──────────────────
inline void apply_dark_bg(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(BG_PRIMARY), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

// ── Shared keyboard/trackball focus treatment ───────────
inline void apply_focus_style(lv_obj_t* obj) {
    lv_obj_set_style_border_color(obj, lv_color_hex(ACCENT_YELLOW), LV_STATE_FOCUSED);
}

// ── Pixel card style (0-radius, dark bg, 2px border) ────
inline void apply_pixel_card(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, PIXEL_BORDER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
    apply_focus_style(obj);
}

// ── Pixel card with accent border ───────────────────────
inline void apply_pixel_card_accent(lv_obj_t* obj) {
    apply_pixel_card(obj);
    lv_obj_set_style_border_color(obj, lv_color_hex(ACCENT), 0);
}

// ── Pixel button (filled) ───────────────────────────────
inline void apply_pixel_btn(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, PIXEL_BORDER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(ACCENT_HOVER), 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
    apply_focus_style(obj);
}

// ── Pixel button (outline) ──────────────────────────────
inline void apply_pixel_btn_outline(lv_obj_t* obj) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, PIXEL_BORDER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
    apply_focus_style(obj);
}

// ── Pixel input field ───────────────────────────────────
inline void apply_pixel_input(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(BG_INPUT), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, PIXEL_BORDER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
    apply_focus_style(obj);
}

// ── Pixel badge (small accent label) ────────────────────
inline void apply_pixel_badge(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_pad_all(obj, 2, 0);
}

// ── Legacy card style (kept for compatibility) ──────────
// Use apply_pixel_card() for new code.
inline void apply_card_style(lv_obj_t* obj) {
    apply_pixel_card(obj);
}

} // namespace slopos::theme
