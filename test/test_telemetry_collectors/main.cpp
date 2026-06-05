// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <gtest/gtest.h>
#include <cstdint>

#include "diagnostics/telemetry_collectors.h"

namespace {

using sigurdos::telemetry::collectors::TaskWatermark;
using sigurdos::telemetry::collectors::task_watermark_output_valid;

TEST(TelemetryCollectorsTest, TaskWatermarkOutputRejectsNullBuffer)
{
    EXPECT_FALSE(task_watermark_output_valid(nullptr, 1));
    EXPECT_FALSE(task_watermark_output_valid(nullptr, UINT8_MAX));
}

TEST(TelemetryCollectorsTest, TaskWatermarkOutputRejectsZeroCapacity)
{
    TaskWatermark task{};

    EXPECT_FALSE(task_watermark_output_valid(&task, 0));
}

TEST(TelemetryCollectorsTest, TaskWatermarkOutputAcceptsNonNullCapacity)
{
    TaskWatermark task{};

    EXPECT_TRUE(task_watermark_output_valid(&task, 1));
    EXPECT_TRUE(task_watermark_output_valid(&task, UINT8_MAX));
}

}  // anonymous namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
