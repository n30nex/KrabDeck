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


#include "chat_screen.h"
#include "navigation.h"
#include "theme.h"
#include "../hal/tdeck_pins.h"
#include "../hal/battery.h"
#include "../mesh/mesh_wrapper.h"
#include <lvgl.h>
#include <cstring>
#include <cstdio>

namespace slopos::ui {

using namespace theme;

// ── Messaging-view widgets ─────────────────────────────────
static lv_obj_t* scr            = nullptr;
static lv_obj_t* top_bar        = nullptr;
static lv_obj_t* channel_ribbon = nullptr;
static lv_obj_t* msg_list       = nullptr;
static lv_obj_t* input_bar      = nullptr;
static lv_obj_t* input_field    = nullptr;

// ── Messaging-view layout ──────────────────────────────────
static constexpr int TOP_H      = 24;
static constexpr int BOT_BAR_H  = 20;
static constexpr int DIVIDER_H  = 1;
static constexpr int INPUT_H    = 36;
static constexpr int BUBBLE_PAD = 6;
static constexpr int MAX_MSGS   = 50;
static constexpr int MSG_LIST_Y = TOP_H + DIVIDER_H;
static constexpr int MSG_LIST_H = TFT_HEIGHT - TOP_H - DIVIDER_H - INPUT_H - DIVIDER_H - BOT_BAR_H;

// ── Channel-list layout (matches screens.cpp constants) ────
static constexpr int LIST_BAR_H  = 22;
static constexpr int LIST_DIV_H  = 1;
static constexpr int LIST_CONT_Y = LIST_BAR_H + LIST_DIV_H;   // 23
static constexpr int LIST_CONT_H = TFT_HEIGHT - LIST_CONT_Y - LIST_DIV_H - BOT_BAR_H; // 196
static constexpr int LIST_ROW_H  = 44;

// ── Channel state ──────────────────────────────────────────
static constexpr int MAX_CHANNELS = 16;
static char  dyn_channels[MAX_CHANNELS][32];
static int   dyn_count      = 0;
static int   active_channel = 0;

// ── Per-channel metadata ───────────────────────────────────
struct ChannelMeta {
    char     preview[64];
    uint32_t timestamp;
    int      unread;
};
static ChannelMeta ch_meta[MAX_CHANNELS];

// ── Forward declarations ───────────────────────────────────
static void show_channel_list(lv_scr_load_anim_t anim);
static void open_channel_messaging(int idx);
static void rebuild_channel_ribbon();

// ════════════════════════════════════════════════════
// Dynamic channels — pulled from mesh, sorted by MRU
// ════════════════════════════════════════════════════
static void refresh_channels()
{
    dyn_count = slopos::mesh::exportChannels(dyn_channels, MAX_CHANNELS);
    if (active_channel >= dyn_count) active_channel = 0;
    if (dyn_count == 0) {
        strncpy(dyn_channels[0], "#general", 31);
        dyn_count = 1;
    }
    memset(ch_meta, 0, sizeof(ch_meta));
}

static void mark_channel_used(int idx)
{
    if (idx < 0 || idx >= dyn_count || idx == 0) return;
    char tmp[32];
    strncpy(tmp, dyn_channels[idx], 31);
    for (int i = idx; i > 0; i--)
        strncpy(dyn_channels[i], dyn_channels[i-1], 31);
    strncpy(dyn_channels[0], tmp, 31);
    active_channel = 0;
}

// ════════════════════════════════════════════════════
// Timestamp helper
// ════════════════════════════════════════════════════
static void format_time(char* buf, size_t sz, uint32_t epoch)
{
    if (epoch == 0) { snprintf(buf, sz, "--:--"); return; }
    uint32_t t = epoch % 86400;
    snprintf(buf, sz, "%02d:%02d", (t / 3600) % 24, (t / 60) % 60);
}

// ════════════════════════════════════════════════════
// Channel-list screen builder (mirrors make_screen_full)
// ════════════════════════════════════════════════════
static lv_obj_t* make_chat_list_screen()
{
    lv_obj_t* s = lv_obj_create(nullptr);
    apply_dark_bg(s);

    // Top bar
    lv_obj_t* top = lv_obj_create(s);
    lv_obj_set_size(top, LV_PCT(100), LIST_BAR_H);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_style_border_width(top, 0, 0);

    // ≡ hamburger → go back to Home
    lv_obj_t* menu = lv_label_create(top);
    lv_label_set_text(menu, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(menu, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_align(menu, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_add_flag(menu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(menu, [](lv_event_t*) { go_back(); }, LV_EVENT_CLICKED, nullptr);

    // Channel hashtags snapshot
    char ch_buf[100] = "";
    {
        char names[8][32];
        int n = slopos::mesh::exportChannels(names, 8);
        int pos = 0;
        for (int i = 0; i < n && pos < 90; i++) {
            const char* nm = names[i];
            pos += snprintf(ch_buf + pos, sizeof(ch_buf) - pos,
                            "%s%s*  ", nm[0] == '#' ? "" : "#", nm);
        }
        while (pos > 0 && ch_buf[pos-1] == ' ') ch_buf[--pos] = '\0';
        if (ch_buf[0] == '\0') snprintf(ch_buf, sizeof(ch_buf), "no channels");
    }
    lv_obj_t* ch_lbl = lv_label_create(top);
    lv_label_set_text(ch_lbl, ch_buf);
    lv_obj_set_style_text_color(ch_lbl, lv_color_hex(CHANNEL_HASH), 0);
    lv_obj_set_style_text_font(ch_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(ch_lbl, LV_ALIGN_LEFT_MID, 22, 0);

    // Time snapshot
    {
        uint32_t epoch = slopos::mesh::getCurrentTime();
        char t[8];
        if (epoch == 0) snprintf(t, sizeof(t), "--:--");
        else {
            uint32_t sec = epoch % 86400;
            snprintf(t, sizeof(t), "%02d:%02d", (sec/3600)%24, (sec/60)%60);
        }
        lv_obj_t* tl = lv_label_create(top);
        lv_label_set_text(tl, t);
        lv_obj_set_style_text_color(tl, lv_color_hex(TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);
        lv_obj_align(tl, LV_ALIGN_RIGHT_MID, -4, 0);
    }

    // Top divider
    lv_obj_t* tdiv = lv_obj_create(s);
    lv_obj_set_size(tdiv, LV_PCT(100), LIST_DIV_H);
    lv_obj_align(tdiv, LV_ALIGN_TOP_MID, 0, LIST_BAR_H);
    lv_obj_set_style_bg_color(tdiv, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_bg_opa(tdiv, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tdiv, 0, 0);

    // Bottom bar
    lv_obj_t* bot = lv_obj_create(s);
    lv_obj_set_size(bot, LV_PCT(100), BOT_BAR_H);
    lv_obj_align(bot, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bot, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(bot, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bot, 0, 0);
    lv_obj_set_style_border_width(bot, 0, 0);

    lv_obj_t* dev = lv_label_create(bot);
    lv_label_set_text(dev, slopos::mesh::getOwnName());
    lv_obj_set_style_text_color(dev, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(dev, &lv_font_montserrat_12, 0);
    lv_obj_align(dev, LV_ALIGN_LEFT_MID, 4, 0);

    {
        int rssi = slopos::mesh::getLastRSSI();
        const char* bars = rssi > -70  ? "▂▄▆█" :
                           rssi > -85  ? "▂▄▆ " :
                           rssi > -100 ? "▂▄  " :
                           rssi > -115 ? "▂   " : "    ";
        lv_obj_t* sig = lv_label_create(bot);
        lv_label_set_text(sig, bars);
        lv_obj_set_style_text_color(sig, lv_color_hex(ACCENT), 0);
        lv_obj_set_style_text_font(sig, &lv_font_montserrat_12, 0);
        lv_obj_align(sig, LV_ALIGN_CENTER, -20, 0);
    }

    {
        char batt[8];
        int pct = slopos_battery_pct();
        snprintf(batt, sizeof(batt), "%d%%", pct);
        lv_obj_t* bl = lv_label_create(bot);
        lv_label_set_text(bl, batt);
        lv_obj_set_style_text_color(bl, lv_color_hex(pct > 20 ? ACCENT : ACCENT_RED), 0);
        lv_obj_set_style_text_font(bl, &lv_font_montserrat_12, 0);
        lv_obj_align(bl, LV_ALIGN_RIGHT_MID, -4, 0);
    }

    // Bottom divider
    lv_obj_t* bdiv = lv_obj_create(s);
    lv_obj_set_size(bdiv, LV_PCT(100), LIST_DIV_H);
    lv_obj_align(bdiv, LV_ALIGN_TOP_MID, 0, TFT_HEIGHT - BOT_BAR_H - LIST_DIV_H);
    lv_obj_set_style_bg_color(bdiv, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_bg_opa(bdiv, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bdiv, 0, 0);

    return s;
}

// ════════════════════════════════════════════════════
// Channel-list view
// ════════════════════════════════════════════════════
static void show_channel_list(lv_scr_load_anim_t anim)
{
    // Null messaging-view pointers — they're invalid once we leave
    scr = top_bar = channel_ribbon = msg_list = input_bar = input_field = nullptr;

    refresh_channels();

    lv_obj_t* s = make_chat_list_screen();

    // Scrollable list container
    lv_obj_t* list = lv_obj_create(s);
    lv_obj_set_size(list, LV_PCT(100), LIST_CONT_H);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, LIST_CONT_Y);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < dyn_count; i++) {
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, LV_PCT(100), LIST_ROW_H);
        lv_obj_set_style_bg_color(row,
            lv_color_hex(i % 2 == 0 ? BG_TERTIARY : BG_INPUT), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        // Blue circle avatar (Discord #5865F2)
        lv_obj_t* avatar = lv_obj_create(row);
        lv_obj_set_size(avatar, 32, 32);
        lv_obj_align(avatar, LV_ALIGN_LEFT_MID, 6, 0);
        lv_obj_set_style_bg_color(avatar, lv_color_hex(0x5865F2), 0);
        lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(avatar, 16, 0);
        lv_obj_set_style_border_width(avatar, 0, 0);
        lv_obj_clear_flag(avatar, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* hash = lv_label_create(avatar);
        lv_label_set_text(hash, "#");
        lv_obj_set_style_text_color(hash, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(hash, &lv_font_montserrat_14, 0);
        lv_obj_center(hash);

        // Channel name (top-left of text area)
        lv_obj_t* name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, dyn_channels[i]);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 46, 6);

        // Timestamp (top-right, shifted left when badge is present)
        if (ch_meta[i].timestamp > 0) {
            char tbuf[8];
            format_time(tbuf, sizeof(tbuf), ch_meta[i].timestamp);
            lv_obj_t* ts = lv_label_create(row);
            lv_label_set_text(ts, tbuf);
            lv_obj_set_style_text_color(ts, lv_color_hex(TEXT_MUTED), 0);
            lv_obj_set_style_text_font(ts, &lv_font_montserrat_12, 0);
            lv_obj_align(ts, LV_ALIGN_TOP_RIGHT,
                ch_meta[i].unread > 0 ? -26 : -4, 8);
        }

        // Preview text (bottom-left of text area)
        lv_obj_t* prev = lv_label_create(row);
        lv_label_set_text(prev,
            ch_meta[i].preview[0] ? ch_meta[i].preview : "No messages yet");
        lv_obj_set_style_text_color(prev, lv_color_hex(TEXT_SECONDARY), 0);
        lv_obj_set_style_text_font(prev, &lv_font_montserrat_12, 0);
        lv_label_set_long_mode(prev, LV_LABEL_LONG_DOT);
        lv_obj_set_width(prev, 200);
        lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 46, 26);

        // Unread badge (cyan circle, right edge)
        if (ch_meta[i].unread > 0) {
            lv_obj_t* badge = lv_obj_create(row);
            lv_obj_set_size(badge, 18, 18);
            lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -4, 0);
            lv_obj_set_style_bg_color(badge, lv_color_hex(ACCENT), 0);
            lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(badge, 9, 0);
            lv_obj_set_style_border_width(badge, 0, 0);
            lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);

            char cnt_buf[4];
            int cnt = ch_meta[i].unread;
            if (cnt > 9) snprintf(cnt_buf, sizeof(cnt_buf), "9+");
            else         snprintf(cnt_buf, sizeof(cnt_buf), "%d", cnt);
            lv_obj_t* cnt_lbl = lv_label_create(badge);
            lv_label_set_text(cnt_lbl, cnt_buf);
            lv_obj_set_style_text_color(cnt_lbl, lv_color_hex(0xffffff), 0);
            lv_obj_set_style_text_font(cnt_lbl, &lv_font_montserrat_12, 0);
            lv_obj_center(cnt_lbl);
        }

        // Tap: open messaging view for this channel
        int ch_idx = i;
        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            int idx = (int)(intptr_t)lv_event_get_user_data(e);
            ch_meta[idx].unread = 0;
            open_channel_messaging(idx);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)ch_idx);
    }

    lv_scr_load_anim(s, anim, 200, 0, true);
}

// ════════════════════════════════════════════════════
// Messaging view — channel ribbon rebuild
// ════════════════════════════════════════════════════
static void rebuild_channel_ribbon()
{
    if (!channel_ribbon) return;
    lv_obj_clean(channel_ribbon);

    for (int i = 0; i < dyn_count; i++) {
        lv_obj_t* pill = lv_btn_create(channel_ribbon);
        lv_obj_set_height(pill, TOP_H - 8);
        lv_obj_set_style_radius(pill, 10, 0);
        lv_obj_set_style_border_width(pill, 0, 0);
        lv_obj_set_style_pad_hor(pill, 8, 0);
        lv_obj_set_style_pad_ver(pill, 2, 0);

        if (i == active_channel) {
            lv_obj_set_style_bg_color(pill, lv_color_hex(ACCENT), 0);
            lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(pill, lv_color_hex(BG_TERTIARY), 0);
            lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
        }

        lv_obj_t* pill_label = lv_label_create(pill);
        lv_label_set_text(pill_label, dyn_channels[i]);
        lv_obj_set_style_text_font(pill_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(pill_label,
            i == active_channel ? lv_color_hex(0xffffff) : lv_color_hex(CHANNEL_HASH), 0);
        lv_obj_center(pill_label);

        int idx = i;
        lv_obj_add_event_cb(pill, [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            active_channel = ch;
            rebuild_channel_ribbon();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
    }
}

// ════════════════════════════════════════════════════
// Messaging view — top bar (← back + channel pills)
// ════════════════════════════════════════════════════
static void create_top_bar()
{
    top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, LV_PCT(100), TOP_H);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);

    // ← back button → return to channel list
    lv_obj_t* back = lv_btn_create(top_bar);
    lv_obj_set_size(back, 24, TOP_H - 4);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(back, 4, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, [](lv_event_t*) {
        show_channel_list(LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }, LV_EVENT_CLICKED, nullptr);

    // Horizontal scrollable channel ribbon
    channel_ribbon = lv_obj_create(top_bar);
    lv_obj_set_size(channel_ribbon, LV_PCT(78), TOP_H - 4);
    lv_obj_align(channel_ribbon, LV_ALIGN_LEFT_MID, 28, 0);
    lv_obj_set_style_bg_opa(channel_ribbon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(channel_ribbon, 0, 0);
    lv_obj_set_style_pad_all(channel_ribbon, 0, 0);
    lv_obj_set_flex_flow(channel_ribbon, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(channel_ribbon, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(channel_ribbon, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(channel_ribbon, LV_SCROLLBAR_MODE_OFF);

    // 24h time (right side)
    {
        uint32_t epoch = slopos::mesh::getCurrentTime();
        char t[8];
        if (epoch == 0) snprintf(t, sizeof(t), "--:--");
        else {
            uint32_t sec = epoch % 86400;
            snprintf(t, sizeof(t), "%02d:%02d", (sec/3600)%24, (sec/60)%60);
        }
        lv_obj_t* tl = lv_label_create(top_bar);
        lv_label_set_text(tl, t);
        lv_obj_set_style_text_color(tl, lv_color_hex(TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);
        lv_obj_align(tl, LV_ALIGN_RIGHT_MID, -4, 0);
    }

    for (int i = 0; i < dyn_count; i++) {
        lv_obj_t* pill = lv_btn_create(channel_ribbon);
        lv_obj_set_height(pill, TOP_H - 8);
        lv_obj_set_style_radius(pill, 10, 0);
        lv_obj_set_style_border_width(pill, 0, 0);
        lv_obj_set_style_pad_hor(pill, 8, 0);
        lv_obj_set_style_pad_ver(pill, 2, 0);

        if (i == active_channel) {
            lv_obj_set_style_bg_color(pill, lv_color_hex(ACCENT), 0);
            lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(pill, lv_color_hex(BG_TERTIARY), 0);
            lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
        }

        lv_obj_t* pill_label = lv_label_create(pill);
        lv_label_set_text(pill_label, dyn_channels[i]);
        lv_obj_set_style_text_font(pill_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(pill_label,
            i == active_channel ? lv_color_hex(0xffffff) : lv_color_hex(CHANNEL_HASH), 0);
        lv_obj_center(pill_label);

        int idx = i;
        lv_obj_add_event_cb(pill, [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            active_channel = ch;
            rebuild_channel_ribbon();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
    }

    // Divider
    lv_obj_t* div = lv_obj_create(scr);
    lv_obj_set_size(div, LV_PCT(100), DIVIDER_H);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, TOP_H);
    lv_obj_set_style_bg_color(div, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
}

// ════════════════════════════════════════════════════
// Message bubble — Discord style
// ════════════════════════════════════════════════════
static lv_obj_t* create_bubble(lv_obj_t* parent, const char* sender,
                                const char* text, uint32_t timestamp,
                                bool is_self)
{
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_width(container, LV_PCT(100));
    lv_obj_set_height(container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, BUBBLE_PAD / 2, 0);

    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    if (is_self) {
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    }

    lv_obj_t* bubble = lv_obj_create(container);
    lv_obj_set_width(bubble, LV_PCT(78));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_pad_all(bubble, 6, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);

    if (is_self) {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(ACCENT), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(MSG_INCOMING), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    }

    // Sender name + timestamp row
    lv_obj_t* header = lv_obj_create(bubble);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* name = lv_label_create(header);
    lv_label_set_text(name, sender);
    lv_obj_set_style_text_color(name,
        is_self ? lv_color_hex(0xffffff) : lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);

    char time_buf[8];
    format_time(time_buf, sizeof(time_buf), timestamp);
    lv_obj_t* ts = lv_label_create(header);
    lv_label_set_text(ts, time_buf);
    lv_obj_set_style_text_color(ts,
        is_self ? lv_color_hex(0xb0d4ff) : lv_color_hex(TEXT_MUTED), 0);
    lv_obj_set_style_text_font(ts, &lv_font_montserrat_12, 0);

    lv_obj_t* msg_text = lv_label_create(bubble);
    lv_label_set_text(msg_text, text);
    lv_obj_set_style_text_color(msg_text,
        is_self ? lv_color_hex(0xffffff) : lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(msg_text, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(msg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_text, LV_PCT(100));

    return container;
}

// ════════════════════════════════════════════════════
// Message list (vertical scroll)
// ════════════════════════════════════════════════════
static void create_message_list()
{
    msg_list = lv_obj_create(scr);
    lv_obj_set_size(msg_list, LV_PCT(100), MSG_LIST_H);
    lv_obj_align(msg_list, LV_ALIGN_TOP_MID, 0, MSG_LIST_Y);
    lv_obj_set_style_bg_opa(msg_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(msg_list, 0, 0);
    lv_obj_set_style_pad_all(msg_list, 2, 0);
    lv_obj_set_flex_flow(msg_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(msg_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(msg_list, LV_SCROLLBAR_MODE_OFF);
}

// ════════════════════════════════════════════════════
// Send helper
// ════════════════════════════════════════════════════
static void do_send()
{
    const char* text = lv_textarea_get_text(input_field);
    if (!text || !text[0]) return;

    const char* chan = dyn_channels[active_channel];
    bool is_dm = (strncmp(chan, "DM: ", 4) == 0);
    const char* dest = is_dm ? (chan + 4) : chan;

    if (is_dm) slopos::mesh::sendMessage(dest, text);
    else       slopos::mesh::sendChannelMessage(dest, text);

    mark_channel_used(active_channel);

    uint32_t now = slopos::mesh::getCurrentTime();
    create_bubble(msg_list, slopos::mesh::getOwnName(), text, now, true);
    lv_textarea_set_text(input_field, "");

    lv_obj_t* last = lv_obj_get_child(msg_list, lv_obj_get_child_cnt(msg_list) - 1);
    if (last) lv_obj_scroll_to_view(last, LV_ANIM_ON);
}

// ════════════════════════════════════════════════════
// Input bar
// ════════════════════════════════════════════════════
static void create_input_bar()
{
    int input_y = TFT_HEIGHT - BOT_BAR_H - INPUT_H - DIVIDER_H;

    input_bar = lv_obj_create(scr);
    lv_obj_set_size(input_bar, LV_PCT(100), INPUT_H);
    lv_obj_align(input_bar, LV_ALIGN_TOP_MID, 0, input_y + DIVIDER_H);
    lv_obj_set_style_bg_color(input_bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(input_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(input_bar, 4, 0);
    lv_obj_set_style_border_width(input_bar, 0, 0);

    lv_obj_t* div = lv_obj_create(scr);
    lv_obj_set_size(div, LV_PCT(100), DIVIDER_H);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, input_y);
    lv_obj_set_style_bg_color(div, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);

    input_field = lv_textarea_create(input_bar);
    lv_obj_set_size(input_field, LV_PCT(78), INPUT_H - 8);
    lv_obj_align(input_field, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(input_field, lv_color_hex(BG_INPUT), 0);
    lv_obj_set_style_bg_opa(input_field, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(input_field, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(input_field, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_width(input_field, 1, 0);
    lv_obj_set_style_border_color(input_field, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(input_field, 6, 0);
    lv_obj_set_style_pad_all(input_field, 4, 0);
    lv_textarea_set_one_line(input_field, true);
    lv_textarea_set_placeholder_text(input_field, "Message #channel");

    lv_obj_t* send_btn = lv_btn_create(input_bar);
    lv_obj_set_size(send_btn, 52, INPUT_H - 8);
    lv_obj_align(send_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(send_btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_bg_opa(send_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(send_btn, 6, 0);
    lv_obj_set_style_border_width(send_btn, 0, 0);

    lv_obj_t* send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "Send");
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(send_label, lv_color_hex(0xffffff), 0);
    lv_obj_center(send_label);

    lv_obj_add_event_cb(send_btn, [](lv_event_t*) { do_send(); },
                        LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(input_field, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_READY) do_send();
    }, LV_EVENT_ALL, nullptr);
}

// ════════════════════════════════════════════════════
// Bottom status bar
// ════════════════════════════════════════════════════
static void create_bottom_bar()
{
    lv_obj_t* bot = lv_obj_create(scr);
    lv_obj_set_size(bot, LV_PCT(100), BOT_BAR_H);
    lv_obj_align(bot, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bot, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(bot, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bot, 0, 0);
    lv_obj_set_style_border_width(bot, 0, 0);

    lv_obj_t* dev = lv_label_create(bot);
    lv_label_set_text(dev, slopos::mesh::getOwnName());
    lv_obj_set_style_text_color(dev, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(dev, &lv_font_montserrat_12, 0);
    lv_obj_align(dev, LV_ALIGN_LEFT_MID, 4, 0);

    int rssi = slopos::mesh::getLastRSSI();
    const char* bars = rssi > -70  ? "▂▄▆█" :
                       rssi > -85  ? "▂▄▆ " :
                       rssi > -100 ? "▂▄  " :
                       rssi > -115 ? "▂   " : "    ";
    lv_obj_t* sig = lv_label_create(bot);
    lv_label_set_text(sig, bars);
    lv_obj_set_style_text_color(sig, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(sig, &lv_font_montserrat_12, 0);
    lv_obj_align(sig, LV_ALIGN_CENTER, -20, 0);

    char batt_buf[8];
    int pct = slopos_battery_pct();
    snprintf(batt_buf, sizeof(batt_buf), "%d%%", pct);
    lv_obj_t* bl = lv_label_create(bot);
    lv_label_set_text(bl, batt_buf);
    lv_obj_set_style_text_color(bl, lv_color_hex(pct > 20 ? ACCENT : ACCENT_RED), 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_12, 0);
    lv_obj_align(bl, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_obj_t* div = lv_obj_create(scr);
    lv_obj_set_size(div, LV_PCT(100), DIVIDER_H);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, TFT_HEIGHT - BOT_BAR_H - DIVIDER_H);
    lv_obj_set_style_bg_color(div, lv_color_hex(DIVIDER), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
}

// ════════════════════════════════════════════════════
// Messaging view (opened by tapping a channel)
// ════════════════════════════════════════════════════
static void open_channel_messaging(int idx)
{
    active_channel = idx;

    scr = lv_obj_create(nullptr);
    apply_dark_bg(scr);

    create_top_bar();
    create_message_list();
    create_input_bar();
    create_bottom_bar();

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, true);
}

// ════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════
void chat_screen_show()
{
    show_channel_list(LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

void chat_screen_add_msg(const char* sender, const char* text, bool is_self)
{
    // Update channel metadata regardless of view state
    uint32_t now = slopos::mesh::getCurrentTime();
    if (active_channel < MAX_CHANNELS) {
        strncpy(ch_meta[active_channel].preview, text, 63);
        ch_meta[active_channel].preview[63] = '\0';
        ch_meta[active_channel].timestamp = now;
        // Increment unread only when messaging view is not open
        if (!is_self && !msg_list)
            ch_meta[active_channel].unread++;
    }

    if (!msg_list) return;

    create_bubble(msg_list, sender, text, now, is_self);

    if (lv_obj_get_child_cnt(msg_list) > MAX_MSGS)
        lv_obj_del(lv_obj_get_child(msg_list, 0));

    lv_obj_t* last = lv_obj_get_child(msg_list, lv_obj_get_child_cnt(msg_list) - 1);
    if (last) lv_obj_scroll_to_view(last, LV_ANIM_ON);
}

} // namespace slopos::ui
