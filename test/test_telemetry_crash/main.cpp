// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <gtest/gtest.h>
#include <cstdint>

#include "diagnostics/telemetry_crash.h"

namespace {

using sigurdos::telemetry::crash::RTC_CRASH_BACKTRACE_CAPACITY;
using sigurdos::telemetry::crash::RtcCrashRecord;
using sigurdos::telemetry::crash::bounded_backtrace_count;

TEST(TelemetryCrashTest, BacktraceCapacityMatchesRecordStorage)
{
    RtcCrashRecord record{};

    EXPECT_EQ(sizeof(record.backtrace_pcs) / sizeof(record.backtrace_pcs[0]),
              RTC_CRASH_BACKTRACE_CAPACITY);
    EXPECT_EQ(sizeof(record.backtrace_pcs), 16u);
}

TEST(TelemetryCrashTest, BoundedBacktraceCountPreservesValidCounts)
{
    EXPECT_EQ(bounded_backtrace_count(0), 0);
    EXPECT_EQ(bounded_backtrace_count(1), 1);
    EXPECT_EQ(bounded_backtrace_count(RTC_CRASH_BACKTRACE_CAPACITY),
              RTC_CRASH_BACKTRACE_CAPACITY);
}

TEST(TelemetryCrashTest, BoundedBacktraceCountClampsOverflowCounts)
{
    EXPECT_EQ(bounded_backtrace_count(RTC_CRASH_BACKTRACE_CAPACITY + 1),
              RTC_CRASH_BACKTRACE_CAPACITY);
    EXPECT_EQ(bounded_backtrace_count(UINT8_MAX), RTC_CRASH_BACKTRACE_CAPACITY);
}

}  // anonymous namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
