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


#include "home_screen.h"
#include "navigation.h"
#include "theme.h"
#include "../hal/tdeck_pins.h"
#include <lvgl.h>
#include <cstdio>

namespace slopos::ui {

using namespace theme;

static lv_obj_t* scr = nullptr;
static lv_obj_t* top_bar = nullptr;
static lv_obj_t* bottom_bar = nullptr;
static lv_obj_t* grid = nullptr;
static lv_obj_t* time_label = nullptr;
static lv_obj_t* batt_label = nullptr;
static lv_obj_t* signal_label = nullptr;

static constexpr int GRID_COLS = 3;
static constexpr int GRID_ROWS = 4;
static constexpr int TOP_BAR_H = 24;
static constexpr int BOT_BAR_H = 20;
static constexpr int GRID_PAD = 4;

struct IconDef {
    const char* label;
    const char* emoji;
    bool badge;
    Screen target;
};

static const IconDef icons[] = {
    {"Chat",       "\xF0\x9F\x92\xAC", true,  Screen::Chat},       // 💬
    {"Contacts",   "\xF0\x9F\x91\xA5", false, Screen::Contacts},   // 👥
    {"Channels",   "\xF0\x9F\x93\xA1", false, Screen::Channels},  // 📡
    {"Network",    "\xF0\x9F\x8C\x90", false, Screen::Network},   // 🌐
    {"Heard",      "\xF0\x9F\x91\x82", false, Screen::Heard},      // 👂
    {"Map",        "\xF0\x9F\x97\xBA", false, Screen::Map},        // 🗺
    {"Advertise",  "\xF0\x9F\x93\xA2", false, Screen::Advertise},  // 📢
    {"Settings",   "\xE2\x9A\x99",      false, Screen::Settings},  // ⚙
    {"Trace",      "\xF0\x9F\x93\x8D", false, Screen::Trace},      // 📍
    {"Terminal",   "\xF0\x9F\x92\xBB", false, Screen::Terminal},   // 💻
    {"Noise",      "\xF0\x9F\x93\x8A", false, Screen::Noise},      // 📊
    {"Signal",     "\xF0\x9F\x93\xB6", false, Screen::Signal},     // 📶
};

// ── Icon click handler ──────────────────────────────────
static void on_icon_click(lv_event_t* e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < (int)(sizeof(icons)/sizeof(icons[0]))) {
        navigate_to(icons[idx].target);
    }
}

