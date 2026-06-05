// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <gtest/gtest.h>

#include "diagnostics/telemetry.h"

namespace {

using sigurdos::telemetry::packet_log_field_or_empty;

TEST(TelemetryPacketLogTest, NullFieldReturnsEmptyString)
{
    const char* field = packet_log_field_or_empty(nullptr);

    ASSERT_NE(field, nullptr);
    EXPECT_STREQ(field, "");
}

TEST(TelemetryPacketLogTest, NonNullFieldIsPreserved)
{
    const char* field = packet_log_field_or_empty("sender");

    EXPECT_STREQ(field, "sender");
}

TEST(TelemetryPacketLogTest, EmptyFieldIsPreserved)
{
    const char* field = packet_log_field_or_empty("");

    ASSERT_NE(field, nullptr);
    EXPECT_STREQ(field, "");
}

}  // anonymous namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
