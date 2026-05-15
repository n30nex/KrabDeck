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


#include "screens.h"
#include "navigation.h"
#include "theme.h"
#include "../hal/tdeck_pins.h"
#include "../hal/sdcard.h"
#include "../hal/gps.h"
#include "../hal/prefs.h"
#include "../mesh/mesh_wrapper.h"
#include "../app/map_renderer.h"
#include <Arduino.h>
#include <lvgl.h>
#include <cstdio>
#include <cstring>

namespace slopos::ui {

using namespace theme;

// ── Helper: create a screen with back button ────────────
static lv_obj_t* make_screen(const char* title)
{
    lv_obj_t* scr = lv_obj_create(nullptr);
    apply_dark_bg(scr);

    // Top bar
    lv_obj_t* bar = lv_obj_create(scr);
    lv_obj_set_size(bar, LV_PCT(100), 24);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);

    // Back button
    lv_obj_t* back = lv_btn_create(bar);
    lv_obj_set_size(back, 40, 20);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(back, 4, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, [](lv_event_t*) { go_back(); }, LV_EVENT_CLICKED, nullptr);

    // Title
    lv_obj_t* tt = lv_label_create(bar);
    lv_label_set_text(tt, title);
    lv_obj_set_style_text_color(tt, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(tt, &lv_font_montserrat_14, 0);
    lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);

    return scr;
}

static void show_screen(lv_obj_t* scr)
{
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, true);
}