// ── Top bar ─────────────────────────────────────────────
static void create_top_bar()
{
    top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, LV_PCT(100), TOP_BAR_H);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);

    lv_obj_t* menu_icon = lv_label_create(top_bar);
    lv_label_set_text(menu_icon, "\xe2\x98\xb0");
    lv_obj_set_style_text_color(menu_icon, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(menu_icon, &lv_font_montserrat_16, 0);
    lv_obj_align(menu_icon, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* hashtags = lv_label_create(top_bar);
    lv_label_set_text(hashtags, "#hertford*  #london*  #Jokez");
    lv_obj_set_style_text_color(hashtags, lv_color_hex(CHANNEL_HASH), 0);
    lv_obj_set_style_text_font(hashtags, &lv_font_montserrat_12, 0);
    lv_obj_align(hashtags, LV_ALIGN_LEFT_MID, 26, 0);

    time_label = lv_label_create(top_bar);
    lv_label_set_text(time_label, "14:03");
    lv_obj_set_style_text_color(time_label, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -4, 0);
}

// ── Bottom bar ──────────────────────────────────────────
static void create_bottom_bar()
{
    bottom_bar = lv_obj_create(scr);
    lv_obj_set_size(bottom_bar, LV_PCT(100), BOT_BAR_H);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bottom_bar, 0, 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);

    lv_obj_t* dev = lv_label_create(bottom_bar);
    lv_label_set_text(dev, "TDeck+");
    lv_obj_set_style_text_color(dev, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(dev, &lv_font_montserrat_12, 0);
    lv_obj_align(dev, LV_ALIGN_LEFT_MID, 4, 0);

    signal_label = lv_label_create(bottom_bar);
    lv_label_set_text(signal_label, "▂▄▆█");
    lv_obj_set_style_text_color(signal_label, lv_color_hex(ACCENT_GREEN), 0);
    lv_obj_set_style_text_font(signal_label, &lv_font_montserrat_12, 0);
    lv_obj_align(signal_label, LV_ALIGN_CENTER, -20, 0);

    batt_label = lv_label_create(bottom_bar);
    lv_label_set_text(batt_label, "85%");
    lv_obj_set_style_text_color(batt_label, lv_color_hex(ACCENT_GREEN), 0);
    lv_obj_set_style_text_font(batt_label, &lv_font_montserrat_12, 0);
    lv_obj_align(batt_label, LV_ALIGN_RIGHT_MID, -4, 0);
}

// ── Create a single icon tile ───────────────────────────
static lv_obj_t* create_icon_tile(lv_obj_t* parent, const IconDef& icon, int idx)
{
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_style_bg_color(tile, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tile, 10, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 4, 0);

    // Make tile clickable
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, on_icon_click, LV_EVENT_CLICKED, (void*)(intptr_t)idx);

    lv_obj_t* emoji = lv_label_create(tile);
    lv_label_set_text(emoji, icon.emoji);
    lv_obj_set_style_text_font(emoji, &lv_font_montserrat_24, 0);
    lv_obj_align(emoji, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t* label = lv_label_create(tile);
    lv_label_set_text(label, icon.label);
    lv_obj_set_style_text_color(label, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -4);

    if (icon.badge) {
        lv_obj_t* badge = lv_obj_create(tile);
        lv_obj_set_size(badge, 10, 10);
        lv_obj_set_style_bg_color(badge, lv_color_hex(ACCENT_RED), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -4, 4);
    }

    return tile;
}

// ── 3x4 Grid ────────────────────────────────────────────
static void create_icon_grid()
{
    grid = lv_obj_create(scr);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, GRID_PAD, 0);
    lv_obj_set_size(grid, LV_PCT(100), TFT_HEIGHT - TOP_BAR_H - BOT_BAR_H);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, TOP_BAR_H);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    int gw = TFT_WIDTH - (GRID_PAD * (GRID_COLS + 1));
    int gh = (TFT_HEIGHT - TOP_BAR_H - BOT_BAR_H) - (GRID_PAD * (GRID_ROWS + 1));
    int tw = gw / GRID_COLS;
    int th = gh / GRID_ROWS;

    for (int i = 0; i < GRID_COLS * GRID_ROWS; i++) {
        lv_obj_t* tile = create_icon_tile(grid, icons[i], i);
        lv_obj_set_size(tile, tw, th);
    }
}

// ── Public ──────────────────────────────────────────────
void home_screen_create()
{
    scr = lv_obj_create(nullptr);
    apply_dark_bg(scr);
    create_top_bar();
    create_bottom_bar();
    create_icon_grid();
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
}

void home_screen_show()
{
    scr = lv_obj_create(nullptr);
    apply_dark_bg(scr);
    create_top_bar();
    create_bottom_bar();
    create_icon_grid();
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, true);
}

void home_screen_update_battery(int pct)
{
    if (!batt_label) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(batt_label, buf);
    lv_obj_set_style_text_color(batt_label,
        pct > 20 ? lv_color_hex(ACCENT_GREEN) : lv_color_hex(ACCENT_RED), 0);
}

void home_screen_update_time(const char* time_str)
{
    if (!time_label) return;
    lv_label_set_text(time_label, time_str);
}

void home_screen_update_signal(int rssi)
{
    if (!signal_label) return;
    // Map RSSI to bars: > -70=4, -70..-85=3, -85..-100=2, -100..-115=1, < -115=0
    const char* bars;
    if (rssi > -70)       bars = "▂▄▆█";
    else if (rssi > -85)  bars = "▂▄▆ ";
    else if (rssi > -100) bars = "▂▄  ";
    else if (rssi > -115) bars = "▂   ";
    else                  bars = "    ";
    lv_label_set_text(signal_label, bars);
}

} // namespace slopos::ui
