// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Native tests for runtime Launcher detection (C3).

#include <gtest/gtest.h>

#include "hal/launcher_env.h"
#include "esp_partition.h"

namespace {

class LauncherEnvTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default: no Launcher partition present
        sigurdos::test::mock_launcher_partition(false);
    }
};

TEST_F(LauncherEnvTest, DetectsStandaloneByDefault)
{
    EXPECT_FALSE(sigurdos_is_under_launcher());
    EXPECT_STREQ(sigurdos_launcher_env_name(), "standalone");
}

TEST_F(LauncherEnvTest, DetectsLauncherWhenTestPartitionExists)
{
    sigurdos::test::mock_launcher_partition(true);
    EXPECT_TRUE(sigurdos_is_under_launcher());
    EXPECT_STREQ(sigurdos_launcher_env_name(), "bmorcelli/Launcher");
}

TEST_F(LauncherEnvTest, StandaloneAfterLauncherThenRemove)
{
    sigurdos::test::mock_launcher_partition(true);
    EXPECT_TRUE(sigurdos_is_under_launcher());

    sigurdos::test::mock_launcher_partition(false);
    EXPECT_FALSE(sigurdos_is_under_launcher());
    EXPECT_STREQ(sigurdos_launcher_env_name(), "standalone");
}

} // namespace
