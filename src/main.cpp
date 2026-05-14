#include <Arduino.h>
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

    // ── Board init ──────────────────────────────────
    board.begin();
    slopos_battery_init();

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

    Serial.println("SlopOS T-Deck ready");
}

void loop()
{
    // Mesh networking tick
    slopos::mesh::loop();

    // GPS NMEA parsing
    slopos_gps_loop();

    // LVGL UI rendering
    slopos_display_loop();

    // UI logic (screen transitions, etc.)
    slopos::ui::loop();
}
