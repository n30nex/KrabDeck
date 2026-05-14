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


/**
 * Unit tests for offline map tile math and rendering contract
 * Tests: Web Mercator coordinate math, tile bounds checking,
 *        lat/lon ↔ tile conversion, zoom level validation, pan limits
 */
#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>

namespace {

// Earth radius for Web Mercator
static constexpr double PI = 3.14159265358979323846;
static constexpr double EARTH_RADIUS_M = 6378137.0;

// ── Web Mercator math (EPSG:3857) ────────────────────────
// Convert longitude to meters
double lon_to_meters_x(double lon) {
    return lon * (PI / 180.0) * EARTH_RADIUS_M;
}

// Convert latitude to meters
double lat_to_meters_y(double lat) {
    double lat_rad = lat * PI / 180.0;
    return EARTH_RADIUS_M * log(tan(PI / 4.0 + lat_rad / 2.0));
}

// Convert lat/lon to tile coordinates at a given zoom level
void latlon_to_tile(double lat, double lon, int zoom,
                    int* tile_x, int* tile_y) {
    int n = 1 << zoom;
    *tile_x = (int)((lon + 180.0) / 360.0 * n);
    double lat_rad = lat * PI / 180.0;
    *tile_y = (int)((1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / PI) / 2.0 * n);

    // Clamp
    if (*tile_x < 0) *tile_x = 0;
    if (*tile_x >= n) *tile_x = n - 1;
    if (*tile_y < 0) *tile_y = 0;
    if (*tile_y >= n) *tile_y = n - 1;
}

// Get the total number of tiles at a given zoom level
int tiles_at_zoom(int zoom) {
    int n = 1 << zoom;
    return n * n;
}

// Check if a tile is within valid range for a zoom level
bool tile_valid(int z, int x, int y) {
    int max_tile = (1 << z) - 1;
    return x >= 0 && x <= max_tile && y >= 0 && y <= max_tile;
}

// ── Tile path on SD card ──────────────────────────────────
void tile_to_path(int z, int x, int y, char* buf, size_t buf_sz) {
    snprintf(buf, buf_sz, "/maps/%d/%d/%d.png", z, x, y);
}

// ── LVGL canvas math (simplified) ─────────────────────────
struct MapViewport {
    int screen_w;     // display width
    int screen_h;     // display height
    double center_lon;
    double center_lat;
    int zoom;         // 0-18
};

void viewport_pixel_to_latlon(const MapViewport& vp, int px, int py,
                               double* out_lat, double* out_lon) {
    // Simplified: pixel offset from center as lat/lon delta
    // At zoom level z, each tile is 256×256 pixels
    double tile_size = 256.0;
    int tiles_per_dim = 1 << vp.zoom;
    double world_pixels = tile_size * tiles_per_dim;

    // Pixel position in world coordinates
    double center_px = world_pixels / 2.0;
    double center_py = world_pixels / 2.0;

    double offset_x = (px - vp.screen_w / 2.0);
    double offset_y = (py - vp.screen_h / 2.0);

    // Lon: linear with x
    double lon = vp.center_lon + (offset_x / world_pixels) * 360.0;

    // Lat: approximate (non-linear, simplified for test)
    double lat_range = 180.0 / (1 << vp.zoom) * 4.0; // rough at this zoom
    double lat = vp.center_lat - (offset_y / world_pixels) * 360.0;

    // Clamp
    if (lon > 180.0) lon = 180.0;
    if (lon < -180.0) lon = -180.0;
    if (lat > 85.0511) lat = 85.0511;
    if (lat < -85.0511) lat = -85.0511;

    *out_lat = lat;
    *out_lon = lon;
}

// ════════════════════════════════════════════════════════
// TESTS
// ════════════════════════════════════════════════════════
class MapTest : public ::testing::Test {
protected:
    MapViewport vp;
    void SetUp() override {
        vp = {320, 240, 0.0, 51.5, 10}; // London-ish
    }
};

// ── Web Mercator ──────────────────────────────────────────
TEST_F(MapTest, LonZeroIsMercatorCenter) {
    double x = lon_to_meters_x(0.0);
    EXPECT_NEAR(x, 0.0, 0.1);
}

TEST_F(MapTest, LatZeroIsMercatorEquator) {
    double y = lat_to_meters_y(0.0);
    EXPECT_NEAR(y, 0.0, 0.1);
}

TEST_F(MapTest, MercatorIsSymmetric) {
    double y1 = lat_to_meters_y(45.0);
    double y2 = lat_to_meters_y(-45.0);
    EXPECT_NEAR(y1, -y2, 1000.0);
}

// ── Tile coordinates ──────────────────────────────────────
TEST_F(MapTest, Zoom0HasOneTile) {
    EXPECT_EQ(tiles_at_zoom(0), 1);
}

TEST_F(MapTest, Zoom1HasFourTiles) {
    EXPECT_EQ(tiles_at_zoom(1), 4);
}

TEST_F(MapTest, Zoom10Has1MTiles) {
    EXPECT_EQ(tiles_at_zoom(10), 1024 * 1024);
}

TEST_F(MapTest, NullIslandAtZoom0) {
    int tx, ty;
    latlon_to_tile(0.0, 0.0, 0, &tx, &ty);
    EXPECT_EQ(tx, 0);
    EXPECT_EQ(ty, 0);
}

TEST_F(MapTest, LondonAtZoom10) {
    int tx, ty;
    latlon_to_tile(51.5074, -0.1278, 10, &tx, &ty);
    EXPECT_TRUE(tile_valid(10, tx, ty));
    // London should be roughly tile_x: ~511, tile_y: ~340 at zoom 10
    EXPECT_EQ(tx, 511);
    EXPECT_NEAR(ty, 340, 2);
}

TEST_F(MapTest, PoleClampedToValidRange) {
    int tx, ty;
    latlon_to_tile(90.0, 0.0, 5, &tx, &ty);
    EXPECT_TRUE(tile_valid(5, tx, ty));
    EXPECT_EQ(ty, 0); // North pole → top row
}

// ── Tile validity ─────────────────────────────────────────
TEST_F(MapTest, TileValidInRange) {
    EXPECT_TRUE(tile_valid(5, 10, 10));
    EXPECT_TRUE(tile_valid(0, 0, 0));
    EXPECT_TRUE(tile_valid(5, 0, 0));
    EXPECT_TRUE(tile_valid(5, 31, 31)); // max at zoom 5
}

TEST_F(MapTest, TileInvalidOutOfRange) {
    EXPECT_FALSE(tile_valid(5, 32, 10)); // x > 31
    EXPECT_FALSE(tile_valid(5, 10, 32)); // y > 31
    EXPECT_FALSE(tile_valid(5, -1, 10));
}

// ── Tile path ─────────────────────────────────────────────
TEST_F(MapTest, TilePathFormat) {
    char buf[64];
    tile_to_path(10, 512, 340, buf, sizeof(buf));
    EXPECT_STREQ(buf, "/maps/10/512/340.png");
}

// ── Viewport pan limits ───────────────────────────────────
TEST_F(MapTest, CenterPixelIsCenterLatLon) {
    double lat, lon;
    vp = {320, 240, -0.1278, 51.5074, 10};
    viewport_pixel_to_latlon(vp, 160, 120, &lat, &lon);
    EXPECT_NEAR(lat, 51.5074, 0.5);
    EXPECT_NEAR(lon, -0.1278, 0.5);
}

TEST_F(MapTest, ScreenCornerMapsToValidCoord) {
    double lat, lon;
    viewport_pixel_to_latlon(vp, 0, 0, &lat, &lon);
    EXPECT_GE(lat, -85.0511);
    EXPECT_LE(lat, 85.0511);
    EXPECT_GE(lon, -180.0);
    EXPECT_LE(lon, 180.0);
}

} // anonymous namespace
