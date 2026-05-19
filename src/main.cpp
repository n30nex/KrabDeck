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
    Serial.println("[boot] step 1: serial OK");

    board.begin();
    Serial.println("[boot] step 2: board init OK");
    slopos_battery_init();

    bool spiffs_ok = SPIFFS.begin(true);
    if (!spiffs_ok)
        Serial.println("[boot] WARNING: SPIFFS mount failed — identity/contacts won't persist across reboots");
    else
        Serial.println("[boot] step 3: SPIFFS mounted");

    slopos_gps_init();
    Serial.println("[boot] step 4: GPS init done");

    if (!slopos_sdcard_init())
        Serial.println("[boot] INFO: No SD card detected");
    else
        Serial.println("[boot] step 5: SD card mounted");

    if (!slopos_display_init()) {
        Serial.println("[boot] FATAL: Display init failed");
        while (1) delay(1000);
    }
    Serial.println("[boot] step 6: display init OK");

    slopos::mesh::setOwnName("SlopOS T-Deck");
    if (!slopos::mesh::init(spiffs_ok))
        Serial.println("[boot] WARNING: Radio init failed");
    else
        Serial.println("[boot] step 7: mesh radio initialized");

    slopos::ui::init();
    Serial.println("[boot] step 8: UI splash screen shown");

    if (slopos_sdcard_mounted())
        slopos_map_init();

    Serial.println("[boot] === SlopOS T-Deck ready ===");
}

void loop()
{
    // Low-battery auto-shutdown (matches MeshCore pattern)
    static uint32_t last_batt_check = 0;
    if (millis() - last_batt_check > 30000) {  // every 30s
        last_batt_check = millis();
        if (board.isBatteryCritical()) {
            Serial.println("CRITICAL: Battery low — entering deep sleep");
            board.sleep(0);  // sleep indefinitely until charged
            return;
        }
    }

    slopos::mesh::loop();
    slopos_gps_loop();
    slopos_display_loop();
    slopos::ui::loop();
}
