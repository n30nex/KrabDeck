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
#include "../mesh/mesh_wrapper.h"
#include <lvgl.h>
#include <cstring>

namespace slopos::ui {

using namespace theme;

static lv_obj_t* scr = nullptr;
static lv_obj_t* top_bar = nullptr;
static lv_obj_t* msg_list = nullptr;
static lv_obj_t* input_bar = nullptr;
static lv_obj_t* input_field = nullptr;
static lv_obj_t* channel_label = nullptr;

static constexpr int TOP_H    = 24;
static constexpr int INPUT_H  = 34;
static constexpr int BUBBLE_PAD = 6;
static constexpr int MAX_VISIBLE_MSGS = 50;

// ── Channel tabs at top ─────────────────────────────────
static const char* channels[] = {
    "#hertford*", "#london*", "#Jokez", "#general",
    "DM: Alice",  "DM: Bob"
};
static constexpr int NUM_CHANNELS = 6;
static int active_channel = 0;

// ── Top bar ─────────────────────────────────────────────
static void create_top_bar()
{
    top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, LV_PCT(100), TOP_H);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);

    // Back button (←) — clickable
    lv_obj_t* back = lv_btn_create(top_bar);
    lv_obj_set_size(back, 24, 20);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(back, 4, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, [](lv_event_t*) { go_back(); }, LV_EVENT_CLICKED, nullptr);

    // Channel name
    channel_label = lv_label_create(top_bar);
    lv_label_set_text(channel_label, channels[active_channel]);
    lv_obj_set_style_text_color(channel_label, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(channel_label, &lv_font_montserrat_14, 0);
    lv_obj_align(channel_label, LV_ALIGN_CENTER, 0, 0);

    // < prev channel button — clickable
    lv_obj_t* prev = lv_btn_create(top_bar);
    lv_obj_set_size(prev, 22, 18);
    lv_obj_align(prev, LV_ALIGN_LEFT_MID, 110, 0);
    lv_obj_set_style_bg_color(prev, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(prev, 3, 0);
    lv_obj_set_style_border_width(prev, 0, 0);
    lv_obj_t* pl = lv_label_create(prev);
    lv_label_set_text(pl, "<");
    lv_obj_set_style_text_color(pl, lv_color_hex(CHANNEL_HASH), 0);
    lv_obj_set_style_text_font(pl, &lv_font_montserrat_12, 0);
    lv_obj_center(pl);
    lv_obj_add_event_cb(prev, [](lv_event_t*) {
        active_channel = (active_channel - 1 + NUM_CHANNELS) % NUM_CHANNELS;
        lv_label_set_text(channel_label, channels[active_channel]);
    }, LV_EVENT_CLICKED, nullptr);

    // next > channel button — clickable
    lv_obj_t* next = lv_btn_create(top_bar);
    lv_obj_set_size(next, 22, 18);
    lv_obj_align(next, LV_ALIGN_RIGHT_MID, -50, 0);
    lv_obj_set_style_bg_color(next, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(next, 3, 0);
    lv_obj_set_style_border_width(next, 0, 0);
    lv_obj_t* nl = lv_label_create(next);
    lv_label_set_text(nl, ">");
    lv_obj_set_style_text_color(nl, lv_color_hex(CHANNEL_HASH), 0);
    lv_obj_set_style_text_font(nl, &lv_font_montserrat_12, 0);
    lv_obj_center(nl);
    lv_obj_add_event_cb(next, [](lv_event_t*) {
        active_channel = (active_channel + 1) % NUM_CHANNELS;
        lv_label_set_text(channel_label, channels[active_channel]);
    }, LV_EVENT_CLICKED, nullptr);
}

// ── Message bubble ─────────────────────────────────────
static lv_obj_t* create_bubble(lv_obj_t* parent, const char* sender,
                                const char* text, bool is_self)
{
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_width(container, LV_PCT(100));
    lv_obj_set_height(container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, BUBBLE_PAD / 2, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);

    // Sender name
    lv_obj_t* name = lv_label_create(container);
    lv_label_set_text(name, sender);
    lv_obj_set_style_text_color(name, is_self
        ? lv_color_hex(ACCENT) : lv_color_hex(ACCENT_GREEN), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);

    // Message bubble
    lv_obj_t* bubble = lv_obj_create(container);
    lv_obj_set_width(bubble, LV_PCT(85));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_pad_all(bubble, 6, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);

    if (is_self) {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(ACCENT), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_30, 0);
        lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
    } else {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(BG_TERTIARY), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
    }

    lv_obj_t* msg_text = lv_label_create(bubble);
    lv_label_set_text(msg_text, text);
    lv_obj_set_style_text_color(msg_text, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(msg_text, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(msg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_text, LV_PCT(100));

    return container;
}

// ── Message list ────────────────────────────────────────
static void create_message_list()
{
    msg_list = lv_obj_create(scr);
    lv_obj_set_size(msg_list, LV_PCT(100),
                    TFT_HEIGHT - TOP_H - INPUT_H);
    lv_obj_align(msg_list, LV_ALIGN_TOP_MID, 0, TOP_H);
    lv_obj_set_style_bg_opa(msg_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(msg_list, 0, 0);
    lv_obj_set_style_pad_all(msg_list, 2, 0);
    lv_obj_set_flex_flow(msg_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(msg_list, LV_DIR_VER);
}

// ── Send helper ──────────────────────────────────────────
static void do_send()
{
    const char* text = lv_textarea_get_text(input_field);
    if (!text || !text[0]) return;

    const char* chan = channels[active_channel];
    bool is_dm = (strncmp(chan, "DM: ", 4) == 0);
    const char* dest = is_dm ? (chan + 4) : chan;

    bool ok;
    if (is_dm) {
        ok = slopos::mesh::sendMessage(dest, text);
    } else {
        ok = slopos::mesh::sendChannelMessage(dest, text);
    }

    // Echo sent message locally
    create_bubble(msg_list, slopos::mesh::getOwnName(), text, true);
    lv_textarea_set_text(input_field, "");

    // Scroll to bottom
    lv_obj_t* last = lv_obj_get_child(msg_list,
                        lv_obj_get_child_cnt(msg_list) - 1);
    if (last) lv_obj_scroll_to_view(last, LV_ANIM_ON);
}

// ── Input bar ───────────────────────────────────────────
static void create_input_bar()
{
    input_bar = lv_obj_create(scr);
    lv_obj_set_size(input_bar, LV_PCT(100), INPUT_H);
    lv_obj_align(input_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(input_bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(input_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(input_bar, 3, 0);
    lv_obj_set_style_border_width(input_bar, 0, 0);

    // Text input
    input_field = lv_textarea_create(input_bar);
    lv_obj_set_size(input_field, LV_PCT(82), INPUT_H - 6);
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
    lv_textarea_set_placeholder_text(input_field, "Type a message...");

    // Send button
    lv_obj_t* send_btn = lv_btn_create(input_bar);
    lv_obj_set_size(send_btn, LV_PCT(15), INPUT_H - 6);
    lv_obj_align(send_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(send_btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_bg_opa(send_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(send_btn, 6, 0);
    lv_obj_set_style_border_width(send_btn, 0, 0);

    lv_obj_t* send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, LV_SYMBOL_UPLOAD);
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_16, 0);
    lv_obj_center(send_label);

    // Wire send button
    lv_obj_add_event_cb(send_btn, [](lv_event_t*) {
        do_send();
    }, LV_EVENT_CLICKED, nullptr);

    // Wire Enter key on input field
    lv_obj_add_event_cb(input_field, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_READY) {
            do_send();
        }
    }, LV_EVENT_ALL, nullptr);
}

// ── Public API ──────────────────────────────────────────
void chat_screen_show()
{
    // Previous screen was auto-deleted by LVGL when we navigated away
    scr = nullptr;
    scr = lv_obj_create(nullptr);
    apply_dark_bg(scr);

    create_top_bar();
    create_message_list();
    create_input_bar();

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, true);
}

void chat_screen_add_msg(const char* sender, const char* text, bool is_self)
{
    if (!msg_list) return;
    create_bubble(msg_list, sender, text, is_self);

    // Keep message list trimmed
    if (lv_obj_get_child_cnt(msg_list) > MAX_VISIBLE_MSGS) {
        lv_obj_del(lv_obj_get_child(msg_list, 0));
    }

    // Scroll to bottom
    lv_obj_t* last = lv_obj_get_child(msg_list,
                        lv_obj_get_child_cnt(msg_list) - 1);
    if (last) lv_obj_scroll_to_view(last, LV_ANIM_ON);
}

} // namespace slopos::ui
