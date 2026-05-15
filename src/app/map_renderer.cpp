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


#include "map_renderer.h"
#include "../hal/tdeck_pins.h"
#include "../hal/sdcard.h"
#include <lvgl.h>
#include <cmath>
#include <cstdio>
#include <SD.h>

#ifdef ESP32_PLATFORM
#include <esp32/rom/tjpgd.h>
#endif

// ── Constants ─────────────────────────────────────────────
static constexpr int TILE_SIZE    = 256;   // standard tile size (pixels)
static constexpr int MAX_ZOOM     = 18;
static constexpr int MIN_ZOOM     = 0;
static constexpr double MAX_LAT   = 85.0511;
static constexpr double MIN_LAT   = -85.0511;
static constexpr double MAX_LON   = 180.0;
static constexpr double MIN_LON   = -180.0;

// Note: PI is defined by Arduino.h as a macro
static lv_obj_t* map_canvas = nullptr;
static lv_draw_buf_t draw_buf;
static uint8_t*   canvas_pixels = nullptr;

static double center_lat = 51.5074;  // London
static double center_lon = -0.1278;
static int    zoom_level = 10;
static bool   initialized = false;

// ── JPEG tile decode buffer (PSRAM on ESP32) ──────────────
static uint16_t* tile_rgb565 = nullptr;  // 256×256×2 = 131KB
static uint8_t*  jpeg_inbuf  = nullptr;  // input buffer (~32KB typical JPEG)
static JDEC       jdec;

// ── Web Mercator helpers ──────────────────────────────────
static double lon_to_tile_x(double lon, int z) {
    double n = (double)(1 << z);
    return (lon + 180.0) / 360.0 * n;
}

static double lat_to_tile_y(double lat, int z) {
    double n = (double)(1 << z);
    double lat_rad = lat * PI / 180.0;
    return (1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / PI) / 2.0 * n;
}

static double tile_x_to_lon(double tx, int z) {
    double n = (double)(1 << z);
    return tx / n * 360.0 - 180.0;
}

static double tile_y_to_lat(double ty, int z) {
    double n = (double)(1 << z);
    return atan(sinh(PI * (1.0 - 2.0 * ty / n))) * 180.0 / PI;
}

