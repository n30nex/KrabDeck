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
 * Unit tests for NodePrefs defaults and native preference persistence.
 */
#include <gtest/gtest.h>

#include "hal/prefs.h"

namespace {

class PrefsTest : public ::testing::Test {
protected:
    void SetUp() override {
        sigurdos::NodePrefs defaults;
        defaults.set_defaults();
        sigurdos::prefs_set(defaults);
    }
};

TEST_F(PrefsTest, DefaultRxBoostedGainIsDisabled) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();

    EXPECT_FALSE(prefs.rx_boosted_gain);
}

TEST_F(PrefsTest, RxBoostedGainRoundTripsThroughPrefsSetAndGet) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    prefs.rx_boosted_gain = true;

    sigurdos::prefs_set(prefs);

    EXPECT_TRUE(sigurdos::prefs_get().rx_boosted_gain);

    prefs.rx_boosted_gain = false;
    sigurdos::prefs_set(prefs);

    EXPECT_FALSE(sigurdos::prefs_get().rx_boosted_gain);
}

TEST_F(PrefsTest, RxBoostedGainRoundTripsThroughPrefsSaveAndLoad) {
    sigurdos::NodePrefs saved;
    saved.set_defaults();
    saved.rx_boosted_gain = true;

    ASSERT_TRUE(sigurdos::prefs_save(saved));

    sigurdos::NodePrefs loaded;
    loaded.set_defaults();

    ASSERT_TRUE(sigurdos::prefs_load(loaded));
    EXPECT_TRUE(loaded.rx_boosted_gain);
}

} // namespace
