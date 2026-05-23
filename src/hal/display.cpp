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


#include "display.h"
#include "touch.h"
#include "keyboard.h"
#include "tdeck_pins.h"
#include <lvgl.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ── LovyanGFX configuration for T-Deck ST7789 ────────────
class LGFX_SlopOS : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;

public:
    LGFX_SlopOS()
    {
        {
            auto cfg = _bus.config();
            cfg.spi_host   = SPI3_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = PIN_TFT_SCL;
            cfg.pin_mosi   = PIN_TFT_SDA;
            cfg.pin_miso   = -1;
            cfg.pin_dc     = PIN_TFT_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs    = PIN_TFT_CS;
            cfg.pin_rst   = PIN_TFT_RST;
            cfg.panel_width  = 240;   // ST7789 native portrait
            cfg.panel_height = 320;
            cfg.offset_x  = 0;
            cfg.offset_y  = 0;
            cfg.offset_rotation = 0;
            cfg.invert    = true;
            cfg.rgb_order = false;
            cfg.memory_width  = 240;  // must match native portrait
            cfg.memory_height = 320;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl = PIN_TFT_BL;
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 0;
            _light.config(cfg);
        }
        _panel.setLight(&_light);
        setPanel(&_panel);
    }
};

static LGFX_SlopOS tft;
static lv_display_t* lv_disp = nullptr;
static constexpr int LVGL_DRAW_BUF_LINES = 20;  // smaller = less RAM, 20 lines = 12.5KB
static lv_color_t draw_buf[TFT_WIDTH * LVGL_DRAW_BUF_LINES];

// ── Auto-off timer ──────────────────────────────────
// Based on MeshCore's AUTO_OFF_MILLIS pattern (MIT license)
static constexpr uint32_t AUTO_OFF_MS  = 30000;  // 30 seconds
static uint32_t            auto_off_at = 0;
static bool                display_on  = true;
static bool                wake_refresh_pending = false;

static void reset_auto_off() {
    auto_off_at = millis() + AUTO_OFF_MS;
}

static void restore_display_after_sleep()
{
    tft.setRotation(1);  // Reassert ST7789 landscape state after display auto-off.
    tft.fillScreen(TFT_BLACK);

    lv_obj_t* active = lv_scr_act();
    if (active) {
        lv_obj_invalidate(active);
    }
}

// ── LVGL flush callback ─────────────────────────────────
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((lgfx::rgb565_t*)px_map, w * h);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

// ── Touch read callback ──────────────────────────────────
static void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    int x, y;
    bool pressed = false;
    if (slopos_touch_get(&x, &y, &pressed) && pressed) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        slopos_display_wake();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ── Keyboard read callback ───────────────────────────────
static void lvgl_kb_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    int key = slopos_keyboard_get_key();
    if (key > 0 && slopos_keyboard_has_new_event()) {
        data->key = (uint32_t)key;
        data->state = LV_INDEV_STATE_PRESSED;
        slopos_display_wake();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ── Public API ───────────────────────────────────────────
bool slopos_display_init()
{
    tft.init();
    tft.setRotation(1);  // 90° CW: native portrait (240×320) → landscape (320×240)
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    lv_init();
    lv_tick_set_cb(slopos_display_millis);
    lv_disp = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    lv_display_set_buffers(lv_disp, draw_buf, nullptr, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, lvgl_touch_cb);

    lv_indev_t* kb = lv_indev_create();
    lv_indev_set_type(kb, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(kb, lvgl_kb_cb);

    // Set default group so keyboard input reaches focused widgets
    lv_group_t* g = lv_group_create();
    lv_indev_set_group(kb, g);
    lv_group_set_default(g);

    // Initialize touch controller
    if (!slopos_touch_init()) {
        // Touch init failed — device works with keyboard only
    }

    // Initialize keyboard matrix scanner
    if (!slopos_keyboard_init()) {
        // Keyboard init failed — device works with touch only
    }

    display_on = true;
    reset_auto_off();

    // Backlight pulse: brief off→on to confirm display is alive
    tft.setBrightness(0);
    delay(50);
    tft.setBrightness(255);

    return true;
}

void slopos_display_loop()
{
    slopos_touch_loop();
    slopos_keyboard_scan();

    if (wake_refresh_pending) {
        wake_refresh_pending = false;
        restore_display_after_sleep();
    }

    // Auto-off: turn off backlight after inactivity
    if (display_on && millis() > auto_off_at) {
        tft.setBrightness(0);
        display_on = false;
    }

    uint32_t next = lv_timer_handler();
    delay(next > 5 ? 5 : next);
}

uint32_t slopos_display_millis()
{
    return millis();
}

void slopos_display_wake()
{
    if (!display_on) {
        tft.setBrightness(255);
        display_on = true;
        wake_refresh_pending = true;
    }
    reset_auto_off();
}

bool slopos_display_is_on()
{
    return display_on;
}
