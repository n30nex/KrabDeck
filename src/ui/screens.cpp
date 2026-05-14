#include "screens.h"
#include "navigation.h"
#include "theme.h"
#include "../hal/tdeck_pins.h"
#include "../mesh/mesh_wrapper.h"
#include <Arduino.h>
#include <lvgl.h>
#include <cstdio>

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

    // Sample nodes (in production: populate from mesh_wrapper)
    struct { const char* name; int rssi; int ago; } nodes[] = {
        {"Alice",       -72,  12},
        {"Bob",         -85,  45},
        {"Charlie",     -98, 120},
        {"Repeater-01", -65,   3},
        {"Dave",       -105, 300},
        {"Eve",         -91,  60},
        {"RoomServer",  -78,  15},
    };

    char buf[80];
    for (auto& n : nodes) {
        snprintf(buf, sizeof(buf), "%s  %ddBm  %ds ago", n.name, n.rssi, n.ago);
        lv_obj_t* item = lv_list_add_btn(list, nullptr, buf);
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_30, 0);
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

    const char* contacts[] = {"Alice", "Bob", "Charlie", "Repeater-01", "RoomServer"};
    for (auto& c : contacts) {
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_FILE, c);
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Signal — radio signal strength details
// ════════════════════════════════════════════════════════
void signal_screen_show()
{
    lv_obj_t* scr = make_screen("Signal");

    int rssi = slopos::mesh::get_last_rssi();
    float snr = slopos::mesh::get_last_snr();
    int noise = slopos::mesh::get_noise_floor();

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

    int noise = slopos::mesh::get_noise_floor();
    int rssi = slopos::mesh::get_last_rssi();

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

    const char* items[] = {
        LV_SYMBOL_SETTINGS "  Node Name: TDeck+",
        LV_SYMBOL_WIFI    "  Frequency: 869.618 MHz",
        LV_SYMBOL_SHUFFLE "  Spreading Factor: SF8",
        LV_SYMBOL_BATTERY_FULL "  Power: 22 dBm",
        LV_SYMBOL_SD_CARD "  SD Card: Not mounted",
        LV_SYMBOL_GPS     "  GPS: 38400 baud",
        LV_SYMBOL_HOME    "  About SlopOS v0.1.0",
    };
    for (auto& item : items) {
        lv_obj_t* btn = lv_list_add_btn(list, nullptr, item);
        lv_obj_set_style_bg_color(btn, lv_color_hex(BG_TERTIARY), 0);
    }

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

    struct { const char* name; int hops; int rssi; } reps[] = {
        {"Hilltop-Rptr",   1, -85},
        {"Church-Rptr",    2, -98},
        {"Valley-Rptr",    1, -72},
    };
    char buf[64];
    for (auto& r : reps) {
        snprintf(buf, sizeof(buf), "%s  [%d hop%s]  %ddBm",
                 r.name, r.hops, r.hops == 1 ? "" : "s", r.rssi);
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_LOOP, buf);
        lv_obj_set_style_bg_color(item, lv_color_hex(BG_TERTIARY), 0);
    }

    show_screen(scr);
}

// ════════════════════════════════════════════════════════
// Finder — device discovery
// ════════════════════════════════════════════════════════
void finder_screen_show()
{
    lv_obj_t* scr = make_screen("Finder");

    lv_obj_t* info = lv_label_create(scr);
    lv_label_set_text(info,
        "Device Finder\n\n"
        "Scanning for nearby\n"
        "MeshCore devices...\n\n"
        "This may take a few\n"
        "seconds.");
    lv_obj_set_style_text_color(info, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 8, 30);

    // Scan indicator
    lv_obj_t* spinner = lv_spinner_create(scr);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(ACCENT), 0);

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

    show_screen(scr);
}

} // namespace slopos::ui
