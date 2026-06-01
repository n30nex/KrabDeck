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


/**
 * Unit tests for GT911 touch driver HAL
 * Tests: coordinate parsing, boundary clamping, multitouch,
 *        touch lifecycle (press → move → release), I2C register read simulation
 */
#include <gtest/gtest.h>
#include "hal/tdeck_pins.h"
#include "Arduino.h"
#include <cstdint>
#include <cstring>

// ── GT911 register constants (from datasheet) ────────────
// These mirror what touch.cpp uses — tested here to validate against datasheet
static constexpr uint8_t  GT911_ADDR        = 0x5D;
static constexpr uint16_t GT911_REG_CONFIG  = 0x8047u;
static constexpr uint16_t GT911_REG_STATUS  = 0x814Eu;
static constexpr uint16_t GT911_CONFIG_SIZE = 186u;   // 0x8100 - 0x8047 + 1

// Touch point structure (8 bytes per point starting at 0x814F)
struct GT911Point {
    uint8_t  track_id;
    uint8_t  x_low;
    uint8_t  x_high;
    uint8_t  y_low;
    uint8_t  y_high;
    uint8_t  size_low;
    uint8_t  size_high;
    uint8_t  reserved;
};

static_assert(sizeof(GT911Point) == 8, "GT911 point must be 8 bytes");

namespace {

// ── Simulated I2C register bank ──────────────────────────
class GT911RegisterBank {
    static constexpr size_t BANK_SIZE = 0x8150;
    uint8_t regs[BANK_SIZE] = {0};

public:
    GT911RegisterBank() { clear(); }

    void clear() {
        memset(regs, 0, sizeof(regs));
        // Default: no touches
        regs[GT911_REG_STATUS & 0xFF] = 0;
    }

    uint8_t read(uint16_t addr) const {
        if (addr < BANK_SIZE) return regs[addr];
        return 0;
    }

    void write(uint16_t addr, uint8_t val) {
        if (addr < BANK_SIZE) regs[addr] = val;
    }

    // Simulate a touch at given coordinates
    void set_touch(int idx, uint8_t track_id, uint16_t x, uint16_t y) {
        // Status register: bit 7 = buffer ready, bits 3-0 = count
        uint8_t count = idx + 1;
        uint16_t base = GT911_REG_STATUS + 1 + (idx * 8);
        write(base + 0, track_id);
        write(base + 1, x & 0xFF);       // x_low
        write(base + 2, (x >> 8) & 0xFF); // x_high
        write(base + 3, y & 0xFF);       // y_low
        write(base + 4, (y >> 8) & 0xFF); // y_high
        write(base + 5, 30);             // size
        write(base + 6, 0);
        write(base + 7, 0);
        // Set status
        regs[GT911_REG_STATUS & 0xFF] = (0x80 | (count & 0x0F));
    }

    void set_no_touch() {
        regs[GT911_REG_STATUS & 0xFF] = 0x80; // buffer ready, 0 points
    }

    void clear_status() {
        regs[GT911_REG_STATUS & 0xFF] = 0;
    }
};

// ── Coordinate transformation (mirrors touch.cpp) ────────
struct TouchMapping {
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
    int max_x;   // touch sensor max (e.g., 320)
    int max_y;   // touch sensor max (e.g., 240)
    int screen_w; // display width
    int screen_h; // display height
};

void transform_coords(const TouchMapping& m, uint16_t raw_x, uint16_t raw_y,
                      int* out_x, int* out_y)
{
    int x = raw_x, y = raw_y;

    // Swap XY if needed
    if (m.swap_xy) {
        int tmp = x; x = y; y = tmp;
    }

    // Mirror if needed (flip across axis)
    if (m.mirror_x) x = m.screen_w - (x * m.screen_w / m.max_x);
    else            x = x * m.screen_w / m.max_x;

    if (m.mirror_y) y = m.screen_h - (y * m.screen_h / m.max_y);
    else            y = y * m.screen_h / m.max_y;

    // Clamp to screen bounds
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= m.screen_w) x = m.screen_w - 1;
    if (y >= m.screen_h) y = m.screen_h - 1;

