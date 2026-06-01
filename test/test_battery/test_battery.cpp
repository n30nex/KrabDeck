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
 * Unit tests for battery HAL
 * Tests: mV to percentage conversion, edge cases, clamping
 */
#include <gtest/gtest.h>
#include "Arduino.h"

// Include the source under test (we test the algorithm, not the hardware)
// Battery formula: pct = ((mv - 3000) * 100) / (4200 - 3000)
// Clamped to [0, 100]

namespace {

// Replicate the battery algorithm from hal/battery.cpp for pure testing
uint8_t battery_pct_from_mv(uint16_t mv) {
    const int BAT_MIN_MV = 3000;
    const int BAT_MAX_MV = 4200;
    int32_t m = (int32_t)mv;
    if (m <= BAT_MIN_MV) return 0;
    if (m >= BAT_MAX_MV) return 100;
    return (uint8_t)(((m - BAT_MIN_MV) * 100) / (BAT_MAX_MV - BAT_MIN_MV));
}

class BatteryTest : public ::testing::Test {
protected:
    void SetUp() override {
        arduino_mock::reset();
    }
};

// ── Normal range ────────────────────────────────────────
TEST_F(BatteryTest, FullCharge_4200mV_Returns100) {
    EXPECT_EQ(battery_pct_from_mv(4200), 100);
}

TEST_F(BatteryTest, EmptyCharge_3000mV_Returns0) {
    EXPECT_EQ(battery_pct_from_mv(3000), 0);
}

TEST_F(BatteryTest, HalfCharge_3600mV_Returns50) {
    // (3600 - 3000) * 100 / 1200 = 60000 / 1200 = 50
    EXPECT_EQ(battery_pct_from_mv(3600), 50);
}

TEST_F(BatteryTest, QuarterCharge_3300mV_Returns25) {
    EXPECT_EQ(battery_pct_from_mv(3300), 25);
}

TEST_F(BatteryTest, ThreeQuarterCharge_3900mV_Returns75) {
    EXPECT_EQ(battery_pct_from_mv(3900), 75);
}

TEST_F(BatteryTest, NominalLiPo_3700mV_ReturnsApprox58) {
    // (3700 - 3000) * 100 / 1200 = 70000 / 1200 ≈ 58
    uint8_t pct = battery_pct_from_mv(3700);
    EXPECT_GE(pct, 57);
    EXPECT_LE(pct, 59);
}

// ── Edge cases ──────────────────────────────────────────
TEST_F(BatteryTest, OverVoltage_4300mV_ClampedTo100) {
    EXPECT_EQ(battery_pct_from_mv(4300), 100);
}

TEST_F(BatteryTest, OverVoltage_5000mV_ClampedTo100) {
    EXPECT_EQ(battery_pct_from_mv(5000), 100);
}

TEST_F(BatteryTest, UnderVoltage_2500mV_ClampedTo0) {
    EXPECT_EQ(battery_pct_from_mv(2500), 0);
}

TEST_F(BatteryTest, UnderVoltage_0mV_ClampedTo0) {
    EXPECT_EQ(battery_pct_from_mv(0), 0);
}

TEST_F(BatteryTest, JustAboveMin_3001mV_Returns0) {
    // 1 * 100 / 1200 = 0 (integer division)
    EXPECT_EQ(battery_pct_from_mv(3001), 0);
}

TEST_F(BatteryTest, JustBelowMax_4199mV_Returns99) {
    // 1199 * 100 / 1200 = 99
    EXPECT_EQ(battery_pct_from_mv(4199), 99);
}

// ── Monotonicity ────────────────────────────────────────
TEST_F(BatteryTest, PercentageIncreasesMonotonically) {
    uint8_t prev = 0;
    for (uint16_t mv = 3000; mv <= 4200; mv += 10) {
        uint8_t curr = battery_pct_from_mv(mv);
        EXPECT_GE(curr, prev) << "Non-monotonic at " << mv << "mV";
        prev = curr;
    }
}

// ── Range check ──────────────────────────────────────────
TEST_F(BatteryTest, AllValuesInRange) {
    for (uint16_t mv = 0; mv < 6000; mv += 50) {
        uint8_t pct = battery_pct_from_mv(mv);
        EXPECT_GE(pct, 0);
        EXPECT_LE(pct, 100);
    }
}

// ── ADC conversion test ─────────────────────────────────
TEST_F(BatteryTest, ADCRawToMillivolts) {
    // BAT_ADC_MULT = 2.0 * 3.3 * 1000 = 6600
    // mV = (6600 * raw) / 4096
    const float BAT_ADC_MULT = 6600.0f;

    // raw = 2048 (mid-scale on 12-bit ADC) → ~3300 mV
    float mv = (BAT_ADC_MULT * 2048.0f) / 4096.0f;
    EXPECT_NEAR(mv, 3300.0f, 1.0f);

    // raw = 4095 (full scale) → ~6600 mV (would be clamped)
    mv = (BAT_ADC_MULT * 4095.0f) / 4096.0f;
    EXPECT_NEAR(mv, 6598.0f, 5.0f);
}

// ── Integration with mock analogRead ────────────────────
TEST_F(BatteryTest, MockAnalogReadIntegration) {
    // Simulate 8 samples at 3800 mV:
    // raw = (3800 * 4096) / 6600 ≈ 2359
    arduino_mock::analog_values[4] = 2359; // PIN_BAT_ADC = 4

    int raw = analogRead(4);
    EXPECT_EQ(raw, 2359);

    // Average 8 identical reads
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) sum += raw;
    sum /= 8;
    uint16_t mv = (uint16_t)((6600.0f * (float)sum) / 4096.0f);
    // Should be approximately 3800
    EXPECT_NEAR(mv, 3800, 20);
}

} // anonymous namespace
