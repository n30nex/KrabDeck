// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <cstring>

#include <gtest/gtest.h>

#include "hal/prefs.h"
#include "hal/radio_profiles.h"

namespace {

TEST(RadioProfilesTest, DefaultProfileIsKrabOsCanada) {
    const auto* profile = sigurdos::radio_profile_default();

    ASSERT_NE(nullptr, profile);
    EXPECT_STREQ("ca_902_928", profile->id);
    EXPECT_STREQ("Canada 902-928", profile->short_label);
    EXPECT_FLOAT_EQ(910.525f, profile->freq_mhz);
    EXPECT_FLOAT_EQ(62.5f, profile->bw_khz);
    EXPECT_EQ(7, profile->sf);
    EXPECT_EQ(5, profile->cr);
    EXPECT_EQ(20, profile->tx_power_dbm);
}

TEST(RadioProfilesTest, ProfileAtRejectsTheCountBoundary) {
    const size_t count = sigurdos::radio_profile_count();

    ASSERT_GT(count, 0u);
    EXPECT_EQ(sigurdos::radio_profile_default(), sigurdos::radio_profile_at(0));
    EXPECT_EQ(nullptr, sigurdos::radio_profile_at(count));
}

TEST(RadioProfilesTest, CanadaUsesKrabOsProductionTuple) {
    const auto* us = sigurdos::radio_profile_find("us_902_928");
    const auto* ca = sigurdos::radio_profile_find("ca_902_928");

    ASSERT_NE(nullptr, us);
    ASSERT_NE(nullptr, ca);
    EXPECT_NE(us->freq_mhz, ca->freq_mhz);
    EXPECT_FLOAT_EQ(910.525f, ca->freq_mhz);
    EXPECT_FLOAT_EQ(62.5f, ca->bw_khz);
    EXPECT_EQ(7, ca->sf);
    EXPECT_EQ(5, ca->cr);
    EXPECT_EQ(20, ca->tx_power_dbm);
    EXPECT_EQ(20, ca->max_tx_power_dbm);
}

TEST(RadioProfilesTest, ApplySetsPrefsAndKeepsTransmitGuardExplicit) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    ASSERT_FALSE(prefs.configured);

    const auto* ca = sigurdos::radio_profile_find("ca_902_928");
    ASSERT_NE(nullptr, ca);
    sigurdos::radio_profile_apply(*ca, prefs);

    EXPECT_TRUE(prefs.configured);
    EXPECT_STREQ("ca_902_928", prefs.radio_profile);
    EXPECT_FLOAT_EQ(910.525f, prefs.freq);
    EXPECT_FLOAT_EQ(62.5f, prefs.bw);
    EXPECT_EQ(7, prefs.sf);
    EXPECT_EQ(5, prefs.cr);
    EXPECT_EQ(20, prefs.tx_power_dbm);
}

TEST(RadioProfilesTest, SavedProfileBreaksTiesForIdenticalTuples) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    const auto* ca = sigurdos::radio_profile_find("ca_902_928");
    ASSERT_NE(nullptr, ca);
    sigurdos::radio_profile_apply(*ca, prefs);

    const auto* matched = sigurdos::radio_profile_match(prefs);

    ASSERT_NE(nullptr, matched);
    EXPECT_STREQ("ca_902_928", matched->id);
}

TEST(RadioProfilesTest, CustomMarksManualSettings) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    prefs.freq = 916.250f;
    prefs.bw = 125.0f;
    prefs.sf = 9;
    prefs.cr = 6;
    prefs.tx_power_dbm = 20;
    prefs.configured = true;

    EXPECT_EQ(nullptr, sigurdos::radio_profile_match(prefs));

    sigurdos::radio_profile_set_custom(prefs);
    EXPECT_STREQ("custom", prefs.radio_profile);
}

TEST(RadioProfilesTest, StatusLabelTracksNamedCustomAndUnconfiguredRadio) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    EXPECT_STREQ("Radio setup required",
                 sigurdos::radio_profile_status_label(prefs));

    const auto* us = sigurdos::radio_profile_find("us_902_928");
    ASSERT_NE(nullptr, us);
    sigurdos::radio_profile_apply(*us, prefs);
    EXPECT_STREQ("USA 902-928", sigurdos::radio_profile_status_label(prefs));

    prefs.freq = 916.250f;
    EXPECT_STREQ("Custom RF", sigurdos::radio_profile_status_label(prefs));
}

