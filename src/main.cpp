// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <Arduino.h>
#include <SPIFFS.h>
#include "hal/tdeck_pins.h"
#include "hal/tdeck_board.h"
#include "hal/display.h"
#include "hal/battery.h"
#include "hal/gps.h"
#include "hal/sdcard.h"
#include "app/map_renderer.h"
#include "mesh/mesh_wrapper.h"
#include "ui/ui.h"

static slopos::TDeckBoard board;

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("SlopOS T-Deck — booting...");

    board.begin();
    slopos_battery_init();

    if (!SPIFFS.begin(true))
        Serial.println("WARNING: SPIFFS mount failed");
    else
        Serial.println("SPIFFS mounted");

    slopos_gps_init();

    if (!slopos_sdcard_init())
        Serial.println("INFO: No SD card detected");
    else
        Serial.println("SD card mounted");

    if (!slopos_display_init()) {
        Serial.println("FATAL: Display init failed");
        while (1) delay(1000);
    }

    slopos::mesh::setOwnName("SlopOS T-Deck");
    if (!slopos::mesh::init())
        Serial.println("WARNING: Radio init failed");
    else
        Serial.println("MeshCore radio initialized");

    slopos::ui::init();

    if (slopos_sdcard_mounted())
        slopos_map_init();

    Serial.println("SlopOS T-Deck ready");
}

void loop()
{
    slopos::mesh::loop();
    slopos_gps_loop();
    slopos_display_loop();
    slopos::ui::loop();
}