static int clamp(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static double clamp_d(double val, double lo, double hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

// ── JPEG decode helpers (TJpgDec callbacks) ───────────────

struct JpegInputCtx {
    File file;
    size_t total;
};

static UINT jpeg_input_func(JDEC* jd, BYTE* buf, UINT nbytes) {
    JpegInputCtx* ctx = (JpegInputCtx*)jd->device;
    if (!ctx || !ctx->file) return 0;
    if (buf) {
        return (UINT)ctx->file.read(buf, nbytes);
    } else {
        // Seek: skip nbytes
        ctx->file.seek(ctx->file.position() + nbytes);
        return nbytes;
    }
}

static UINT jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect) {
    // bitmap points to RGB565 pixel data for the rectangle
    // Store into our tile_rgb565 buffer
    uint16_t* src = (uint16_t*)bitmap;
    uint16_t* dst = tile_rgb565;
    for (int y = rect->top; y <= rect->bottom; y++) {
        for (int x = rect->left; x <= rect->right; x++) {
            dst[y * TILE_SIZE + x] = *src++;
        }
    }
    return 1;  // continue
}

// ── Tile loading ──────────────────────────────────────────

static bool load_tile_jpeg(int zoom, int tx, int ty) {
    if (!tile_rgb565 || !jpeg_inbuf) return false;

    // Try .jpg first, then .png fallback
    char path[64];
    snprintf(path, sizeof(path), "/maps/%d/%d/%d.jpg", zoom, tx, ty);

    if (!SD.exists(path)) {
        // Try alternate filename (some tools use different naming)
        snprintf(path, sizeof(path), "/maps/%d/%d/%d.jpeg", zoom, tx, ty);
        if (!SD.exists(path)) return false;
    }

    JpegInputCtx ctx;
    ctx.file = SD.open(path, FILE_READ);
    if (!ctx.file) return false;
    ctx.total = ctx.file.size();

    // Prepare JPEG decoder
    JRESULT rc = jd_prepare(&jdec, jpeg_input_func, jpeg_inbuf, 4096, &ctx);
    if (rc != JDR_OK) {
        ctx.file.close();
        return false;
    }

    // Decode to tile_rgb565
    rc = jd_decomp(&jdec, jpeg_output_func, 0);  // scale=0 (full size)
    ctx.file.close();

    return (rc == JDR_OK);
}

// ── Draw tile to LVGL layer ───────────────────────────────

static void draw_tile_pixels(lv_layer_t* layer, int screen_x, int screen_y,
                              int tile_w, int tile_h,
                              int src_x, int src_y, int src_w, int src_h) {
    if (!tile_rgb565) return;

    // Clamp to screen bounds
    int x1 = screen_x;
    int y1 = screen_y;
    int x2 = screen_x + tile_w - 1;
    int y2 = screen_y + tile_h - 1;

    if (x1 < 0) { src_x -= x1; src_w += x1; x1 = 0; }
    if (y1 < 0) { src_y -= y1; src_h += y1; y1 = 0; }
    if (x2 >= TFT_WIDTH)  { src_w -= (x2 - TFT_WIDTH + 1);  x2 = TFT_WIDTH - 1; }
    if (y2 >= TFT_HEIGHT) { src_h -= (y2 - TFT_HEIGHT + 1); y2 = TFT_HEIGHT - 1; }
    if (src_w <= 0 || src_h <= 0) return;
    if (src_x < 0) src_x = 0;
    if (src_y < 0) src_y = 0;

    // Draw pixel by pixel through LVGL image descriptor
    // For each row, draw as a horizontal line of pixels
    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    img_dsc.opa = LV_OPA_COVER;

    lv_area_t area;
    area.x1 = x1;
    area.y1 = y1;
    area.x2 = x2;
    area.y2 = y2;

    // Create a temporary image buffer for the visible portion
    size_t px_count = (size_t)(area.x2 - area.x1 + 1) * (area.y2 - area.y1 + 1);
    size_t buf_size = px_count * 2;  // RGB565 = 2 bytes/pixel

    uint16_t* img_buf = (uint16_t*)lv_malloc(buf_size);
    if (!img_buf) return;

    // Copy visible pixels from tile buffer
    int idx = 0;
    for (int y = src_y; y < src_y + src_h; y++) {
        for (int x = src_x; x < src_x + src_w; x++) {
            img_buf[idx++] = tile_rgb565[y * TILE_SIZE + x];
        }
    }

    lv_image_dsc_t img;
    img.header.w = (uint32_t)(src_w);
    img.header.h = (uint32_t)(src_h);
    img.header.stride = (uint32_t)(src_w * 2);
    img.header.cf = LV_COLOR_FORMAT_RGB565;
    img.data = (const uint8_t*)img_buf;
    img.data_size = (uint32_t)buf_size;

    lv_draw_image(layer, &img_dsc, &area);

    lv_free(img_buf);
}

// ════════════════════════════════════════════════════════
// PUBLIC API
// ════════════════════════════════════════════════════════

void slopos_map_init() {
    if (initialized) return;

    map_canvas = lv_canvas_create(lv_scr_act());
    lv_obj_set_size(map_canvas, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_align(map_canvas, LV_ALIGN_CENTER, 0, 0);

    // Allocate draw buffer (320×240×2 = 153KB for RGB565)
    size_t buf_size = (size_t)TFT_WIDTH * TFT_HEIGHT * 2;
    canvas_pixels = (uint8_t*)lv_malloc(buf_size);
    if (canvas_pixels) {
        lv_canvas_set_buffer(map_canvas, canvas_pixels,
                             TFT_WIDTH, TFT_HEIGHT, LV_COLOR_FORMAT_RGB565);
    }

    // Allocate JPEG tile decode buffers (use PSRAM if available)
    tile_rgb565 = (uint16_t*)lv_malloc(TILE_SIZE * TILE_SIZE * 2);  // 131KB
    jpeg_inbuf  = (uint8_t*)lv_malloc(4096);  // 4KB input buffer

    initialized = true;
}

void slopos_map_set_view(double lat, double lon, int zoom) {
    center_lat = clamp_d(lat, MIN_LAT, MAX_LAT);
    center_lon = clamp_d(lon, MIN_LON, MAX_LON);
    zoom_level = clamp(zoom, MIN_ZOOM, MAX_ZOOM);
}

double slopos_map_get_lat()  { return center_lat; }
double slopos_map_get_lon()  { return center_lon; }
int    slopos_map_get_zoom() { return zoom_level; }

void slopos_map_pan(int dx, int dy) {
    double n = (double)(1 << zoom_level);
    double pixels_per_degree = n * TILE_SIZE / 360.0;
    center_lon -= dx / pixels_per_degree;
    center_lat += dy / pixels_per_degree;
    center_lat = clamp_d(center_lat, MIN_LAT, MAX_LAT);
    center_lon = clamp_d(center_lon, MIN_LON, MAX_LON);
}

void slopos_map_zoom_in()  { zoom_level = clamp(zoom_level + 1, MIN_ZOOM, MAX_ZOOM); }
void slopos_map_zoom_out() { zoom_level = clamp(zoom_level - 1, MIN_ZOOM, MAX_ZOOM); }

void slopos_map_render() {
    if (!initialized || !map_canvas || !canvas_pixels) return;

    lv_canvas_fill_bg(map_canvas, lv_color_hex(0x0f3460), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(map_canvas, &layer);

    // ── Draw tile grid ────────────────────────────────
    double center_tx = lon_to_tile_x(center_lon, zoom_level);
    double center_ty = lat_to_tile_y(center_lat, zoom_level);

    double px_per_tile = TILE_SIZE;
    int tiles_across = 1 + TFT_WIDTH / TILE_SIZE + 1;
    int tiles_down   = 1 + TFT_HEIGHT / TILE_SIZE + 1;

    int center_px = TFT_WIDTH / 2;
    int center_py = TFT_HEIGHT / 2;

    bool any_tile_loaded = false;

    for (int ty = -1; ty <= tiles_down; ty++) {
        for (int tx = -1; tx <= tiles_across; tx++) {
            int tile_x = (int)(center_tx + tx);
            int tile_y = (int)(center_ty + ty);

            // Clamp tile coordinates to valid range
            int n = 1 << zoom_level;
            if (tile_x < 0 || tile_x >= n || tile_y < 0 || tile_y >= n) continue;

            int screen_x = (int)(center_px + (tx * px_per_tile) -
                                 (center_tx - (int)center_tx) * px_per_tile);
            int screen_y = (int)(center_py + (ty * px_per_tile) -
                                 (center_ty - (int)center_ty) * px_per_tile);

            // Skip if completely off-screen
            if (screen_x + TILE_SIZE < 0 || screen_x > TFT_WIDTH ||
                screen_y + TILE_SIZE < 0 || screen_y > TFT_HEIGHT) {
                continue;
            }

            // Try to load and render the JPEG tile
            if (slopos_sdcard_mounted() && load_tile_jpeg(zoom_level, tile_x, tile_y)) {
                draw_tile_pixels(&layer, screen_x, screen_y,
                                 TILE_SIZE, TILE_SIZE, 0, 0, TILE_SIZE, TILE_SIZE);
                any_tile_loaded = true;
            } else {
                // Fallback: placeholder grid
                lv_draw_rect_dsc_t rect_dsc;
                lv_draw_rect_dsc_init(&rect_dsc);
                rect_dsc.bg_color = lv_color_hex(
                    ((tile_x + tile_y) & 1) ? 0x1a1a2e : 0x16213e);
                rect_dsc.bg_opa = LV_OPA_COVER;
                rect_dsc.radius = 0;

                lv_area_t area;
                area.x1 = screen_x;
                area.y1 = screen_y;
                area.x2 = screen_x + TILE_SIZE - 1;
                area.y2 = screen_y + TILE_SIZE - 1;

                lv_draw_rect(&layer, &rect_dsc, &area);
            }
        }
    }

    // If no tiles loaded at all, show status message
    if (!any_tile_loaded) {
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.color = lv_color_hex(0x8e9297);
        label_dsc.text = slopos_sdcard_mounted() ?
            "No map tiles found\nCopy maps/ folder\nto SD card root" :
            "No SD card\nInsert SD card with\nmaps/ folder";
        label_dsc.text_local = true;

        lv_area_t label_area;
        label_area.x1 = 20;
        label_area.y1 = TFT_HEIGHT / 2 - 24;
        label_area.x2 = TFT_WIDTH - 20;
        label_area.y2 = TFT_HEIGHT / 2 + 24;

        lv_draw_label(&layer, &label_dsc, &label_area);
    }

    // ── Center crosshair ──────────────────────────────
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x5865f2);
    line_dsc.width = 1;
    line_dsc.opa = LV_OPA_50;

    line_dsc.p1.x = center_px - 12; line_dsc.p1.y = center_py;
    line_dsc.p2.x = center_px + 12; line_dsc.p2.y = center_py;
    lv_draw_line(&layer, &line_dsc);
    line_dsc.p1.x = center_px; line_dsc.p1.y = center_py - 12;
    line_dsc.p2.x = center_px; line_dsc.p2.y = center_py + 12;
    lv_draw_line(&layer, &line_dsc);

    lv_canvas_finish_layer(map_canvas, &layer);
}

bool slopos_map_tiles_available() {
    if (!slopos_sdcard_mounted()) return false;
    // Check for at least one tile at current zoom
    int tx = (int)lon_to_tile_x(center_lon, zoom_level);
    int ty = (int)lat_to_tile_y(center_lat, zoom_level);
    char path[64];
    snprintf(path, sizeof(path), "/maps/%d/%d/%d.jpg", zoom_level, tx, ty);
    return SD.exists(path);
}

void slopos_map_pixel_to_latlon(int px, int py, double* out_lat, double* out_lon) {
    double n = (double)(1 << zoom_level);
    double center_tx = lon_to_tile_x(center_lon, zoom_level);
    double center_ty = lat_to_tile_y(center_lat, zoom_level);

    double px_per_tile = TILE_SIZE;
    int center_px = TFT_WIDTH / 2;
    int center_py = TFT_HEIGHT / 2;

    double tile_x = center_tx + (px - center_px) / px_per_tile;
    double tile_y = center_ty + (py - center_py) / px_per_tile;

    *out_lon = tile_x_to_lon(tile_x, zoom_level);
    *out_lat = tile_y_to_lat(tile_y, zoom_level);
}