TEST(RadioProfilesTest, RepeatFrequencyUsesExactActiveProfileFrequency) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    const auto* uk = sigurdos::radio_profile_find("uk_869_525");
    ASSERT_NE(nullptr, uk);
    sigurdos::radio_profile_apply(*uk, prefs);

    uint32_t frequency_khz = 0;
    ASSERT_TRUE(sigurdos::radio_profile_repeat_frequency_khz(
        prefs, &frequency_khz));
    EXPECT_EQ(869525u, frequency_khz);
}

TEST(RadioProfilesTest, RepeatFrequencyRejectsUnconfiguredAndCustomRadio) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    uint32_t frequency_khz = 123;

    EXPECT_FALSE(sigurdos::radio_profile_repeat_frequency_khz(
        prefs, &frequency_khz));

    const auto* profile = sigurdos::radio_profile_default();
    ASSERT_NE(nullptr, profile);
    sigurdos::radio_profile_apply(*profile, prefs);
    sigurdos::radio_profile_set_custom(prefs);
    EXPECT_FALSE(sigurdos::radio_profile_repeat_frequency_khz(
        prefs, &frequency_khz));
}

TEST(RadioProfilesTest, RepeatFrequencyRejectsProfileTupleDrift) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    const auto* profile = sigurdos::radio_profile_find("eu_868");
    ASSERT_NE(nullptr, profile);
    sigurdos::radio_profile_apply(*profile, prefs);
    prefs.freq = 868.100f;

    uint32_t frequency_khz = 0;
    EXPECT_FALSE(sigurdos::radio_profile_repeat_frequency_khz(
        prefs, &frequency_khz));
}

TEST(RadioProfilesTest, CompanionRepeatRangesMatchCompileTimePolicy) {
    uint32_t pairs[16]{};
    const size_t count = sigurdos::radio_profile_repeat_frequency_ranges(pairs, 8);

    ASSERT_EQ(count, 3u);
    EXPECT_EQ(pairs[0], 433000u);
    EXPECT_EQ(pairs[1], 433000u);
    EXPECT_EQ(pairs[2], 869495u);
    EXPECT_EQ(pairs[3], 869495u);
    EXPECT_EQ(pairs[4], 910525u);
    EXPECT_EQ(pairs[5], 910525u);
}

TEST(RadioProfilesTest, CompanionRepeatAcceptanceUsesAdvertisedFrequencyUnion) {
    EXPECT_TRUE(sigurdos::radio_profile_repeat_frequency_allowed(433000));
    EXPECT_TRUE(sigurdos::radio_profile_repeat_frequency_allowed(910525));
    EXPECT_FALSE(sigurdos::radio_profile_repeat_frequency_allowed(918000));
    EXPECT_FALSE(sigurdos::radio_profile_repeat_frequency_allowed(869500));
}

TEST(RadioProfilesTest, RegulatoryProfileFrequencyBoundariesAreInclusive) {
    const auto* us = sigurdos::radio_profile_find("us_902_928");
    const auto* eu = sigurdos::radio_profile_find("eu_868");
    ASSERT_NE(nullptr, us);
    ASSERT_NE(nullptr, eu);

    EXPECT_TRUE(sigurdos::radio_profile_frequency_allowed(*us, 902.0f));
    EXPECT_TRUE(sigurdos::radio_profile_frequency_allowed(*us, 928.0f));
    EXPECT_FALSE(sigurdos::radio_profile_frequency_allowed(*us, 901.999f));
    EXPECT_FALSE(sigurdos::radio_profile_frequency_allowed(*us, 928.001f));
    EXPECT_TRUE(sigurdos::radio_profile_frequency_allowed(*eu, 863.0f));
    EXPECT_TRUE(sigurdos::radio_profile_frequency_allowed(*eu, 870.0f));
    EXPECT_FALSE(sigurdos::radio_profile_frequency_allowed(*eu, 870.001f));
}

TEST(RadioProfilesTest, NamedProfileRejectsTupleDriftAndUnknownPolicy) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    const auto* profile = sigurdos::radio_profile_find("uk_869_525");
    ASSERT_NE(nullptr, profile);
    sigurdos::radio_profile_apply(*profile, prefs);
    EXPECT_TRUE(sigurdos::radio_profile_configuration_valid(prefs));

    prefs.freq = 869.600f;
    EXPECT_FALSE(sigurdos::radio_profile_configuration_valid(prefs));

    std::strncpy(prefs.radio_profile, "unknown", sizeof(prefs.radio_profile));
    prefs.radio_profile[sizeof(prefs.radio_profile) - 1] = '\0';
    EXPECT_FALSE(sigurdos::radio_profile_configuration_valid(prefs));

    sigurdos::radio_profile_set_custom(prefs);
    EXPECT_TRUE(sigurdos::radio_profile_configuration_valid(prefs));
}

} // namespace
