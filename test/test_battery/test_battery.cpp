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
 * Unit tests for battery HAL conversion helpers.
 * Tests: mV to percentage conversion, ADC raw to mV conversion, edge cases,
 *        clamping, monotonicity, and mock ADC reads.
 */
#include <gtest/gtest.h>
#include "Arduino.h"
#include "hal/battery.h"

namespace {

class BatteryTest : public ::testing::Test {
protected:
    void SetUp() override {
        arduino_mock::reset();
    }
};

TEST_F(BatteryTest, FullCharge_4200mV_Returns100) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(4200), 100);
}

TEST_F(BatteryTest, EmptyCharge_3000mV_Returns0) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(3000), 0);
}

TEST_F(BatteryTest, HalfCharge_3600mV_Returns50) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(3600), 50);
}

TEST_F(BatteryTest, QuarterCharge_3300mV_Returns25) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(3300), 25);
}

TEST_F(BatteryTest, ThreeQuarterCharge_3900mV_Returns75) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(3900), 75);
}

TEST_F(BatteryTest, NominalLiPo_3700mV_ReturnsApprox58) {
    uint8_t pct = sigurdos_battery_pct_from_mv(3700);
    EXPECT_GE(pct, 57);
    EXPECT_LE(pct, 59);
}

TEST_F(BatteryTest, OverVoltage_4300mV_ClampedTo100) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(4300), 100);
}

TEST_F(BatteryTest, OverVoltage_5000mV_ClampedTo100) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(5000), 100);
}

TEST_F(BatteryTest, UnderVoltage_2500mV_ClampedTo0) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(2500), 0);
}

TEST_F(BatteryTest, UnderVoltage_0mV_ClampedTo0) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(0), 0);
}

TEST_F(BatteryTest, JustAboveMin_3001mV_Returns0) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(3001), 0);
}

TEST_F(BatteryTest, JustBelowMax_4199mV_Returns99) {
    EXPECT_EQ(sigurdos_battery_pct_from_mv(4199), 99);
}

TEST_F(BatteryTest, PercentageIncreasesMonotonically) {
    uint8_t prev = 0;
    for (uint16_t mv = BAT_MIN_MV; mv <= BAT_MAX_MV; mv += 10) {
        uint8_t curr = sigurdos_battery_pct_from_mv(mv);
        EXPECT_GE(curr, prev) << "Non-monotonic at " << mv << "mV";
        prev = curr;
    }
}

TEST_F(BatteryTest, AllValuesInRange) {
    for (uint16_t mv = 0; mv < 6000; mv += 50) {
        uint8_t pct = sigurdos_battery_pct_from_mv(mv);
        EXPECT_GE(pct, 0);
        EXPECT_LE(pct, 100);
    }
}

TEST_F(BatteryTest, ADCRawToMillivolts) {
    EXPECT_EQ(sigurdos_battery_mv_from_adc_raw(0), 0);
    EXPECT_NEAR(sigurdos_battery_mv_from_adc_raw(2048), 3300, 1);
    EXPECT_NEAR(sigurdos_battery_mv_from_adc_raw(4095), 6598, 5);
}

TEST_F(BatteryTest, MockAnalogReadIntegration) {
    arduino_mock::analog_values[PIN_BAT_ADC] = 2359;

    int raw = analogRead(PIN_BAT_ADC);
    EXPECT_EQ(raw, 2359);

    uint16_t mv = sigurdos_battery_mv_from_adc_raw((uint16_t)raw);
    EXPECT_NEAR(mv, 3800, 20);
    EXPECT_EQ(sigurdos_battery_pct_from_mv(mv), 66);
}

} // anonymous namespace
