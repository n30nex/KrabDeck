// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SigurdOS.
//
// SigurdOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SigurdOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SigurdOS.  If not, see <https://www.gnu.org/licenses/>.

#include "../screens.h"
#include "../screens_common.h"
#include "../theme.h"
#include "../responsive.h"
#include "../screen_lifetime.h"
#include "../../mesh/mesh_wrapper.h"
#include "../../app/map_renderer.h"
#include "../../app/map_download.h"
#include "../../app/gps_track_log.h"
#include "../../hal/gps.h"
#include "../../hal/prefs.h"
#include "../../hal/sdcard.h"
#include <Arduino.h>
#include <lvgl.h>
#include <algorithm>
#include <cstring>
#include <new>

namespace sigurdos::ui {

using namespace theme;
using namespace responsive;

// ── Forward declarations for trackball handler ──
static void render_map_with_contacts();
static void update_download_dialog();
static bool g_map_download_dialog_open = false;

// ════════════════════════════════════════════════════════
// Map — trackball pan navigation
// ════════════════════════════════════════════════════════

bool map_screen_handle_trackball(SigurdOSTrackballEvent event) {
    if (g_map_download_dialog_open) {
        lv_group_t* group = lv_group_get_default();
        if (!group) return true;
        if (event == SigurdOSTrackballEvent::Up ||
            event == SigurdOSTrackballEvent::Left) {
            lv_group_focus_prev(group);
        } else if (event == SigurdOSTrackballEvent::Down ||
                   event == SigurdOSTrackballEvent::Right) {
            lv_group_focus_next(group);
        } else if (event == SigurdOSTrackballEvent::Click) {
            lv_group_send_data(group, LV_KEY_ENTER);
        }
        return true;
    }
    const int PAN_PX = 12;  // pixels per trackball tick
    switch (event) {
        case SigurdOSTrackballEvent::Up:
            sigurdos_map_pan(0, -PAN_PX);
            render_map_with_contacts();
            return true;
        case SigurdOSTrackballEvent::Down:
            sigurdos_map_pan(0, PAN_PX);
            render_map_with_contacts();
            return true;
        case SigurdOSTrackballEvent::Left:
            sigurdos_map_pan(-PAN_PX, 0);
            render_map_with_contacts();
            return true;
        case SigurdOSTrackballEvent::Right:
            sigurdos_map_pan(PAN_PX, 0);
            render_map_with_contacts();
            return true;
        case SigurdOSTrackballEvent::Click:
            sigurdos_map_zoom_in();
            render_map_with_contacts();
            return true;
        default:
            return false;
    }
}

// ════════════════════════════════════════════════════════
// Map — offline tile maps
// ════════════════════════════════════════════════════════

// Helper: render map tiles then overlay contact markers
static sigurdos::mesh::ContactInfo* map_contacts = nullptr;
static lv_timer_t* g_map_tile_timer = nullptr;
static lv_timer_t* g_map_warmup_timer = nullptr;
static lv_timer_t* g_map_discovery_timer = nullptr;
static lv_obj_t* g_map_discovery_status = nullptr;
static lv_obj_t* g_map_discovery_cancel = nullptr;
static lv_obj_t* g_map_track_status = nullptr;
static lv_obj_t* g_map_track_toggle = nullptr;
static lv_timer_t* g_map_track_timer = nullptr;
static lv_timer_t* g_map_download_timer = nullptr;
static lv_obj_t* g_map_download_dialog = nullptr;
static lv_obj_t* g_map_download_status = nullptr;
static lv_obj_t* g_map_download_action = nullptr;
static lv_obj_t* g_map_download_action_label = nullptr;
static sigurdos::app::GpsTrackPoint* g_map_track_points = nullptr;
static std::size_t g_map_track_count = 0;
static std::size_t g_map_track_total_count = 0;
static int g_map_warmup_passes = 0;
static ScreenLifetime g_map_lifetime;

static bool read_sd_text(const char* path, char* out, size_t capacity)
{
    if (!out || capacity < 2) return false;
    const size_t length = sigurdos_sdcard_read(
        path, reinterpret_cast<uint8_t*>(out), capacity - 1);
    if (length == 0 || length >= capacity) return false;
    out[length] = '\0';
    while (out[0] && (out[std::strlen(out) - 1] == '\r' ||
                      out[std::strlen(out) - 1] == '\n')) {
        out[std::strlen(out) - 1] = '\0';
    }
    return out[0] != '\0';
}

static sigurdos::app::map_download::Bounds download_bounds()
{
    const double lat = sigurdos_map_get_lat();
    const double lon = sigurdos_map_get_lon();
    return {
        std::max(-85.05112878, lat - 0.05),
        std::max(-180.0, lon - 0.05),
        std::min(85.05112878, lat + 0.05),
        std::min(180.0, lon + 0.05),
    };
}

static void add_dialog_button(lv_obj_t* button)
{
    apply_pixel_btn_outline(button);
    lv_group_t* group = lv_group_get_default();
    if (group) lv_group_add_obj(group, button);
}

static void update_download_dialog()
{
    if (!g_map_download_dialog_open || !g_map_download_status ||
        !g_map_download_action_label) {
        return;
    }
    using namespace sigurdos::app::map_download;
    const Status current = status();
    const uint32_t done = current.completed + current.skipped + current.failed;
    char free_space[20];
    sigurdos_sdcard_format_size(
        current.free_bytes, free_space, sizeof(free_space));
    lv_label_set_text_fmt(
        g_map_download_status, "%s\n%lu / %lu tiles  free %s\n%s",
        stateName(current.state), static_cast<unsigned long>(done),
        static_cast<unsigned long>(current.total), free_space,
        current.detail[0] ? current.detail : "Ready");
    if (current.state == State::Running) {
        lv_label_set_text(g_map_download_action_label, "PAUSE");
        lv_obj_clear_flag(g_map_download_action, LV_OBJ_FLAG_HIDDEN);
    } else if (current.state == State::Paused) {
        lv_label_set_text(g_map_download_action_label, "RESUME");
        lv_obj_clear_flag(g_map_download_action, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_map_download_action, LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_download_dialog(lv_obj_t* screen)
{
    if (!g_map_download_dialog) {
        g_map_download_dialog = lv_obj_create(screen);
        lv_obj_set_size(g_map_download_dialog, 304, 190);
        lv_obj_center(g_map_download_dialog);
        apply_pixel_card_accent(g_map_download_dialog);
        lv_obj_set_style_bg_opa(g_map_download_dialog, LV_OPA_COVER, 0);

        lv_obj_t* title = lv_label_create(g_map_download_dialog);
        lv_label_set_text(title, "OFFLINE MAPS");
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 6, 2);

        g_map_download_status = lv_label_create(g_map_download_dialog);
        lv_obj_set_width(g_map_download_status, 282);
        lv_obj_set_style_text_color(
            g_map_download_status, lv_color_hex(TEXT_SECONDARY), 0);
        lv_obj_align(g_map_download_status, LV_ALIGN_TOP_LEFT, 6, 24);

        lv_obj_t* nrcan = lv_btn_create(g_map_download_dialog);
        lv_obj_set_size(nrcan, 82, 34);
        lv_obj_align(nrcan, LV_ALIGN_BOTTOM_LEFT, 4, -42);
        add_dialog_button(nrcan);
        lv_obj_t* nrcan_label = lv_label_create(nrcan);
        lv_label_set_text(nrcan_label, "NRCAN");
        lv_obj_center(nrcan_label);
        lv_obj_add_event_cb(nrcan, [](lv_event_t*) {
            sigurdos::app::map_download::startNrCan(
                download_bounds(), 10, 15);
            update_download_dialog();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* custom = lv_btn_create(g_map_download_dialog);
        lv_obj_set_size(custom, 82, 34);
        lv_obj_align(custom, LV_ALIGN_BOTTOM_MID, 0, -42);
        add_dialog_button(custom);
        lv_obj_t* custom_label = lv_label_create(custom);
        lv_label_set_text(custom_label, "CUSTOM");
        lv_obj_center(custom_label);
        lv_obj_add_event_cb(custom, [](lv_event_t*) {
            char url[sigurdos::app::map_download::MAX_URL_BYTES + 1];
            char attribution[96];
            if (read_sd_text("/tiles/provider.url", url, sizeof(url)) &&
                read_sd_text("/tiles/provider.attribution", attribution,
                             sizeof(attribution))) {
                sigurdos::app::map_download::startXyz(
                    download_bounds(), 10, 15, "Custom offline map",
                    attribution, url);
            }
            update_download_dialog();
        }, LV_EVENT_CLICKED, nullptr);

        g_map_download_action = lv_btn_create(g_map_download_dialog);
        lv_obj_set_size(g_map_download_action, 82, 34);
        lv_obj_align(g_map_download_action, LV_ALIGN_BOTTOM_RIGHT, -4, -42);
        add_dialog_button(g_map_download_action);
        g_map_download_action_label = lv_label_create(g_map_download_action);
        lv_obj_center(g_map_download_action_label);
        lv_obj_add_event_cb(g_map_download_action, [](lv_event_t*) {
            using namespace sigurdos::app::map_download;
            if (status().state == State::Running) pause();
            else resume();
            update_download_dialog();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* cancel = lv_btn_create(g_map_download_dialog);
        lv_obj_set_size(cancel, 82, 34);
        lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 4, -2);
        add_dialog_button(cancel);
        lv_obj_t* cancel_label = lv_label_create(cancel);
        lv_label_set_text(cancel_label, "CANCEL");
        lv_obj_center(cancel_label);
        lv_obj_add_event_cb(cancel, [](lv_event_t*) {
            sigurdos::app::map_download::cancel();
            update_download_dialog();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* close = lv_btn_create(g_map_download_dialog);
        lv_obj_set_size(close, 82, 34);
        lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
        add_dialog_button(close);
        lv_obj_t* close_label = lv_label_create(close);
        lv_label_set_text(close_label, "CLOSE");
        lv_obj_center(close_label);
        lv_obj_add_event_cb(close, [](lv_event_t*) {
            g_map_download_dialog_open = false;
            lv_obj_add_flag(g_map_download_dialog, LV_OBJ_FLAG_HIDDEN);
        }, LV_EVENT_CLICKED, nullptr);
    }
    g_map_download_dialog_open = true;
    lv_obj_clear_flag(g_map_download_dialog, LV_OBJ_FLAG_HIDDEN);
    update_download_dialog();
}

static void update_track_status()
{
    if (!g_map_track_status || !g_map_track_toggle) return;
    const sigurdos::NodePrefs& prefs = sigurdos::prefs_get();
    lv_obj_t* toggle_label = lv_obj_get_child(g_map_track_toggle, 0);
    if (toggle_label) {
        lv_label_set_text(toggle_label,
                          prefs.gps_track_enabled ? "REC ON" : "REC OFF");
    }
    sigurdos::app::GpsTrackStats stats{};
    if (!sigurdos::app::gpsTrackGetStats(&stats)) {
        lv_label_set_text(g_map_track_status, "Track unavailable");
        return;
    }
    g_map_track_total_count = stats.waypoint_count;
    char distance[20];
    if (stats.distance_m >= 1000.0) {
        snprintf(distance, sizeof(distance), "%.1f km", stats.distance_m / 1000.0);
    } else {
        snprintf(distance, sizeof(distance), "%.0f m", stats.distance_m);
    }
    const unsigned long hours = stats.duration_s / 3600UL;
    const unsigned long minutes = (stats.duration_s % 3600UL) / 60UL;
    lv_label_set_text_fmt(
        g_map_track_status, "%u pts  %s  %lu:%02lu",
        static_cast<unsigned>(stats.waypoint_count), distance, hours, minutes);
}

static void render_map_with_contacts() {
    sigurdos_map_render();
    if (!g_map_track_points) {
        g_map_track_points = new(std::nothrow)
            sigurdos::app::GpsTrackPoint[sigurdos::app::GPS_TRACK_MAP_POINTS];
    }
    g_map_track_count = g_map_track_points
        ? sigurdos::app::gpsTrackLoadRecent(
              g_map_track_points, sigurdos::app::GPS_TRACK_MAP_POINTS)
        : 0;
    sigurdos_map_track_render(g_map_track_points, g_map_track_count);
    if (!map_contacts) {
        map_contacts = new(std::nothrow) sigurdos::mesh::ContactInfo[MAX_CONTACTS];
    }
    if (!map_contacts) {
        sigurdos_map_contact_render(nullptr, 0);
        return;
    }
    int n = sigurdos::mesh::exportContactsFull(map_contacts, MAX_CONTACTS);
    if (n < 0) n = 0;
    if (n > MAX_CONTACTS) n = MAX_CONTACTS;
    sigurdos_map_contact_render(map_contacts, n);
}

static void start_map_warmup() {
    g_map_warmup_passes = 3;
    g_map_warmup_timer = lv_timer_create([](lv_timer_t* timer) {
        render_map_with_contacts();
        if (sigurdos_map_last_deferred_tiles() == 0 ||
            --g_map_warmup_passes <= 0) {
            lv_timer_del(timer);
            if (g_map_warmup_timer == timer) g_map_warmup_timer = nullptr;
        }
    }, 250, nullptr);
}

static void hide_discovery_controls() {
    if (g_map_discovery_status) {
        lv_obj_add_flag(g_map_discovery_status, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_map_discovery_cancel) {
        lv_obj_add_flag(g_map_discovery_cancel, LV_OBJ_FLAG_HIDDEN);
    }
}

void map_screen_show()
{
    // Opening Map is an explicit foreground request for responsive location.
    // It does not mutate the persisted background GPS preference.
    sigurdos_gps_set_map_high_rate(true);
    lv_obj_t* scr = make_screen_full("Map");
    g_map_lifetime.bind(scr);
    g_map_lifetime.trackTimer(&g_map_tile_timer);
    g_map_lifetime.trackTimer(&g_map_warmup_timer);
    g_map_lifetime.trackTimer(&g_map_discovery_timer);
    g_map_lifetime.trackTimer(&g_map_track_timer);
    g_map_lifetime.trackTimer(&g_map_download_timer);
    g_map_lifetime.track(&g_map_discovery_status);
    g_map_lifetime.track(&g_map_discovery_cancel);
    g_map_lifetime.track(&g_map_track_status);
    g_map_lifetime.track(&g_map_track_toggle);
    g_map_lifetime.onDelete([] {
        sigurdos_map_cancel_discovery();
        sigurdos_gps_set_map_high_rate(false);
        delete[] map_contacts;
        map_contacts = nullptr;
        delete[] g_map_track_points;
        g_map_track_points = nullptr;
        g_map_track_count = 0;
        g_map_track_total_count = 0;
        g_map_warmup_passes = 0;
        g_map_download_dialog = nullptr;
        g_map_download_status = nullptr;
        g_map_download_action = nullptr;
        g_map_download_action_label = nullptr;
        g_map_download_dialog_open = false;
    });

    // Create the map overlay container before initializing contacts
    lv_obj_t* map = lv_obj_create(scr);
    lv_obj_set_size(map, DISPLAY_W, CONTENT_H);
    lv_obj_align(map, LV_ALIGN_TOP_MID, 0, CONTENT_Y);
    lv_obj_set_style_bg_opa(map, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(map, 0, 0);
    lv_obj_add_flag(map, LV_OBJ_FLAG_CLICKABLE);

    sigurdos_map_init();
    if (!sigurdos_map_initialized()) {
        lv_obj_t* error = lv_label_create(map);
        lv_label_set_text(error, "Map unavailable\nPSRAM allocation failed");
        lv_obj_set_style_text_color(
            error, lv_color_hex(sigurdos::theme::TEXT_SECONDARY), 0);
        lv_obj_center(error);
        show_screen(scr);
        return;
    }
    sigurdos_map_reparent(scr);

    // Pre-allocate contact marker dots on top of map BEFORE rendering
    sigurdos_map_contact_init(map, CONTENT_Y);
    sigurdos_map_contact_set_tap_cb(navigate_to_contact_detail);

    render_map_with_contacts();
    g_map_tile_timer = lv_timer_create([](lv_timer_t*) {
        if (sigurdos_map_process_tile_completions()) {
            render_map_with_contacts();
        }
    }, 20, nullptr);

    g_map_track_toggle = lv_btn_create(map);
    lv_obj_set_size(g_map_track_toggle, 72, 28);
    lv_obj_align(g_map_track_toggle, LV_ALIGN_TOP_LEFT, 8, 8);
    apply_pixel_btn_outline(g_map_track_toggle);
    lv_obj_t* track_toggle_label = lv_label_create(g_map_track_toggle);
    lv_obj_center(track_toggle_label);
    lv_obj_add_event_cb(g_map_track_toggle, [](lv_event_t*) {
        sigurdos::NodePrefs prefs = sigurdos::prefs_get();
        prefs.gps_track_enabled = !prefs.gps_track_enabled;
        if (sigurdos::prefs_set(prefs)) update_track_status();
    }, LV_EVENT_CLICKED, nullptr);

    g_map_track_status = lv_label_create(map);
    lv_obj_set_style_text_color(
        g_map_track_status, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_bg_color(
        g_map_track_status, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(g_map_track_status, LV_OPA_90, 0);
    lv_obj_set_style_pad_all(g_map_track_status, 4, 0);
    lv_obj_align(g_map_track_status, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    update_track_status();

    g_map_track_timer = lv_timer_create([](lv_timer_t*) {
        const std::size_t previous_count = g_map_track_total_count;
        update_track_status();
        if (g_map_track_total_count != previous_count) {
            render_map_with_contacts();
        }
    }, 2000, nullptr);

    lv_obj_t* maps_button = lv_btn_create(map);
    lv_obj_set_size(maps_button, 72, 28);
    lv_obj_align(maps_button, LV_ALIGN_TOP_RIGHT, -8, 8);
    apply_pixel_btn_outline(maps_button);
    lv_obj_t* maps_label = lv_label_create(maps_button);
    lv_label_set_text(maps_label, "TILES");
    lv_obj_center(maps_label);
    lv_obj_add_event_cb(maps_button, [](lv_event_t* event) {
        show_download_dialog(
            static_cast<lv_obj_t*>(lv_event_get_user_data(event)));
    }, LV_EVENT_CLICKED, scr);
    lv_group_t* map_group = lv_group_get_default();
    if (map_group) lv_group_add_obj(map_group, maps_button);
    g_map_download_timer = lv_timer_create([](lv_timer_t*) {
        update_download_dialog();
    }, 1000, nullptr);

    static int drag_start_x = 0, drag_start_y = 0;
    static uint32_t map_last_render_ms = 0;

    // Reset drag state on every entry so stale values from a previous visit
    // don't cause a map jump or skip the first re-render.
    lv_obj_add_event_cb(scr, [](lv_event_t*) {
        drag_start_x = 0;
        drag_start_y = 0;
        map_last_render_ms = 0;
    }, LV_EVENT_SCREEN_LOADED, nullptr);

    lv_obj_add_event_cb(map, [](lv_event_t* e) {
        int code = lv_event_get_code(e);
        if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) return;
        lv_indev_t* indev = lv_indev_get_act();
        lv_point_t pt;
        lv_indev_get_point(indev, &pt);
        if (code == LV_EVENT_PRESSED) {
            drag_start_x = pt.x; drag_start_y = pt.y;
        } else if (code == LV_EVENT_PRESSING) {
            int dx = drag_start_x - pt.x;
            int dy = drag_start_y - pt.y;
            drag_start_x = pt.x; drag_start_y = pt.y;
            if (dx != 0 || dy != 0) sigurdos_map_pan(dx, dy);
            uint32_t now = millis();
            if (now - map_last_render_ms >= 200) {
                render_map_with_contacts();
                map_last_render_ms = now;
            }
        } else if (code == LV_EVENT_RELEASED) {
            render_map_with_contacts();
            map_last_render_ms = millis();
        }
    }, LV_EVENT_ALL, nullptr);

    // Zoom buttons (above bottom bar)
    int zoom_y_base = DISPLAY_H - BOT_BAR_H - DIVIDER_H - 8;

    lv_obj_t* zoom_in = lv_btn_create(scr);
    lv_obj_set_size(zoom_in, 32, 32);
    lv_obj_align(zoom_in, LV_ALIGN_BOTTOM_RIGHT, -8, -(BOT_BAR_H + DIVIDER_H + 8));
    lv_obj_set_style_bg_color(zoom_in, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(zoom_in, 0, 0);
    lv_obj_t* zi = lv_label_create(zoom_in);
    lv_label_set_text(zi, "+"); lv_obj_center(zi);
    lv_obj_add_event_cb(zoom_in, [](lv_event_t*) { sigurdos_map_zoom_in(); render_map_with_contacts(); },
                        LV_EVENT_CLICKED, nullptr);

    lv_obj_t* zoom_out = lv_btn_create(scr);
    lv_obj_set_size(zoom_out, 32, 32);
    lv_obj_align(zoom_out, LV_ALIGN_BOTTOM_RIGHT, -8, -(BOT_BAR_H + DIVIDER_H + 48));
    lv_obj_set_style_bg_color(zoom_out, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(zoom_out, 0, 0);
    lv_obj_t* zo = lv_label_create(zoom_out);
    lv_label_set_text(zo, "-"); lv_obj_center(zo);
    lv_obj_add_event_cb(zoom_out, [](lv_event_t*) { sigurdos_map_zoom_out(); render_map_with_contacts(); },
                        LV_EVENT_CLICKED, nullptr);

    g_map_discovery_status = lv_label_create(map);
    lv_label_set_text(g_map_discovery_status, "Scanning map tiles...");
    lv_obj_set_style_text_color(
        g_map_discovery_status, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_bg_color(
        g_map_discovery_status, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(g_map_discovery_status, LV_OPA_90, 0);
    lv_obj_set_style_pad_all(g_map_discovery_status, 4, 0);
    lv_obj_align(g_map_discovery_status, LV_ALIGN_TOP_MID, 0, 8);

    g_map_discovery_cancel = lv_btn_create(map);
    lv_obj_set_size(g_map_discovery_cancel, 72, 28);
    lv_obj_align(g_map_discovery_cancel, LV_ALIGN_TOP_MID, 0, 42);
    apply_pixel_btn_outline(g_map_discovery_cancel);
    lv_obj_t* cancel_label = lv_label_create(g_map_discovery_cancel);
    lv_label_set_text(cancel_label, "CANCEL");
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(g_map_discovery_cancel, [](lv_event_t*) {
        sigurdos_map_cancel_discovery();
        if (g_map_discovery_timer) {
            lv_timer_del(g_map_discovery_timer);
            g_map_discovery_timer = nullptr;
        }
        hide_discovery_controls();
    }, LV_EVENT_CLICKED, nullptr);

    (void)zoom_y_base;
    show_screen(scr);

    // Discovery starts only after the screen has been loaded. Each timer tick
    // is bounded by both directory operations and elapsed time, so LVGL keeps
    // servicing input even with a very large or malformed tile tree.
    sigurdos_map_discover_tiles();
    g_map_discovery_timer = lv_timer_create([](lv_timer_t* timer) {
        const bool more = sigurdos_map_discovery_step();
        const SigurdosMapDiscoveryProgress progress =
            sigurdos_map_discovery_progress();
        if (g_map_discovery_status && more) {
            lv_label_set_text_fmt(
                g_map_discovery_status, "Scanning tiles z%d  %lu",
                progress.zoom,
                static_cast<unsigned long>(progress.entries_processed));
        }
        if (!more) {
            lv_timer_del(timer);
            if (g_map_discovery_timer == timer) {
                g_map_discovery_timer = nullptr;
            }
            if (progress.result == SigurdosMapDiscoveryResult::LimitReached &&
                g_map_discovery_status) {
                lv_label_set_text(g_map_discovery_status,
                                  "Tile scan limit reached");
                if (g_map_discovery_cancel) {
                    lv_obj_add_flag(
                        g_map_discovery_cancel, LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                hide_discovery_controls();
            }
            render_map_with_contacts();
            start_map_warmup();
        }
    }, 20, nullptr);
}

} // namespace sigurdos::ui