    *out_x = x;
    *out_y = y;
}

// ── Register parsing ─────────────────────────────────────
struct TouchPoint {
    int x, y;
    bool valid;
};

bool parse_touch_point(const uint8_t* buf, int idx, TouchPoint* out,
                       int max_x = 320, int max_y = 240)
{
    const uint8_t* p = buf + idx * 8;
    uint16_t x = p[1] | ((uint16_t)p[2] << 8);
    uint16_t y = p[3] | ((uint16_t)p[4] << 8);

    // Invalid sentinel (GT911 uses 0xFFFF for no point)
    if (x == 0xFFFF || y == 0xFFFF || (x == 0 && y == 0)) {
        out->valid = false;
        return false;
    }

    // Bounds check
    if (x > (uint16_t)max_x || y > (uint16_t)max_y) {
        out->valid = false;
        return false;
    }

    out->x = x;
    out->y = y;
    out->valid = true;
    return true;
}

// ════════════════════════════════════════════════════════
// TEST FIXTURE
// ════════════════════════════════════════════════════════
class TouchTest : public ::testing::Test {
protected:
    GT911RegisterBank regs;
    // T-Deck: swap portrait→landscape, mirror Y (portrait X bottom→top in landscape)
    // max_x = TFT_HEIGHT(240) = portrait X range; max_y = TFT_WIDTH(320) = portrait Y range
    TouchMapping default_mapping = {true, false, true, TFT_HEIGHT, TFT_WIDTH, TFT_WIDTH, TFT_HEIGHT};

    void SetUp() override {
        arduino_mock::reset();
        regs.clear();
    }

    // Simulate reading the 5-point data buffer
    void read_points(uint8_t* out_buf) {
        for (int i = 0; i < 5 * 8; i++) {
            out_buf[i] = regs.read(GT911_REG_STATUS + 1 + i);
        }
    }
};

// ── Coordinate transformation ────────────────────────────
TEST_F(TouchTest, CenterPointMapsToScreenCenter) {
    int x, y;
    // raw(160,120): swap→x=120,y=160; scale x=120*320/240=160, y=160*240/320=120; mirror_y y=240-120=120
    transform_coords(default_mapping, 160, 120, &x, &y);
    EXPECT_EQ(x, 160);
    EXPECT_EQ(y, 120);
}

TEST_F(TouchTest, OriginMapsToOrigin) {
    int x, y;
    TouchMapping m = {true, false, false, 320, 240, TFT_WIDTH, TFT_HEIGHT};
    transform_coords(m, 0, 0, &x, &y);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
}

TEST_F(TouchTest, MaxCoordsMapToScreenMax) {
    int x, y;
    TouchMapping m = {true, false, false, 320, 240, TFT_WIDTH, TFT_HEIGHT};
    transform_coords(m, 240, 320, &x, &y);
    EXPECT_EQ(x, 319);  // raw_y=320 → screen_x = 320*320/320 = 320, clamped to 319
    EXPECT_EQ(y, 239);
}

TEST_F(TouchTest, NoSwapXYDirectMapping) {
    int x, y;
    TouchMapping m = {false, false, false, TFT_WIDTH, TFT_HEIGHT, TFT_WIDTH, TFT_HEIGHT};
    transform_coords(m, 160, 120, &x, &y);
    EXPECT_EQ(x, 160);
    EXPECT_EQ(y, 120);
}

TEST_F(TouchTest, MirrorXFlipsAcrossHorizontal) {
    int x, y;
    TouchMapping m = {false, true, false, 320, 240, TFT_WIDTH, TFT_HEIGHT};
    transform_coords(m, 50, 100, &x, &y);
    // x = 320 - (50*320/320) = 320 - 50 = 270
    EXPECT_EQ(x, 270);
    EXPECT_EQ(y, 100);
}

