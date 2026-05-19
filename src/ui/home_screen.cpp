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
#include "../mesh/mesh_wrapper.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>

namespace slopos::ui {

using namespace theme;

static lv_obj_t* scr           = nullptr;
static lv_obj_t* top_bar       = nullptr;
static lv_obj_t* bottom_bar    = nullptr;
static lv_obj_t* grid          = nullptr;
static lv_obj_t* time_label    = nullptr;
static lv_obj_t* batt_label    = nullptr;
static lv_obj_t* signal_label  = nullptr;
static lv_obj_t* hashtag_label = nullptr;

static constexpr int GRID_COLS  = 3;
static constexpr int GRID_ROWS  = 4;
static constexpr int TOP_BAR_H  = 22;
static constexpr int BOT_BAR_H  = 20;
static constexpr int GRID_PAD   = 3;
static constexpr int DIVIDER_H  = 1;

struct IconDef {
    const char* label;
    const char* symbol;
    bool        badge;
    Screen      target;
};

static const IconDef icons[] = {
    {"CHATS",     LV_SYMBOL_ENVELOPE,   true,  Screen::Chat},
    {"CONTACTS",  LV_SYMBOL_CALL,       false, Screen::Contacts},
    {"REPEATERS", LV_SYMBOL_WIFI,       false, Screen::Channels},
    {"FINDER",    LV_SYMBOL_EYE_OPEN,   false, Screen::Network},
    {"HEARD",     LV_SYMBOL_VOLUME_MID, false, Screen::Heard},
    {"MAP",       LV_SYMBOL_GPS,        false, Screen::Map},
    {"ADVERTISE", LV_SYMBOL_AUDIO,      false, Screen::Advertise},
    {"SETTINGS",  LV_SYMBOL_SETTINGS,   false, Screen::Settings},
    {"TRACE",     LV_SYMBOL_SHUFFLE,    false, Screen::Trace},
    {"TERMINAL",  LV_SYMBOL_KEYBOARD,   false, Screen::Terminal},
    {"NOISE",     LV_SYMBOL_VOLUME_MAX, false, Screen::Noise},
    {"SIGNAL",    LV_SYMBOL_BARS,       false, Screen::Signal},
};

static void on_icon_click(lv_event_t* e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < (int)(sizeof(icons)/sizeof(icons[0])))
        navigate_to(icons[idx].target);
}

