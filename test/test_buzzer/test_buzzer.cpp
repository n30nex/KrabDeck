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

#include <gtest/gtest.h>

#include "hal/buzzer.h"

namespace {

using sigurdos::hal::BuzzerPatternKind;
using sigurdos::hal::BuzzerPatternStep;
using sigurdos::hal::SIGURDOS_BUZZER_DOUBLE_GAP_MS;
using sigurdos::hal::SIGURDOS_BUZZER_DOUBLE_ON_MS;
using sigurdos::hal::SIGURDOS_BUZZER_SHORT_ON_MS;
using sigurdos::hal::sigurdos_buzzer_pattern;

class BuzzerPatternTest : public ::testing::Test {};

TEST_F(BuzzerPatternTest, ShortPatternMatchesNotificationContract) {
    std::size_t count = 0;
    const BuzzerPatternStep* pattern =
        sigurdos_buzzer_pattern(BuzzerPatternKind::Short, &count);

    ASSERT_NE(pattern, nullptr);
    ASSERT_EQ(count, 2U);
    EXPECT_TRUE(pattern[0].level_high);
    EXPECT_EQ(pattern[0].duration_ms, SIGURDOS_BUZZER_SHORT_ON_MS);
    EXPECT_FALSE(pattern[1].level_high);
    EXPECT_EQ(pattern[1].duration_ms, 0U);
}

TEST_F(BuzzerPatternTest, DoublePatternMatchesNotificationContract) {
    std::size_t count = 0;
    const BuzzerPatternStep* pattern =
        sigurdos_buzzer_pattern(BuzzerPatternKind::Double, &count);

    ASSERT_NE(pattern, nullptr);
    ASSERT_EQ(count, 4U);
    EXPECT_TRUE(pattern[0].level_high);
    EXPECT_EQ(pattern[0].duration_ms, SIGURDOS_BUZZER_DOUBLE_ON_MS);
    EXPECT_FALSE(pattern[1].level_high);
    EXPECT_EQ(pattern[1].duration_ms, SIGURDOS_BUZZER_DOUBLE_GAP_MS);
    EXPECT_TRUE(pattern[2].level_high);
    EXPECT_EQ(pattern[2].duration_ms, SIGURDOS_BUZZER_DOUBLE_ON_MS);
    EXPECT_FALSE(pattern[3].level_high);
    EXPECT_EQ(pattern[3].duration_ms, 0U);
}

TEST_F(BuzzerPatternTest, PatternLookupSupportsNullCount) {
    const BuzzerPatternStep* pattern =
        sigurdos_buzzer_pattern(BuzzerPatternKind::Short, nullptr);

    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern[0].level_high);
    EXPECT_EQ(pattern[0].duration_ms, SIGURDOS_BUZZER_SHORT_ON_MS);
}

TEST_F(BuzzerPatternTest, PatternDurationsStayWithinResponsiveBounds) {
    std::size_t count = 0;
    const BuzzerPatternStep* pattern =
        sigurdos_buzzer_pattern(BuzzerPatternKind::Double, &count);

    uint16_t total_ms = 0;
    for (std::size_t i = 0; i < count; ++i) {
        total_ms = static_cast<uint16_t>(total_ms + pattern[i].duration_ms);
        EXPECT_LE(pattern[i].duration_ms, 100U);
    }

    EXPECT_EQ(total_ms,
              static_cast<uint16_t>((2 * SIGURDOS_BUZZER_DOUBLE_ON_MS) +
                                    SIGURDOS_BUZZER_DOUBLE_GAP_MS));
    EXPECT_LE(total_ms, 250U);
}

} // namespace