TEST_F(TouchTest, MirrorYFlipsAcrossVertical) {
    int x, y;
    TouchMapping m = {false, false, true, 320, 240, TFT_WIDTH, TFT_HEIGHT};
    transform_coords(m, 160, 30, &x, &y);
    // y = 240 - (30*240/240) = 240 - 30 = 210
    EXPECT_EQ(x, 160);
    EXPECT_EQ(y, 210);
}

TEST_F(TouchTest, NegativeCoordsClampedToZero) {
    int x, y;
    // Simulating out-of-bounds sensor reading
    transform_coords(default_mapping, 0, 0, &x, &y);
    EXPECT_GE(x, 0);
    EXPECT_GE(y, 0);
}

TEST_F(TouchTest, BeyondMaxCoordsClampedToScreenEdge) {
    int x, y;
    TouchMapping m = {true, false, false, 320, 240, TFT_WIDTH, TFT_HEIGHT};
    // raw_y=500 → screen_x = 500*320/320 = 500 → clamped to 319
    transform_coords(m, 500, 500, &x, &y);
    EXPECT_LE(x, TFT_WIDTH - 1);
    EXPECT_LE(y, TFT_HEIGHT - 1);
}

// ── GT911 point parsing ─────────────────────────────────
TEST_F(TouchTest, ParseValidPoint) {
    uint8_t buf[40] = {0}; // 5 points * 8 bytes
    // Valid point: track_id=1, x=200, y=150
    buf[0] = 1;       // track_id
    buf[1] = 200;     // x_low
    buf[2] = 0;       // x_high  (200 & 0xFF = 0xC8, so x=0x00C8=200)
    buf[3] = 150;     // y_low
    buf[4] = 0;       // y_high
    buf[5] = 30;      // size
    buf[6] = 0;
    buf[7] = 0;

    TouchPoint pt;
    bool ok = parse_touch_point(buf, 0, &pt);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(pt.valid);
    EXPECT_EQ(pt.x, 200);
    EXPECT_EQ(pt.y, 150);
}

TEST_F(TouchTest, ParsePointWithHighBytes) {
    uint8_t buf[40] = {0};
    // x=500 (0x01F4), y=300 (0x012C)
    buf[0] = 0;
    buf[1] = 0xF4;    // x_low
    buf[2] = 0x01;    // x_high
    buf[3] = 0x2C;    // y_low
    buf[4] = 0x01;    // y_high
    buf[5] = 20;
    buf[6] = 0;
    buf[7] = 0;

    TouchPoint pt;
    bool ok = parse_touch_point(buf, 0, &pt, 800, 480);
    EXPECT_TRUE(ok);
    EXPECT_EQ(pt.x, 500);
    EXPECT_EQ(pt.y, 300);
}

TEST_F(TouchTest, ParseZeroPointReturnsInvalid) {
    uint8_t buf[40] = {0};
    // All zeros in first 4 bytes = no touch
    TouchPoint pt;
    bool ok = parse_touch_point(buf, 0, &pt);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(pt.valid);
}

TEST_F(TouchTest, ParseFFFFSentinelReturnsInvalid) {
    uint8_t buf[40];
    memset(buf, 0xFF, 40);
    TouchPoint pt;
    bool ok = parse_touch_point(buf, 0, &pt);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(pt.valid);
}

TEST_F(TouchTest, ParseOOBPointReturnsInvalid) {
    uint8_t buf[40] = {0};
    buf[1] = 0xFF;  buf[2] = 0x07; // x=2047, beyond 320 max
    buf[3] = 0xFF;  buf[4] = 0x07; // y=2047
    TouchPoint pt;
    bool ok = parse_touch_point(buf, 0, &pt, 320, 240);
    EXPECT_FALSE(ok);
}