// ── Build dynamic channel hashtag string ────────────────
static void build_channel_string(char* buf, size_t sz)
{
    char names[8][32];
    int n = slopos::mesh::exportChannels(names, 8);
    if (n == 0) { strncpy(buf, "no channels", sz); return; }
    int pos = 0;
    for (int i = 0; i < n && pos < (int)sz - 20; i++) {
        const char* nm = names[i];
        pos += snprintf(buf + pos, sz - pos, "%s%s*  ",
                        nm[0] == '#' ? "" : "#", nm);
    }
    while (pos > 0 && buf[pos-1] == ' ') buf[--pos] = '\0';
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

    // ≡ hamburger using LVGL symbol font (reliable on all builds)
    lv_obj_t* menu_icon = lv_label_create(top_bar);
    lv_label_set_text(menu_icon, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(menu_icon, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_align(menu_icon, LV_ALIGN_LEFT_MID, 4, 0);

    // Dynamic channel hashtags
    char ch_buf[120];
    build_channel_string(ch_buf, sizeof(ch_buf));
    hashtag_label = lv_label_create(top_bar);
    lv_label_set_text(hashtag_label, ch_buf);
    lv_label_set_long_mode(hashtag_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(hashtag_label, 260);
    lv_obj_set_style_text_color(hashtag_label, lv_color_hex(CHANNEL_HASH), 0);
    lv_obj_set_style_text_font(hashtag_label, &lv_font_montserrat_12, 0);
    lv_obj_align(hashtag_label, LV_ALIGN_LEFT_MID, 26, 0);

    // Time (far right)
    time_label = lv_label_create(top_bar);
    lv_label_set_text(time_label, "--:--");
    lv_obj_set_style_text_color(time_label, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -4, 0);

    // Divider
    lv_obj_t* div = lv_obj_create(scr);
    lv_obj_set_size(div, LV_PCT(100), DIVIDER_H);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, TOP_BAR_H);
    lv_obj_set_style_bg_color(div, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
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
    lv_label_set_text(dev, slopos::mesh::getOwnName());
    lv_obj_set_style_text_color(dev, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(dev, &lv_font_montserrat_12, 0);
    lv_obj_align(dev, LV_ALIGN_LEFT_MID, 4, 0);

    signal_label = lv_label_create(bottom_bar);
    lv_label_set_text(signal_label, "▂▄▆█");
    lv_obj_set_style_text_color(signal_label, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(signal_label, &lv_font_montserrat_12, 0);
    lv_obj_align(signal_label, LV_ALIGN_CENTER, -20, 0);

    batt_label = lv_label_create(bottom_bar);
    lv_label_set_text(batt_label, "--%");
    lv_obj_set_style_text_color(batt_label, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(batt_label, &lv_font_montserrat_12, 0);
    lv_obj_align(batt_label, LV_ALIGN_RIGHT_MID, -4, 0);

    // Divider
    lv_obj_t* div = lv_obj_create(scr);
    lv_obj_set_size(div, LV_PCT(100), DIVIDER_H);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, TFT_HEIGHT - BOT_BAR_H - DIVIDER_H);
    lv_obj_set_style_bg_color(div, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
}

// ── Icon tile ────────────────────────────────────────────
static lv_obj_t* create_icon_tile(lv_obj_t* parent, const IconDef& icon, int idx)
{
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_style_bg_color(tile, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tile, 0, 0);
    lv_obj_set_style_border_width(tile, 2, 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_pad_all(tile, 4, 0);

    // Pressed state: lighter bg + cyan border
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x2a2a2a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(tile, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(tile, lv_color_hex(ACCENT), LV_STATE_PRESSED);

    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, on_icon_click, LV_EVENT_CLICKED, (void*)(intptr_t)idx);

    // Icon symbol in cyan
    lv_obj_t* icon_label = lv_label_create(tile);
    lv_label_set_text(icon_label, icon.symbol);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(ACCENT), 0);
    lv_obj_align(icon_label, LV_ALIGN_CENTER, 0, -6);

    // Uppercase label in white below icon
    lv_obj_t* label = lv_label_create(tile);
    lv_label_set_text(label, icon.label);
    lv_obj_set_style_text_color(label, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -2);

    if (icon.badge) {
        lv_obj_t* badge = lv_obj_create(tile);
        lv_obj_set_size(badge, 10, 10);
        lv_obj_set_style_bg_color(badge, lv_color_hex(ACCENT_RED), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, 0, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -4, 4);
    }

    return tile;
}

// ── 3×4 icon grid ────────────────────────────────────────
static void create_icon_grid()
{
    int grid_h = TFT_HEIGHT - TOP_BAR_H - DIVIDER_H - BOT_BAR_H - DIVIDER_H;

    grid = lv_obj_create(scr);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, GRID_PAD, 0);
    lv_obj_set_size(grid, LV_PCT(100), grid_h);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, TOP_BAR_H + DIVIDER_H);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(grid, LV_DIR_NONE);

    int gw = TFT_WIDTH  - (GRID_PAD * 2) - (GRID_PAD * (GRID_COLS - 1));
    int gh = grid_h     - (GRID_PAD * 2) - (GRID_PAD * (GRID_ROWS - 1));
    int tw = gw / GRID_COLS;
    int th = gh / GRID_ROWS;

    for (int i = 0; i < GRID_COLS * GRID_ROWS; i++) {
        lv_obj_t* tile = create_icon_tile(grid, icons[i], i);
        lv_obj_set_size(tile, tw, th);
    }
}

// ── Shared build helper ──────────────────────────────────
static void build_home_screen(lv_scr_load_anim_t anim, uint32_t duration)
{
    // Reset dangling pointers before rebuilding
    hashtag_label = nullptr;
    time_label    = nullptr;
    batt_label    = nullptr;
    signal_label  = nullptr;

    scr = lv_obj_create(nullptr);
    apply_dark_bg(scr);

    // When any other screen replaces Home (auto_del=true), LVGL frees all
    // children — null the static pointers so periodic update functions
    // in ui::loop() don't dereference freed objects.
    lv_obj_add_event_cb(scr, [](lv_event_t*) {
        scr = top_bar = bottom_bar = grid = nullptr;
        time_label = batt_label = signal_label = hashtag_label = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    create_top_bar();
    create_bottom_bar();
    create_icon_grid();
    lv_scr_load_anim(scr, anim, duration, 0, true);
}

// ── Public API ───────────────────────────────────────────
void home_screen_create()
{
    build_home_screen(LV_SCR_LOAD_ANIM_FADE_ON, 300);
}

void home_screen_show()
{
    build_home_screen(LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200);
}

void home_screen_update_battery(int pct)
{
    if (!batt_label) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(batt_label, buf);
    lv_obj_set_style_text_color(batt_label,
        pct > 20 ? lv_color_hex(ACCENT) : lv_color_hex(ACCENT_RED), 0);
}

void home_screen_update_time(const char* time_str)
{
    if (!time_label) return;
    lv_label_set_text(time_label, time_str);
}

void home_screen_update_signal(int rssi)
{
    if (!signal_label) return;
    const char* bars;
    if (rssi > -70)       bars = "▂▄▆█";
    else if (rssi > -85)  bars = "▂▄▆ ";
    else if (rssi > -100) bars = "▂▄  ";
    else if (rssi > -115) bars = "▂   ";
    else                  bars = "    ";
    lv_label_set_text(signal_label, bars);
}

void home_screen_update_channels()
{
    if (!hashtag_label) return;
    char buf[120];
    build_channel_string(buf, sizeof(buf));
    lv_label_set_text(hashtag_label, buf);
}

} // namespace slopos::ui