// ════════════════════════════════════════════════════════
// Heard — Master node list with signal bars
// ════════════════════════════════════════════════════════
void heard_screen_show()
{
    lv_obj_t* scr = make_screen("Heard Nodes");

    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 28);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_text_color(list, lv_color_hex(TEXT_PRIMARY), 0);

    slopos::mesh::ContactInfo contacts[32];
    int n = slopos::mesh::exportContactsFull(contacts, 32);

    // Sort by RSSI (strongest first)
    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (contacts[j].rssi > contacts[i].rssi) {
                auto tmp = contacts[i];
                contacts[i] = contacts[j];
                contacts[j] = tmp;
            }

    if (n == 0) {
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_WARNING, "No nodes heard yet");
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    } else {
        char buf[80];
        for (int i = 0; i < n; i++) {
            auto& c = contacts[i];
            // Signal bar: ▁▂▃▄▅▆▇█ based on RSSI
            const char* bars;
            if (c.rssi > -70)       bars = "▆█";
            else if (c.rssi > -85)  bars = "▄▆ ";
            else if (c.rssi > -100) bars = "▂▄ ";
            else if (c.rssi > -115) bars = "▁  ";
            else                    bars = "   ";
            snprintf(buf, sizeof(buf), "%s %ddBm  %s", bars, c.rssi, c.name);
            lv_obj_t* item = lv_list_add_btn(list, nullptr, buf);
            lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_30, 0);
        }
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Contacts — Tap-to-message directory, sorted by name
// ════════════════════════════════════════════════════════
void contacts_screen_show()
{
    lv_obj_t* scr = make_screen("Contacts");

    char names[32][32];
    int n = slopos::mesh::exportContacts(names, 32);

    // Sort alphabetically
    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (strcmp(names[j], names[i]) < 0) {
                char tmp[32];
                strncpy(tmp, names[i], 31); tmp[31] = '\0';
                strncpy(names[i], names[j], 31); names[i][31] = '\0';
                strncpy(names[j], tmp, 31); names[j][31] = '\0';
            }

    if (n == 0) {
        lv_obj_t* info = lv_label_create(scr);
        lv_label_set_text(info,
            "No contacts yet.\n"
            "Nodes appear here once\n"
            "they broadcast an advert\n"
            "or send a message.\n\n"
            "Tap to send a direct\n"
            "message.");
        lv_obj_set_style_text_color(info, lv_color_hex(TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
        lv_obj_align(info, LV_ALIGN_TOP_LEFT, 8, 30);
        show_screen(scr);
        return;
    }

    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 28);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    for (int i = 0; i < n; i++) {
        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_FILE, names[i]);
        lv_obj_set_style_bg_color(btn, lv_color_hex(BG_TERTIARY), 0);
        // Navigate to chat
        lv_obj_add_event_cb(btn, [](lv_event_t*) {
            navigate_to(Screen::Chat);
        }, LV_EVENT_CLICKED, nullptr);
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Network — "Nearby Now": nodes seen in last 60 seconds
// ════════════════════════════════════════════════════════
void network_screen_show()
{
    lv_obj_t* scr = make_screen("Nearby Now");

    lv_obj_t* info = lv_label_create(scr);
    lv_obj_set_style_text_color(info, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 8, 28);

    slopos::mesh::ContactInfo contacts[32];
    int n = slopos::mesh::exportContactsFull(contacts, 32);
    uint32_t now = slopos::mesh::getCurrentTime();

    // Filter to nodes seen in last 60s and sort by recency
    int recent_n = 0;
    char buf[80];
    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 52);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    // Sort by most recently seen first
    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (contacts[j].last_seen > contacts[i].last_seen) {
                auto tmp = contacts[i];
                contacts[i] = contacts[j];
                contacts[j] = tmp;
            }

    for (int i = 0; i < n; i++) {
        // last_seen is RTC timestamp; compute age from current RTC time
        int32_t age_s = (int32_t)(now - contacts[i].last_seen);
        if (age_s < 0) age_s = 0;  // clock skew guard
        if (age_s > 120) continue; // older than 2 minutes, skip
        recent_n++;
        snprintf(buf, sizeof(buf), "%s  %ds ago  %ddBm",
                 contacts[i].name, age_s, contacts[i].rssi);
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    }

    if (recent_n == 0) {
        lv_label_set_text(info, "No nodes seen in the\nlast 2 minutes.");
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_WARNING,
            "Listening for nearby nodes...");
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    } else {
        snprintf(buf, sizeof(buf), "%d node%s nearby",
                 recent_n, recent_n == 1 ? "" : "s");
        lv_label_set_text(info, buf);
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Signal — radio signal details (live prefs)
// ════════════════════════════════════════════════════════
void signal_screen_show()
{
    lv_obj_t* scr = make_screen("Signal");

    int rssi = slopos::mesh::getLastRSSI();
    float snr = slopos::mesh::getLastSNR();
    int noise = slopos::mesh::getNoiseFloor();
    const slopos::NodePrefs& p = slopos::prefs_get();

    lv_obj_t* lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 30);

    char buf[512];
    if (p.configured) {
        snprintf(buf, sizeof(buf),
            "RSSI:    %d dBm\n"
            "SNR:     %.1f dB\n"
            "Noise:   %d dBm\n\n"
            "Freq:    %.3f MHz\n"
            "BW:      %.1f kHz\n"
            "SF:      %d\n"
            "CR:      4/%d\n"
            "TX Pwr:  %d dBm",
            rssi, snr, noise,
            p.freq, p.bw, p.sf, p.cr, p.tx_power_dbm);
    } else {
        snprintf(buf, sizeof(buf),
            "RSSI:    %d dBm\n"
            "SNR:     %.1f dB\n"
            "Noise:   %d dBm\n\n"
            "Radio:   NOT CONFIGURED\n"
            "Go to Settings -> Radio\n"
            "to set frequency/power.",
            rssi, snr, noise);
    }
    lv_label_set_text(lbl, buf);

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Noise — noise floor visualization
// ════════════════════════════════════════════════════════
void noise_screen_show()
{
    lv_obj_t* scr = make_screen("Noise Floor");

    int noise = slopos::mesh::getNoiseFloor();
    int rssi = slopos::mesh::getLastRSSI();

    // Noise bar
    lv_obj_t* bar_bg = lv_obj_create(scr);
    lv_obj_set_size(bar_bg, 280, 80);
    lv_obj_align(bar_bg, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(bar_bg, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(bar_bg, 8, 0);
    lv_obj_set_style_border_width(bar_bg, 0, 0);

    int bar_w = map(constrain(noise, -120, -60), -120, -60, 28, 252);
    lv_obj_t* bar_fill = lv_obj_create(bar_bg);
    lv_obj_set_size(bar_fill, bar_w, 60);
    lv_obj_align(bar_fill, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_bg_color(bar_fill, lv_color_hex(
        noise > -90 ? ACCENT_RED : noise > -105 ? ACCENT_ORANGE : ACCENT_GREEN), 0);
    lv_obj_set_style_radius(bar_fill, 4, 0);
    lv_obj_set_style_border_width(bar_fill, 0, 0);

    char buf[64];
    lv_obj_t* info = lv_label_create(scr);
    snprintf(buf, sizeof(buf), "Noise: %d dBm   |   RSSI: %d dBm", noise, rssi);
    lv_label_set_text(info, buf);
    lv_obj_set_style_text_color(info, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -30);

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Map — offline tile maps from SD card
// ════════════════════════════════════════════════════════
void map_screen_show()
{
    lv_obj_t* scr = make_screen("Map");

    slopos_map_init();
    slopos_map_reparent(scr);
    slopos_map_render();

    lv_obj_t* map = lv_obj_create(scr);
    lv_obj_set_size(map, TFT_WIDTH, TFT_HEIGHT - 28);
    lv_obj_align(map, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(map, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(map, 0, 0);

    static int drag_start_x = 0, drag_start_y = 0;
    lv_obj_add_event_cb(map, [](lv_event_t* e) {
        lv_indev_t* indev = lv_indev_get_act();
        lv_point_t pt;
        lv_indev_get_point(indev, &pt);
        int code = lv_event_get_code(e);

        if (code == LV_EVENT_PRESSED) {
            drag_start_x = pt.x;
            drag_start_y = pt.y;
        } else if (code == LV_EVENT_PRESSING) {
            int dx = drag_start_x - pt.x;
            int dy = drag_start_y - pt.y;
            drag_start_x = pt.x;
            drag_start_y = pt.y;
            if (dx != 0 || dy != 0) {
                slopos_map_pan(dx, dy);
            }
        }
    }, LV_EVENT_ALL, nullptr);

    lv_obj_t* zoom_in = lv_btn_create(scr);
    lv_obj_set_size(zoom_in, 32, 32);
    lv_obj_align(zoom_in, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_bg_color(zoom_in, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(zoom_in, 16, 0);
    lv_obj_t* zi = lv_label_create(zoom_in);
    lv_label_set_text(zi, "+");
    lv_obj_center(zi);
    lv_obj_add_event_cb(zoom_in, [](lv_event_t*) { slopos_map_zoom_in(); },
                       LV_EVENT_CLICKED, nullptr);

    lv_obj_t* zoom_out = lv_btn_create(scr);
    lv_obj_set_size(zoom_out, 32, 32);
    lv_obj_align(zoom_out, LV_ALIGN_BOTTOM_RIGHT, -8, -44);
    lv_obj_set_style_bg_color(zoom_out, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(zoom_out, 16, 0);
    lv_obj_t* zo = lv_label_create(zoom_out);
    lv_label_set_text(zo, "-");
    lv_obj_center(zo);
    lv_obj_add_event_cb(zoom_out, [](lv_event_t*) { slopos_map_zoom_out(); },
                       LV_EVENT_CLICKED, nullptr);

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Settings
// ════════════════════════════════════════════════════════
void settings_screen_show()
{
    lv_obj_t* scr = make_screen("Settings");

    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 28);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    const slopos::NodePrefs& p = slopos::prefs_get();
    char buf[128];

    // Node name
    snprintf(buf, sizeof(buf), "  Name: %s", p.node_name);
    lv_obj_t* btn0 = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, buf);
    lv_obj_set_style_bg_color(btn0, lv_color_hex(BG_TERTIARY), 0);

    // Radio status
    if (p.configured) {
        snprintf(buf, sizeof(buf), "  Radio: %.3f MHz / %.1f kHz / SF%d / %d dBm",
                 p.freq, p.bw, p.sf, p.tx_power_dbm);
    } else {
        snprintf(buf, sizeof(buf), "  Radio: NOT CONFIGURED");
    }
    lv_obj_t* btn_rf = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
    lv_obj_set_style_bg_color(btn_rf, lv_color_hex(
        p.configured ? BG_TERTIARY : 0x4a2020), 0);

    lv_obj_add_event_cb(btn_rf, [](lv_event_t*) {
        radio_setup_screen_show();
    }, LV_EVENT_CLICKED, nullptr);

    // SD Card
    snprintf(buf, sizeof(buf), "  SD Card: %s",
             slopos_sdcard_mounted() ? "Mounted" : "Not mounted");
    lv_obj_t* btn4 = lv_list_add_btn(list, LV_SYMBOL_SD_CARD, buf);
    lv_obj_set_style_bg_color(btn4, lv_color_hex(BG_TERTIARY), 0);

    // GPS
    snprintf(buf, sizeof(buf), "  GPS: %s",
             slopos_gps_has_fix() ? "Fix acquired" : "No fix");
    lv_obj_t* btn5 = lv_list_add_btn(list, LV_SYMBOL_GPS, buf);
    lv_obj_set_style_bg_color(btn5, lv_color_hex(BG_TERTIARY), 0);

    // Version
    snprintf(buf, sizeof(buf), "  About SlopOS " SLOPOS_VERSION);
    lv_obj_t* btn6 = lv_list_add_btn(list, LV_SYMBOL_HOME, buf);
    lv_obj_set_style_bg_color(btn6, lv_color_hex(BG_TERTIARY), 0);

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Terminal — serial console
// ════════════════════════════════════════════════════════
void terminal_screen_show()
{
    lv_obj_t* scr = make_screen("Terminal");

    lv_obj_t* term = lv_textarea_create(scr);
    lv_obj_set_size(term, LV_PCT(100), TFT_HEIGHT - 60);
    lv_obj_align(term, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_color(term, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_color(term, lv_color_hex(0x00ff00), 0);
    lv_obj_set_style_text_font(term, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_width(term, 0, 0);
    lv_obj_set_style_pad_all(term, 4, 0);

    const slopos::NodePrefs& p = slopos::prefs_get();
    char header[256];
    snprintf(header, sizeof(header),
        "SlopOS T-Deck Terminal\n"
        "MeshCore protocol active\n"
        "Radio: %s\n"
        "> _\n",
        p.configured ? "SX1262 configured" : "NOT CONFIGURED");
    lv_textarea_set_text(term, header);

    lv_obj_t* input = lv_textarea_create(scr);
    lv_obj_set_size(input, LV_PCT(100), 28);
    lv_obj_align(input, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(input, lv_color_hex(BG_INPUT), 0);
    lv_obj_set_style_text_color(input, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(input, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_width(input, 0, 0);
    lv_obj_set_style_pad_all(input, 4, 0);
    lv_textarea_set_one_line(input, true);
    lv_textarea_set_placeholder_text(input, "> enter command...");

    lv_obj_add_event_cb(input, [](lv_event_t* e) {
        lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
        const char* cmd = lv_textarea_get_text(ta);
        if (!cmd || !cmd[0]) return;

        lv_obj_t* term = (lv_obj_t*)lv_event_get_user_data(e);
        char result[256] = "";

        if (strcmp(cmd, "help") == 0) {
            snprintf(result, sizeof(result), "Commands: help status advert ping\n");
        } else if (strcmp(cmd, "status") == 0) {
            int rssi = slopos::mesh::getLastRSSI();
            float snr = slopos::mesh::getLastSNR();
            int noise = slopos::mesh::getNoiseFloor();
            int contacts = slopos::mesh::getContactCount();
            int channels = slopos::mesh::getChannelCount();
            snprintf(result, sizeof(result),
                "RSSI:%ddBm SNR:%.1f Noise:%ddBm\n"
                "Contacts:%d Channels:%d\n",
                rssi, snr, noise, contacts, channels);
        } else if (strcmp(cmd, "advert") == 0) {
            bool ok = slopos::mesh::sendAdvert();
            snprintf(result, sizeof(result), ok ? "Advert sent\n" : "Send failed\n");
        } else if (strcmp(cmd, "ping") == 0) {
            snprintf(result, sizeof(result), "Pong! Uptime: %lu ms\n", millis());
        } else {
            snprintf(result, sizeof(result), "Unknown: %s\nType 'help'\n", cmd);
        }

        lv_textarea_add_text(term, result);
        lv_textarea_set_text(ta, "");
    }, LV_EVENT_READY, term);

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Trace — path trace route with result display
// ════════════════════════════════════════════════════════
static lv_obj_t* trace_result_label = nullptr;

// Clean up on screen delete
static void trace_screen_delete_cb(lv_event_t* e) {
    trace_result_label = nullptr;
}

void trace_screen_show()
{
    lv_obj_t* scr = make_screen("Trace Route");
    slopos::mesh::clearTraceResult();
    trace_result_label = nullptr;

    // When this screen is auto-deleted, clear our dangling pointer
    lv_obj_add_event_cb(scr, trace_screen_delete_cb, LV_EVENT_DELETE, nullptr);

    char names[32][32];
    int total = slopos::mesh::exportContacts(names, 32);

    lv_obj_t* info = lv_label_create(scr);
    lv_obj_set_style_text_color(info, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 8, 30);

    if (total == 0) {
        lv_label_set_text(info, "No contacts discovered.\nWait for adverts or\nincoming messages.");
        show_screen(scr);
        return;
    }

    lv_label_set_text(info, "Tap a contact to trace\nthe path to that node.");

    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 100);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    char buf[48];
    for (int i = 0; i < total; i++) {
        bool has_path = slopos::mesh::contactHasPath(i);
        snprintf(buf, sizeof(buf), "%s  %s",
                 names[i], has_path ? "[has path]" : "[no path]");
        lv_obj_t* btn = lv_list_add_btn(list, has_path ? LV_SYMBOL_GPS : LV_SYMBOL_WARNING, buf);
        lv_obj_set_style_bg_color(btn, lv_color_hex(BG_TERTIARY), 0);

        if (has_path) {
            int contact_idx = i;
            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                int idx = (int)(intptr_t)lv_event_get_user_data(e);
                uint32_t tag;
                if (slopos::mesh::sendTrace(idx, &tag)) {
                    // Poll for result on a timer
                    lv_obj_t* scr_ref = lv_obj_get_screen((lv_obj_t*)lv_event_get_target(e));
                    lv_obj_t* result_lbl = lv_label_create(scr_ref);
                    lv_obj_set_style_text_color(result_lbl, lv_color_hex(ACCENT), 0);
                    lv_obj_set_style_text_font(result_lbl, &lv_font_montserrat_12, 0);
                    lv_obj_align(result_lbl, LV_ALIGN_BOTTOM_MID, 0, -8);
                    lv_label_set_text(result_lbl, "Trace sent, waiting...");
                    trace_result_label = result_lbl;

                    // Create a timer to poll for result
                    lv_timer_create([](lv_timer_t* t) {
                        if (!trace_result_label) {
                            lv_timer_del(t);
                            return;
                        }
                        if (slopos::mesh::hasTraceResult()) {
                            uint8_t len = slopos::mesh::getTracePathLen();
                            uint8_t snrs[16], hashes[16];
                            slopos::mesh::getTracePath(snrs, hashes);

                            char res[128];
                            if (len == 0) {
                                snprintf(res, sizeof(res), "Trace timed out — no response");
                            } else {
                                int pos = snprintf(res, sizeof(res),
                                    "Trace: %d hop%s", len, len == 1 ? "" : "s");
                                for (int h = 0; h < len && pos < (int)sizeof(res) - 10; h++) {
                                    pos += snprintf(res + pos, sizeof(res) - pos,
                                        "  [%ddBm]", snrs[h]);
                                    if (h > 0 && pos >= (int)sizeof(res) - 10) {
                                        pos += snprintf(res + pos, sizeof(res) - pos, "...");
                                        break;
                                    }
                                }
                            }
                            lv_label_set_text(trace_result_label, res);
                            slopos::mesh::clearTraceResult();
                            lv_timer_del(t);
                        }
                    }, 500, nullptr);  // poll every 500ms
                }
            }, LV_EVENT_CLICKED, (void*)(intptr_t)contact_idx);
        }
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Channels — channel list with create/join
// ════════════════════════════════════════════════════════
static void refresh_channel_list(lv_obj_t* list);

static lv_obj_t* channel_create_dialog(lv_obj_t* parent)
{
    lv_obj_t* dialog = lv_obj_create(parent);
    lv_obj_set_size(dialog, 260, 180);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_radius(dialog, 8, 0);
    lv_obj_set_style_border_width(dialog, 0, 0);
    lv_obj_set_style_pad_all(dialog, 8, 0);

    lv_obj_t* title = lv_label_create(dialog);
    lv_label_set_text(title, "Create Channel");
    lv_obj_set_style_text_color(title, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // Name input
    lv_obj_t* name_label = lv_label_create(dialog);
    lv_label_set_text(name_label, "Name:");
    lv_obj_set_style_text_color(name_label, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 4, 28);

    lv_obj_t* name_input = lv_textarea_create(dialog);
    lv_obj_set_size(name_input, 244, 28);
    lv_obj_align(name_input, LV_ALIGN_TOP_MID, 0, 46);
    lv_obj_set_style_bg_color(name_input, lv_color_hex(BG_INPUT), 0);
    lv_obj_set_style_text_color(name_input, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(name_input, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_width(name_input, 0, 0);
    lv_textarea_set_one_line(name_input, true);
    lv_textarea_set_placeholder_text(name_input, "e.g. #general");

    // PSK input
    lv_obj_t* psk_label = lv_label_create(dialog);
    lv_label_set_text(psk_label, "PSK (base64):");
    lv_obj_set_style_text_color(psk_label, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_align(psk_label, LV_ALIGN_TOP_LEFT, 4, 82);

    lv_obj_t* psk_input = lv_textarea_create(dialog);
    lv_obj_set_size(psk_input, 244, 28);
    lv_obj_align(psk_input, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_color(psk_input, lv_color_hex(BG_INPUT), 0);
    lv_obj_set_style_text_color(psk_input, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(psk_input, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_width(psk_input, 0, 0);
    lv_textarea_set_one_line(psk_input, true);
    lv_textarea_set_placeholder_text(psk_input, "base64 key");

    // Feedback label
    lv_obj_t* feedback = lv_label_create(dialog);
    lv_obj_set_style_text_color(feedback, lv_color_hex(ACCENT_RED), 0);
    lv_obj_set_style_text_font(feedback, &lv_font_montserrat_12, 0);
    lv_obj_align(feedback, LV_ALIGN_BOTTOM_MID, 0, -32);

    // Create button
    lv_obj_t* create_btn = lv_btn_create(dialog);
    lv_obj_set_size(create_btn, 100, 28);
    lv_obj_align(create_btn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(create_btn, lv_color_hex(ACCENT_GREEN), 0);
    lv_obj_set_style_radius(create_btn, 4, 0);
    lv_obj_t* cbl = lv_label_create(create_btn);
    lv_label_set_text(cbl, "Create");
    lv_obj_center(cbl);

    lv_obj_add_event_cb(create_btn, [](lv_event_t* e) {
        lv_obj_t* dlg = lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e));
        lv_obj_t* scr = lv_obj_get_screen(dlg);
        lv_obj_t* fb = (lv_obj_t*)lv_event_get_user_data(e);  // feedback label

        // Walk children to find name_input, psk_input
        uint32_t n = lv_obj_get_child_cnt(dlg);
        lv_obj_t* name_in = nullptr;
        lv_obj_t* psk_in = nullptr;
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t* child = lv_obj_get_child(dlg, i);
            if (lv_obj_check_type(child, &lv_textarea_class)) {
                if (!name_in) name_in = child;
                else psk_in = child;
            }
        }

        const char* name = name_in ? lv_textarea_get_text(name_in) : "";
        const char* psk = psk_in ? lv_textarea_get_text(psk_in) : "";

        if (!name[0]) {
            if (fb) lv_label_set_text(fb, "Enter a channel name");
            return;
        }
        if (!psk[0]) {
            if (fb) lv_label_set_text(fb, "Enter a PSK");
            return;
        }

        bool ok = slopos::mesh::addChannel(name, psk);
        if (ok) {
            lv_obj_del_async(dlg);  // async: safe to call from own event handler
            for (uint32_t i = 0; i < lv_obj_get_child_cnt(scr); i++) {
                lv_obj_t* child = lv_obj_get_child(scr, i);
                if (lv_obj_check_type(child, &lv_list_class)) {
                    refresh_channel_list(child);
                    break;
                }
            }
        } else {
            if (fb) lv_label_set_text(fb, "Invalid PSK (must be base64, 16 or 32 byte key)");
        }
    }, LV_EVENT_CLICKED, (void*)feedback);

    return dialog;
}

static void refresh_channel_list(lv_obj_t* list)
{
    lv_obj_clean(list);

    char names[8][32];
    int n = slopos::mesh::exportChannels(names, 8);

    if (n == 0) {
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_WARNING, "No channels joined");
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    } else {
        for (int i = 0; i < n; i++) {
            lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_LIST, names[i]);
            lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
        }
    }
}

void channels_screen_show()
{
    lv_obj_t* scr = make_screen("Channels");

    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 56);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    refresh_channel_list(list);

    // "+" button to create channel
    lv_obj_t* add_btn = lv_btn_create(scr);
    lv_obj_set_size(add_btn, 120, 28);
    lv_obj_align(add_btn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(add_btn, 4, 0);
    lv_obj_t* al = lv_label_create(add_btn);
    lv_label_set_text(al, "+ Create Channel");
    lv_obj_set_style_text_font(al, &lv_font_montserrat_12, 0);
    lv_obj_center(al);

    // Capture list for dialog callback
    lv_obj_add_event_cb(add_btn, [](lv_event_t* e) {
        lv_obj_t* scr = lv_obj_get_screen((lv_obj_t*)lv_event_get_target(e));
        channel_create_dialog(scr);
    }, LV_EVENT_CLICKED, nullptr);

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Advertise — broadcast presence
// ════════════════════════════════════════════════════════
void advertise_screen_show()
{
    lv_obj_t* scr = make_screen("Advertise");

    lv_obj_t* info = lv_label_create(scr);
    lv_label_set_text(info,
        "Advertise Presence\n\n"
        "Broadcast your node to\n"
        "the mesh network.\n\n"
        "Other nodes will see you\n"
        "in their Heard list.\n\n"
        "Tap below to send advert.");
    lv_obj_set_style_text_color(info, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 8, 30);

    lv_obj_t* btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 120, 36);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, "Advertise Now");
    lv_obj_center(bl);
    lv_obj_add_event_cb(btn, [](lv_event_t*) {
        slopos::mesh::sendAdvert();
    }, LV_EVENT_CLICKED, nullptr);

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Radio Setup — configure frequency, SF, power
// ════════════════════════════════════════════════════════
void radio_setup_screen_show()
{
    lv_obj_t* scr = make_screen("Radio Setup");

    const slopos::NodePrefs& p = slopos::prefs_get();
    float   new_freq = p.configured ? p.freq : 869.618f;
    float   new_bw   = p.configured ? p.bw   : 62.5f;
    (void)new_bw;  // preserved for future BW selector
    int     new_sf   = p.configured ? p.sf   : 8;
    int     new_cr   = p.configured ? p.cr   : 5;
    int     new_pwr  = p.configured ? p.tx_power_dbm : 22;

    auto* label = lv_label_create(scr);
    lv_label_set_text(label,
        "Configure your radio settings.\n"
        "Incorrect values may be illegal\n"
        "in your region. Check local\n"
        "regulations before transmitting.");
    lv_obj_set_style_text_color(label, lv_color_hex(0xccaa00), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 8, 30);

    // Frequency presets — MUST be static, button callbacks store pointers here
    static const struct { const char* label; float freq; } freqs[] = {
        {"868.000 MHz (EU)", 868.000f},
        {"869.525 MHz (UK)", 869.525f},
        {"869.618 MHz (UK)", 869.618f},
        {"915.000 MHz (US)", 915.000f},
        {"433.500 MHz (EU)", 433.500f},
    };
    auto* fl = lv_label_create(scr);
    lv_label_set_text(fl, "Frequency:");
    lv_obj_set_style_text_color(fl, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(fl, &lv_font_montserrat_12, 0);
    lv_obj_align(fl, LV_ALIGN_TOP_LEFT, 8, 105);

    // Store mutable state in static locals (screen is recreated each visit)
    static float s_freq = 869.618f;
    static int   s_sf   = 8;
    static int   s_cr   = 5;
    static int   s_pwr  = 22;
    s_freq = new_freq;
    s_sf   = new_sf;
    s_cr   = new_cr;
    s_pwr  = new_pwr;

    int y = 128;
    for (auto& f : freqs) {
        auto* btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 200, 22);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 8, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(
            fabsf(s_freq - f.freq) < 0.001f ? 0x2a5a2a : BG_TERTIARY), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        auto* tl = lv_label_create(btn);
        lv_label_set_text(tl, f.label);
        lv_obj_set_style_text_font(tl, &lv_font_montserrat_12, 0);
        lv_obj_center(tl);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            float* pf = (float*)lv_event_get_user_data(e);
            s_freq = *pf;
            lv_obj_set_style_bg_color((lv_obj_t*)lv_event_get_target(e),
                lv_color_hex(0x2a5a2a), 0);
        }, LV_EVENT_CLICKED, (void*)&f.freq);
        y += 25;
    }

    // SF selector
    char buf[64];
    int sf_y = y + 10;
    snprintf(buf, sizeof(buf), "SF: %d  (tap +/-)", s_sf);
    auto* sf_lbl = lv_label_create(scr);
    lv_label_set_text(sf_lbl, buf);
    lv_obj_set_style_text_color(sf_lbl, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(sf_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(sf_lbl, LV_ALIGN_TOP_LEFT, 8, sf_y);

    auto* sf_plus = lv_btn_create(scr);
    lv_obj_set_size(sf_plus, 30, 22);
    lv_obj_align(sf_plus, LV_ALIGN_TOP_LEFT, 100, sf_y - 2);
    lv_obj_set_style_bg_color(sf_plus, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(sf_plus, 4, 0);
    auto* spl = lv_label_create(sf_plus); lv_label_set_text(spl, "+"); lv_obj_center(spl);
    lv_obj_add_event_cb(sf_plus, [](lv_event_t* e) {
        if (s_sf < 12) {
            s_sf++;
            char b[24]; snprintf(b, sizeof(b), "SF: %d  (tap +/-)", s_sf);
            lv_label_set_text((lv_obj_t*)lv_event_get_user_data(e), b);
        }
    }, LV_EVENT_CLICKED, (void*)sf_lbl);

    auto* sf_minus = lv_btn_create(scr);
    lv_obj_set_size(sf_minus, 30, 22);
    lv_obj_align(sf_minus, LV_ALIGN_TOP_LEFT, 135, sf_y - 2);
    lv_obj_set_style_bg_color(sf_minus, lv_color_hex(ACCENT_RED), 0);
    lv_obj_set_style_radius(sf_minus, 4, 0);
    auto* sml = lv_label_create(sf_minus); lv_label_set_text(sml, "-"); lv_obj_center(sml);
    lv_obj_add_event_cb(sf_minus, [](lv_event_t* e) {
        if (s_sf > 6) {
            s_sf--;
            char b[24]; snprintf(b, sizeof(b), "SF: %d  (tap +/-)", s_sf);
            lv_label_set_text((lv_obj_t*)lv_event_get_user_data(e), b);
        }
    }, LV_EVENT_CLICKED, (void*)sf_lbl);

    // Power selector
    int pwr_y = sf_y + 30;
    snprintf(buf, sizeof(buf), "TX Power: %d dBm  (tap +/-)", s_pwr);
    auto* pwr_lbl = lv_label_create(scr);
    lv_label_set_text(pwr_lbl, buf);
    lv_obj_set_style_text_color(pwr_lbl, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(pwr_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(pwr_lbl, LV_ALIGN_TOP_LEFT, 8, pwr_y);

    auto* pwr_plus = lv_btn_create(scr);
    lv_obj_set_size(pwr_plus, 30, 22);
    lv_obj_align(pwr_plus, LV_ALIGN_TOP_LEFT, 120, pwr_y - 2);
    lv_obj_set_style_bg_color(pwr_plus, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(pwr_plus, 4, 0);
    auto* ppl = lv_label_create(pwr_plus); lv_label_set_text(ppl, "+"); lv_obj_center(ppl);
    lv_obj_add_event_cb(pwr_plus, [](lv_event_t* e) {
        if (s_pwr < 22) {
            s_pwr++;
            char b[32]; snprintf(b, sizeof(b), "TX Power: %d dBm  (tap +/-)", s_pwr);
            lv_label_set_text((lv_obj_t*)lv_event_get_user_data(e), b);
        }
    }, LV_EVENT_CLICKED, (void*)pwr_lbl);

    auto* pwr_minus = lv_btn_create(scr);
    lv_obj_set_size(pwr_minus, 30, 22);
    lv_obj_align(pwr_minus, LV_ALIGN_TOP_LEFT, 155, pwr_y - 2);
    lv_obj_set_style_bg_color(pwr_minus, lv_color_hex(ACCENT_RED), 0);
    lv_obj_set_style_radius(pwr_minus, 4, 0);
    auto* pml = lv_label_create(pwr_minus); lv_label_set_text(pml, "-"); lv_obj_center(pml);
    lv_obj_add_event_cb(pwr_minus, [](lv_event_t* e) {
        if (s_pwr > 2) {
            s_pwr--;
            char b[32]; snprintf(b, sizeof(b), "TX Power: %d dBm  (tap +/-)", s_pwr);
            lv_label_set_text((lv_obj_t*)lv_event_get_user_data(e), b);
        }
    }, LV_EVENT_CLICKED, (void*)pwr_lbl);

    // Save button
    auto* save_btn = lv_btn_create(scr);
    lv_obj_set_size(save_btn, 160, 36);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(ACCENT_GREEN), 0);
    lv_obj_set_style_radius(save_btn, 8, 0);
    auto* svl = lv_label_create(save_btn);
    lv_label_set_text(svl, "Save & Reboot");
    lv_obj_center(svl);
    lv_obj_add_event_cb(save_btn, [](lv_event_t*) {
        slopos::NodePrefs np;
        np.set_defaults();
        np.freq = s_freq;
        np.bw   = 62.5f;
        np.sf   = (uint8_t)s_sf;
        np.cr   = (uint8_t)s_cr;
        np.tx_power_dbm = (int8_t)s_pwr;
        np.configured = true;
        strncpy(np.node_name, "SlopOS T-Deck", sizeof(np.node_name) - 1);
        np.node_name[sizeof(np.node_name) - 1] = '\0';
        slopos::prefs_set(np);
        slopos::prefs_save(np);
        ESP.restart();
    }, LV_EVENT_CLICKED, nullptr);

    show_screen(scr);
}

} // namespace slopos::ui