TEST_F(TouchTest, ParseSecondPointInBuffer) {
    uint8_t buf[40] = {0};
    // Point 0: dummy
    memset(buf, 0xFF, 8);
    // Point 1: valid
    buf[8] = 2;
    buf[9] = 100;
    buf[10] = 0;
    buf[11] = 80;
    buf[12] = 0;

    TouchPoint pt;
    bool ok = parse_touch_point(buf, 1, &pt);
    EXPECT_TRUE(ok);
    EXPECT_EQ(pt.x, 100);
    EXPECT_EQ(pt.y, 80);
}

// ── GT911 status register logic ─────────────────────────
TEST_F(TouchTest, StatusByteIndicatesTouchCount) {
    regs.set_touch(0, 1, 200, 150);
    uint8_t status = regs.read(GT911_REG_STATUS & 0xFF);
    EXPECT_EQ(status, 0x81); // buffer ready + 1 point
}

TEST_F(TouchTest, StatusByteAfterClearShowsZero) {
    regs.set_touch(0, 1, 200, 150);
    regs.clear_status();
    uint8_t status = regs.read(GT911_REG_STATUS & 0xFF);
    EXPECT_EQ(status, 0);
}

TEST_F(TouchTest, TwoTouchesSetCorrectCount) {
    regs.set_touch(0, 1, 100, 100);
    regs.set_touch(1, 2, 200, 150);
    uint8_t status = regs.read(GT911_REG_STATUS & 0xFF);
    EXPECT_EQ(status, 0x82); // buffer ready + 2 points
}

TEST_F(TouchTest, BufferReadyBitAlwaysSetWithData) {
    regs.set_touch(0, 3, 50, 60);
    uint8_t status = regs.read(GT911_REG_STATUS & 0xFF);
    EXPECT_TRUE(status & 0x80); // bit 7 must be set
}

// ── Touch lifecycle (press → hold → release) ────────────
TEST_F(TouchTest, NoTouchInitially) {
    // On startup, no touch data
    regs.clear();
    uint8_t status = regs.read(GT911_REG_STATUS & 0xFF);
    EXPECT_EQ(status, 0);
}

TEST_F(TouchTest, TouchPressDetected) {
    regs.set_touch(0, 1, 160, 120);
    uint8_t pts = regs.read(GT911_REG_STATUS & 0xFF) & 0x0F;
    EXPECT_GT(pts, 0);
}

TEST_F(TouchTest, TouchReleaseToNoTouch) {
    regs.set_touch(0, 1, 160, 120);
    ASSERT_GT(regs.read(GT911_REG_STATUS & 0xFF) & 0x0F, 0);
    regs.set_no_touch();
    EXPECT_EQ(regs.read(GT911_REG_STATUS & 0xFF) & 0x0F, 0);
}

// ── LVGL coordinate clamping ────────────────────────────
TEST_F(TouchTest, AllValidCoordsInScreenRange) {
    // Sweep all valid raw touch coords (5-point sampling)
    // With swap_xy + default mapping, all output coords must stay in [0, 319]×[0, 239]
    for (int raw_x = 0; raw_x <= 320; raw_x += 20) {
        for (int raw_y = 0; raw_y <= 320; raw_y += 20) {
            int x, y;
            transform_coords(default_mapping, raw_x, raw_y, &x, &y);
            EXPECT_GE(x, 0)         << "x < 0 at (" << raw_x << "," << raw_y << ")";
            EXPECT_LT(x, TFT_WIDTH) << "x >= " << TFT_WIDTH << " at (" << raw_x << "," << raw_y << ")";
            EXPECT_GE(y, 0)         << "y < 0 at (" << raw_x << "," << raw_y << ")";
            EXPECT_LT(y, TFT_HEIGHT)<< "y >= " << TFT_HEIGHT << " at (" << raw_x << "," << raw_y << ")";
        }
    }
}

} // anonymous namespace
