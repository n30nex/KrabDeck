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


#include <Arduino.h>
#include <SPIFFS.h>
#include "hal/tdeck_pins.h"
#include "hal/tdeck_board.h"
#include "hal/prefs.h"
#include "hal/display.h"
#include "hal/battery.h"
#include "hal/gps.h"
#include "hal/sdcard.h"
#include "app/map_renderer.h"
#include "mesh/mesh_wrapper.h"
#include "ui/ui.h"

static slopos::TDeckBoard board;
static slopos::NodePrefs prefs;

// ── SPIFFS persistence ──────────────────────────────
static const char* PREFS_PATH = "/slopos.prefs";

static void prefs_load() {
    prefs.set_defaults();
    if (!SPIFFS.exists(PREFS_PATH)) return;

    File f = SPIFFS.open(PREFS_PATH, "r");
    if (!f) return;
    if (f.readBytes((char*)&prefs, sizeof(prefs)) != sizeof(prefs)) {
        prefs.set_defaults();
    }
    f.close();
    Serial.printf("Loaded prefs: name=%s freq=%.3f SF=%d\n",
                  prefs.node_name, prefs.freq, prefs.sf);
}

static void prefs_save() {
    File f = SPIFFS.open(PREFS_PATH, "w");
    if (!f) return;
    f.write((uint8_t*)&prefs, sizeof(prefs));
    f.close();
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("SlopOS T-Deck — booting...");

    // ── Board init ──────────────────────────────────
    board.begin();
    slopos_battery_init();

    // ── SPIFFS mount ────────────────────────────────
    if (!SPIFFS.begin(true)) {
        Serial.println("WARNING: SPIFFS mount failed — preferences not persisted");
    } else {
        Serial.println("SPIFFS mounted");
        prefs_load();
    }

    // ── GPS module ───────────────────────────────────
    slopos_gps_init();

    // ── SD card ──────────────────────────────────────
    if (!slopos_sdcard_init()) {
        Serial.println("INFO: No SD card detected");
    } else {
        Serial.println("SD card mounted");
    }

    // ── Display + LVGL ──────────────────────────────
    if (!slopos_display_init()) {
        Serial.println("FATAL: Display init failed");
        while (1) delay(1000);
    }

    // ── MeshCore networking ─────────────────────────
    slopos::mesh::set_own_name(prefs.node_name);
    if (!slopos::mesh::init()) {
        Serial.println("WARNING: Radio init failed — mesh disabled");
    } else {
        Serial.println("MeshCore radio initialized");
    }

    // ── UI splash screen ────────────────────────────
    slopos::ui::init();

    // ── Map renderer ──────────────────────────────────
    if (slopos_sdcard_mounted()) {
        slopos_map_init();
    }

    // Persist prefs after boot (ensures SPIFFS is working)
    prefs_save();

    Serial.println("SlopOS T-Deck ready");
}

void loop()
{
    // Mesh networking tick
    slopos::mesh::loop();

    // GPS NMEA parsing
    slopos_gps_loop();

    // LVGL UI rendering (includes auto-off timer)
    slopos_display_loop();

    // UI logic (screen transitions, etc.)
    slopos::ui::loop();
}
