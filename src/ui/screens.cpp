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
#include "../mesh/mesh_wrapper.h"
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
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
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
// Map — placeholder for offline tile maps
// ════════════════════════════════════════════════════════
void map_screen_show()
{
    lv_obj_t* scr = make_screen("Map");

    lv_obj_t* holder = lv_obj_create(scr);
    lv_obj_set_size(holder, LV_PCT(100), TFT_HEIGHT - 50);
    lv_obj_align(holder, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_bg_color(holder, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_border_width(holder, 0, 0);

    lv_obj_t* icon = lv_label_create(holder);
    lv_label_set_text(icon, "\xF0\x9F\x97\xBA");
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -16);

    lv_obj_t* info = lv_label_create(holder);
    lv_label_set_text(info, "Offline Maps\n\nLoad .mbtiles or\nraster tiles to SD card\n/maps/ directory");
    lv_obj_set_style_text_color(info, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 30);

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

    // Build settings from live preferences
    int n_items = 0;

    // Node name
    lv_obj_t* btn0 = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "");
    n_items++;
    lv_obj_set_style_bg_color(btn0, lv_color_hex(BG_TERTIARY), 0);
    // We need access to prefs — use hardcoded for now, TODO: expose prefs getters

    lv_obj_t* btn1 = lv_list_add_btn(list, LV_SYMBOL_WIFI, "  Frequency: 869.618 MHz");
    lv_obj_set_style_bg_color(btn1, lv_color_hex(BG_TERTIARY), 0);

    lv_obj_t* btn2 = lv_list_add_btn(list, LV_SYMBOL_SHUFFLE, "  Spreading Factor: SF8");
    lv_obj_set_style_bg_color(btn2, lv_color_hex(BG_TERTIARY), 0);

    lv_obj_t* btn3 = lv_list_add_btn(list, LV_SYMBOL_BATTERY_FULL, "  Power: 22 dBm");
    lv_obj_set_style_bg_color(btn3, lv_color_hex(BG_TERTIARY), 0);

    lv_obj_t* btn4 = lv_list_add_btn(list, LV_SYMBOL_SD_CARD, "  SD Card: Not mounted");
    lv_obj_set_style_bg_color(btn4, lv_color_hex(BG_TERTIARY), 0);

    lv_obj_t* btn5 = lv_list_add_btn(list, LV_SYMBOL_GPS, "  GPS: NMEA 38400 baud");
    lv_obj_set_style_bg_color(btn5, lv_color_hex(BG_TERTIARY), 0);

    lv_obj_t* btn6 = lv_list_add_btn(list, LV_SYMBOL_HOME, "  About SlopOS beta-0.1.6");
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
            snprintf(result, sizeof(result), "Commands: help status advert ping\\n");
        } else if (strcmp(cmd, "status") == 0) {
            int rssi = slopos::mesh::getLastRSSI();
            float snr = slopos::mesh::getLastSNR();
            int noise = slopos::mesh::getNoiseFloor();
            int contacts = slopos::mesh::getContactCount();
            int channels = slopos::mesh::getChannelCount();
            snprintf(result, sizeof(result),
                "RSSI:%ddBm SNR:%.1f Noise:%ddBm\\n"
                "Contacts:%d Channels:%d\\n",
                rssi, snr, noise, contacts, channels);
        } else if (strcmp(cmd, "advert") == 0) {
            bool ok = slopos::mesh::sendAdvert();
            snprintf(result, sizeof(result), ok ? "Advert sent\\n" : "Send failed\\n");
        } else if (strcmp(cmd, "ping") == 0) {
            snprintf(result, sizeof(result), "Pong! Uptime: %lu ms\\n", millis());
        } else {
            snprintf(result, sizeof(result), "Unknown: %s\\nType 'help'\\n", cmd);
        }

        lv_textarea_add_text(term, result);
        lv_textarea_set_text(ta, "");  // clear input
    }, LV_EVENT_READY, term);  // LV_EVENT_READY fires on Enter in textarea

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Trace — path trace tool
// ════════════════════════════════════════════════════════
void trace_screen_show()
{
    lv_obj_t* scr = make_screen("Trace Route");

    lv_obj_t* info = lv_label_create(scr);
    lv_label_set_text(info,
        "Trace Route Tool\n\n"
        "Send a trace packet to\n"
        "map the path through the\n"
        "mesh network.\n\n"
        "Select a target node\n"
        "to begin.");
    lv_obj_set_style_text_color(info, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 8, 30);

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
            "Device Finder\\n\\n"
            "Scanning for nearby\\n"
            "MeshCore devices...\\n\\n"
            "No devices found yet.\\n"
            "Check antenna and\\n"
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

} // namespace slopos::ui
