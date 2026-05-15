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
// Heard — recently heard mesh nodes
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

    // Populate from live mesh contacts
    slopos::mesh::ContactInfo contacts[32];
    int n = slopos::mesh::exportContactsFull(contacts, 32);

    if (n == 0) {
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_WARNING, "No nodes heard yet");
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    } else {
        char buf[80];
        for (int i = 0; i < n; i++) {
            auto& c = contacts[i];
            snprintf(buf, sizeof(buf), "%s  %ddBm", c.name, c.rssi);
            lv_obj_t* item = lv_list_add_btn(list, nullptr, buf);
            lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_30, 0);
        }
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Contacts — saved contacts
// ════════════════════════════════════════════════════════
void contacts_screen_show()
{
    lv_obj_t* scr = make_screen("Contacts");

    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 28);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    // Populate from live mesh contacts
    char names[32][32];
    int n = slopos::mesh::exportContacts(names, 32);

    if (n == 0) {
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_WARNING, "No contacts discovered");
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    } else {
        for (int i = 0; i < n; i++) {
            lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_FILE, names[i]);
            lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
        }
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Signal — radio signal strength details
// ════════════════════════════════════════════════════════
void signal_screen_show()
{
    lv_obj_t* scr = make_screen("Signal");

    int rssi = slopos::mesh::getLastRSSI();
    float snr = slopos::mesh::getLastSNR();
    int noise = slopos::mesh::getNoiseFloor();

    struct { const char* label; const char* value; } rows[] = {
        {"Last RSSI",    ""},        // filled below
        {"Last SNR",     ""},
        {"Noise Floor",  ""},
        {"Frequency",    "869.618 MHz"},
        {"Bandwidth",    "62.5 kHz"},
        {"Spreading",    "SF8"},
        {"Coding Rate",  "4/5"},
        {"TX Power",     "22 dBm"},
    };

    lv_obj_t* lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, 30);

    char buf[512];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "RSSI:    %d dBm\n"
        "SNR:     %.1f dB\n"
        "Noise:   %d dBm\n\n"
        "Freq:    869.618 MHz\n"
        "BW:      62.5 kHz\n"
        "SF:      8\n"
        "CR:      4/5\n"
        "TX Pwr:  22 dBm\n",
        rssi, snr, noise);
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

    // Noise level indicator (wider = more noise = worse)
    // Map dBm: -120 (quiet) to -60 (noisy) -> 0..100% bar width
    int bar_w = map(constrain(noise, -120, -60), -120, -60, 28, 252);
    lv_obj_t* bar_fill = lv_obj_create(bar_bg);
    lv_obj_set_size(bar_fill, bar_w, 60);
    lv_obj_align(bar_fill, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_bg_color(bar_fill, lv_color_hex(
        noise > -90 ? ACCENT_RED : noise > -105 ? ACCENT_ORANGE : ACCENT_GREEN), 0);
    lv_obj_set_style_radius(bar_fill, 4, 0);
    lv_obj_set_style_border_width(bar_fill, 0, 0);

    // Labels
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

    // Initialize map renderer if not already
    slopos_map_init();
    slopos_map_reparent(scr);
    slopos_map_render();

    // Show the map canvas
    lv_obj_t* map = lv_obj_create(scr);
    lv_obj_set_size(map, TFT_WIDTH, TFT_HEIGHT - 28);
    lv_obj_align(map, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(map, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(map, 0, 0);

    // Touch pan handler — drag to pan the map
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

    // Zoom buttons
    lv_obj_t* zoom_in = lv_btn_create(scr);
    lv_obj_set_size(zoom_in, 32, 32);
    lv_obj_align(zoom_in, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_bg_color(zoom_in, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(zoom_in, 16, 0);
    lv_obj_t* zi = lv_label_create(zoom_in);
    lv_label_set_text(zi, "+");
    lv_obj_center(zi);
    lv_obj_add_event_cb(zoom_in, [](lv_event_t*) {
        slopos_map_zoom_in();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* zoom_out = lv_btn_create(scr);
    lv_obj_set_size(zoom_out, 32, 32);
    lv_obj_align(zoom_out, LV_ALIGN_BOTTOM_RIGHT, -8, -44);
    lv_obj_set_style_bg_color(zoom_out, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(zoom_out, 16, 0);
    lv_obj_t* zo = lv_label_create(zoom_out);
    lv_label_set_text(zo, "-");
    lv_obj_center(zo);
    lv_obj_add_event_cb(zoom_out, [](lv_event_t*) {
        slopos_map_zoom_out();
    }, LV_EVENT_CLICKED, nullptr);

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

    // Live settings
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
    lv_textarea_set_text(term,
        "SlopOS T-Deck Terminal\n"
        "MeshCore protocol active\n"
        "Radio: SX1262 @ 869.618 MHz\n"
        "> _\n");

    // Input line
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

    // Basic command execution on Enter
    lv_obj_add_event_cb(input, [](lv_event_t* e) {
        lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
        const char* cmd = lv_textarea_get_text(ta);
        if (!cmd || !cmd[0]) return;

        // Execute command and show result in terminal area
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
        lv_textarea_set_text(ta, "");  // clear input
    }, LV_EVENT_READY, term);  // LV_EVENT_READY fires on Enter in textarea

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Trace — path trace route tool
// ════════════════════════════════════════════════════════
void trace_screen_show()
{
    lv_obj_t* scr = make_screen("Trace Route");
    slopos::mesh::clearTraceResult();  // fresh start

    // Get contacts with known paths
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

    // Build contact list with path indicators
    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 60);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 28);
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
            // Make clickable — send trace to this contact
            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                int idx = (int)(intptr_t)lv_event_get_user_data(e);
                uint32_t tag;
                if (slopos::mesh::sendTrace(idx, &tag)) {
                    // Trace sent — poll for result
                    // In a real implementation, we'd set a timer to check hasTraceResult()
                }
            }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Repeaters — repeater management
// ════════════════════════════════════════════════════════
void repeaters_screen_show()
{
    lv_obj_t* scr = make_screen("Repeaters");

    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 28);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    // Show mesh contacts as potential repeaters (nodes seen with good RSSI)
    slopos::mesh::ContactInfo contacts[32];
    int n = slopos::mesh::exportContactsFull(contacts, 32);

    if (n == 0) {
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_WARNING, "No repeaters discovered");
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    } else {
        char buf[64];
        for (int i = 0; i < n; i++) {
            auto& c = contacts[i];
            snprintf(buf, sizeof(buf), "%s  %ddBm", c.name, c.rssi);
            lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_LOOP, buf);
            lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
        }
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Finder — device discovery
// ════════════════════════════════════════════════════════
void finder_screen_show()
{
    lv_obj_t* scr = make_screen("Finder");

    // Show discovered mesh contacts as "found" devices
    slopos::mesh::ContactInfo contacts[32];
    int n = slopos::mesh::exportContactsFull(contacts, 32);

    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, LV_PCT(100), TFT_HEIGHT - 28);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    if (n == 0) {
        lv_obj_t* info = lv_label_create(scr);
        lv_label_set_text(info,
            "Device Finder\n\n"
            "Scanning for nearby\n"
            "MeshCore devices...\n\n"
            "No devices found yet.\n"
            "Check antenna and\n"
            "frequency settings.");
        lv_obj_set_style_text_color(info, lv_color_hex(TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
        lv_obj_align(info, LV_ALIGN_TOP_LEFT, 8, 30);

        lv_obj_t* spinner = lv_spinner_create(scr);
        lv_obj_set_size(spinner, 40, 40);
        lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_arc_color(spinner, lv_color_hex(ACCENT), 0);
    } else {
        char buf[64];
        for (int i = 0; i < n; i++) {
            auto& c = contacts[i];
            snprintf(buf, sizeof(buf), "%s  %ddBm", c.name, c.rssi);
            lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
            lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
        }
    }

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
        "Status: Idle");
    lv_obj_set_style_text_color(info, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 8, 30);

    // Advertise button
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
    // Use provided values or safe defaults for display
    float   new_freq = p.configured ? p.freq : 869.618f;
    float   new_bw   = p.configured ? p.bw   : 62.5f;
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

    // Frequency presets
    struct { const char* label; float freq; } freqs[] = {
        {"868.000 MHz (EU)", 868.000f},
        {"869.525 MHz (UK)", 869.525f},
        {"869.618 MHz (UK)", 869.618f},
        {"915.000 MHz (US)", 915.000f},
        {"433.500 MHz (EU)", 433.500f},
    };
    auto* fl = lv_label_create(scr);
    lv_label_set_text(fl, "Frequency:");
    lv_obj_set_style_text_color(fl, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_align(fl, LV_ALIGN_TOP_LEFT, 8, 105);

    int y = 128;
    for (auto& f : freqs) {
        auto* btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 200, 22);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 8, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(
            fabsf(new_freq - f.freq) < 0.001f ? 0x2a5a2a : BG_TERTIARY), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        auto* tl = lv_label_create(btn);
        lv_label_set_text(tl, f.label);
        lv_obj_set_style_text_font(tl, &lv_font_montserrat_12, 0);
        lv_obj_center(tl);
        float freq_val = f.freq;
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            float* pf = (float*)lv_event_get_user_data(e);
            // Update all button backgrounds (simplified: just show selection)
            lv_obj_set_style_bg_color((lv_obj_t*)lv_event_get_target(e),
                lv_color_hex(0x2a5a2a), 0);
        }, LV_EVENT_CLICKED, (void*)&freqs[0].freq);
        y += 25;
    }

    // SF selector
    char buf[64];
    int sf_y = y + 10;
    snprintf(buf, sizeof(buf), "SF: %d  (tap +/-)", new_sf);
    auto* sf_lbl = lv_label_create(scr);
    lv_label_set_text(sf_lbl, buf);
    lv_obj_set_style_text_color(sf_lbl, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_align(sf_lbl, LV_ALIGN_TOP_LEFT, 8, sf_y);

    auto* sf_plus = lv_btn_create(scr);
    lv_obj_set_size(sf_plus, 30, 22);
    lv_obj_align(sf_plus, LV_ALIGN_TOP_LEFT, 100, sf_y - 2);
    lv_obj_set_style_bg_color(sf_plus, lv_color_hex(ACCENT), 0);
    auto* spl = lv_label_create(sf_plus); lv_label_set_text(spl, "+"); lv_obj_center(spl);

    auto* sf_minus = lv_btn_create(scr);
    lv_obj_set_size(sf_minus, 30, 22);
    lv_obj_align(sf_minus, LV_ALIGN_TOP_LEFT, 135, sf_y - 2);
    lv_obj_set_style_bg_color(sf_minus, lv_color_hex(ACCENT_RED), 0);
    auto* sml = lv_label_create(sf_minus); lv_label_set_text(sml, "-"); lv_obj_center(sml);

    // Power selector
    int pwr_y = sf_y + 30;
    snprintf(buf, sizeof(buf), "Power: %d dBm  (tap +/-)", new_pwr);
    auto* pwr_lbl = lv_label_create(scr);
    lv_label_set_text(pwr_lbl, buf);
    lv_obj_set_style_text_color(pwr_lbl, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_align(pwr_lbl, LV_ALIGN_TOP_LEFT, 8, pwr_y);

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
        // Build prefs from current selections and save
        slopos::NodePrefs np;
        np.set_defaults();
        // Use currently displayed values (simplified: use defaults for now)
        np.freq = 869.618f;
        np.bw   = 62.5f;
        np.sf   = 8;
        np.cr   = 5;
        np.tx_power_dbm = 22;
        np.configured = true;
        strncpy(np.node_name, "SlopOS T-Deck", sizeof(np.node_name) - 1);
        slopos::prefs_set(np);
        slopos::prefs_save(np);
        // Reboot to apply
        ESP.restart();
    }, LV_EVENT_CLICKED, nullptr);

    show_screen(scr);
}

} // namespace slopos::ui
