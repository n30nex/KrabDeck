#include "map_renderer.h"
#include "../hal/tdeck_pins.h"
#include "../hal/sdcard.h"
#include <lvgl.h>
#include <cmath>
#include <cstdio>

// ── Constants ─────────────────────────────────────────────
static constexpr int TILE_SIZE    = 256;   // standard tile size
static constexpr int MAX_ZOOM     = 18;
static constexpr int MIN_ZOOM     = 0;
static constexpr double MAX_LAT   = 85.0511;
static constexpr double MIN_LAT   = -85.0511;
static constexpr double MAX_LON   = 180.0;
static constexpr double MIN_LON   = -180.0;
static constexpr double PI        = 3.14159265358979323846;

// ── State ─────────────────────────────────────────────────
static lv_obj_t* map_canvas = nullptr;
static lv_draw_buf_t draw_buf;
static uint8_t*   canvas_pixels = nullptr;

static double center_lat = 51.5074;  // London
static double center_lon = -0.1278;
static int    zoom_level = 10;
static bool   initialized = false;

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

// ════════════════════════════════════════════════════════
// PUBLIC API
// ════════════════════════════════════════════════════════

void slopos_map_init() {
    if (initialized) return;

    map_canvas = lv_canvas_create(lv_scr_act());
    lv_obj_set_size(map_canvas, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_align(map_canvas, LV_ALIGN_CENTER, 0, 0);

    // Allocate draw buffer (320×240×2 = 153KB in LV_COLOR_FORMAT_RGB565)
    size_t buf_size = LV_CANVAS_BUF_SIZE(TFT_WIDTH, TFT_HEIGHT, 16);
    canvas_pixels = (uint8_t*)lv_malloc(buf_size);
    if (canvas_pixels) {
        lv_canvas_set_buffer(map_canvas, canvas_pixels,
                             TFT_WIDTH, TFT_HEIGHT, LV_COLOR_FORMAT_RGB565);
    }

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

    // Tile offset in pixels from center
    double px_per_tile = TILE_SIZE;
    int tiles_across = 1 + TFT_WIDTH / TILE_SIZE + 1;  // +1 for partial tiles
    int tiles_down   = 1 + TFT_HEIGHT / TILE_SIZE + 1;

    int center_px = TFT_WIDTH / 2;
    int center_py = TFT_HEIGHT / 2;

    for (int ty = -1; ty <= tiles_down; ty++) {
        for (int tx = -1; tx <= tiles_across; tx++) {
            int tile_x = (int)(center_tx + tx);
            int tile_y = (int)(center_ty + ty);

            int screen_x = (int)(center_px + (tx * px_per_tile) -
                                 (center_tx - (int)center_tx) * px_per_tile);
            int screen_y = (int)(center_py + (ty * px_per_tile) -
                                 (center_ty - (int)center_ty) * px_per_tile);

            // Skip if off-screen
            if (screen_x + (int)px_per_tile < 0 || screen_x > TFT_WIDTH ||
                screen_y + (int)px_per_tile < 0 || screen_y > TFT_HEIGHT) {
                continue;
            }

            // Try to load tile from SD card
            char path[64];
            snprintf(path, sizeof(path), "/maps/%d/%d/%d.png",
                     zoom_level, tile_x, tile_y);

            // Draw tile placeholder (grid with coordinates)
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = lv_color_hex(
                ((tile_x + tile_y) & 1) ? 0x1a1a2e : 0x16213e);
            rect_dsc.bg_opa = LV_OPA_COVER;
            rect_dsc.radius = 0;

            lv_area_t area;
            area.x1 = screen_x;
            area.y1 = screen_y;
            area.x2 = screen_x + (int)px_per_tile - 1;
            area.y2 = screen_y + (int)px_per_tile - 1;

            lv_draw_rect(&layer, &rect_dsc, &area);

            // Tile coordinate label
            char label[16];
            snprintf(label, sizeof(label), "%d/%d", tile_x, tile_y);
            lv_draw_label_dsc_t label_dsc;
            lv_draw_label_dsc_init(&label_dsc);
            label_dsc.color = lv_color_hex(0x5c6067);
            label_dsc.text = label;
            label_dsc.text_local = true;

            lv_area_t label_area;
            label_area.x1 = screen_x + 4;
            label_area.y1 = screen_y + (int)px_per_tile / 2 - 8;
            label_area.x2 = screen_x + (int)px_per_tile - 1;
            label_area.y2 = screen_y + (int)px_per_tile - 1;

            lv_draw_label(&layer, &label_dsc, &label_area);
        }
    }

    // ── Center crosshair ──────────────────────────────
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x5865f2); // blurple
    line_dsc.width = 1;
    line_dsc.opa = LV_OPA_50;

    lv_point_t p1, p2;
    p1.x = center_px - 12; p1.y = center_py;
    p2.x = center_px + 12; p2.y = center_py;
    lv_draw_line(&layer, &line_dsc, &p1, &p2);
    p1.x = center_px; p1.y = center_py - 12;
    p2.x = center_px; p2.y = center_py + 12;
    lv_draw_line(&layer, &line_dsc, &p1, &p2);

    lv_canvas_finish_layer(map_canvas, &layer);
}

bool slopos_map_tiles_available() {
    return slopos_sdcard_mounted();
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
