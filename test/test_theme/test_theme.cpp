/**
 * Unit tests for theme color constants
 * Validates the Discord-inspired dark palette
 */
#include <gtest/gtest.h>
#include <cstdint>

// Include the actual theme header (compile-time constant validation)
#include "ui/theme.h"

namespace {

class ThemeTest : public ::testing::Test {};

// ── Background colors are dark ──────────────────────────
TEST_F(ThemeTest, BackgroundColorsAreDark) {
    using namespace slopos::theme;
    // All background colors should have low brightness (< 0x30 per channel)
    auto is_dark = [](uint32_t c) {
        return ((c >> 16) & 0xFF) < 0x30 &&
               ((c >> 8)  & 0xFF) < 0x60 &&
               (c & 0xFF)        <= 0x60;
    };
    EXPECT_TRUE(is_dark(BG_PRIMARY));
    EXPECT_TRUE(is_dark(BG_SECONDARY));
    EXPECT_TRUE(is_dark(BG_TERTIARY));
    EXPECT_TRUE(is_dark(BG_INPUT));
}

// ── Accent colors are vibrant ───────────────────────────
TEST_F(ThemeTest, AccentColorsAreBright) {
    using namespace slopos::theme;
    // Accent colors should have at least one channel > 0xA0
    auto is_vibrant = [](uint32_t c) {
        return ((c >> 16) & 0xFF) > 0xA0 ||
               ((c >> 8)  & 0xFF) > 0xA0 ||
               (c & 0xFF)        > 0xA0;
    };
    EXPECT_TRUE(is_vibrant(ACCENT));
    EXPECT_TRUE(is_vibrant(ACCENT_GREEN));
    EXPECT_TRUE(is_vibrant(ACCENT_RED));
    EXPECT_TRUE(is_vibrant(ACCENT_ORANGE));
}

// ── Text colors are readable ─────────────────────────────
TEST_F(ThemeTest, TextColorsAreLight) {
    using namespace slopos::theme;
    auto is_light = [](uint32_t c) {
        int sum = ((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF);
        return sum > 350; // average > ~117 per channel
    };
    EXPECT_TRUE(is_light(TEXT_PRIMARY));
}

TEST_F(ThemeTest, MutedTextIsDimmerThanPrimary) {
    using namespace slopos::theme;
    auto brightness = [](uint32_t c) {
        return ((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF);
    };
    EXPECT_GT(brightness(TEXT_PRIMARY), brightness(TEXT_SECONDARY));
    EXPECT_GT(brightness(TEXT_SECONDARY), brightness(TEXT_MUTED));
}

// ── All constants are non-zero ──────────────────────────
TEST_F(ThemeTest, AllConstantsNonZero) {
    using namespace slopos::theme;
    EXPECT_NE(BG_PRIMARY,     0U);
    EXPECT_NE(BG_SECONDARY,   0U);
    EXPECT_NE(BG_TERTIARY,    0U);
    EXPECT_NE(BG_INPUT,       0U);
    EXPECT_NE(ACCENT,         0U);
    EXPECT_NE(ACCENT_HOVER,   0U);
    EXPECT_NE(ACCENT_GREEN,   0U);
    EXPECT_NE(ACCENT_RED,     0U);
    EXPECT_NE(ACCENT_ORANGE,  0U);
    EXPECT_NE(ACCENT_YELLOW,  0U);
    EXPECT_NE(TEXT_PRIMARY,   0U);
    EXPECT_NE(TEXT_SECONDARY, 0U);
    EXPECT_NE(TEXT_MUTED,     0U);
    EXPECT_NE(TEXT_LINK,      0U);
    EXPECT_NE(CHANNEL_HASH,   0U);
    EXPECT_NE(CHANNEL_ACTIVE, 0U);
}

// ── Color uniqueness ────────────────────────────────────
TEST_F(ThemeTest, AccentColorsAreDistinct) {
    using namespace slopos::theme;
    EXPECT_NE(ACCENT, ACCENT_GREEN);
    EXPECT_NE(ACCENT, ACCENT_RED);
    EXPECT_NE(ACCENT_GREEN, ACCENT_RED);
    EXPECT_NE(ACCENT_ORANGE, ACCENT_YELLOW);
}

TEST_F(ThemeTest, BackgroundColorsAreDistinct) {
    using namespace slopos::theme;
    EXPECT_NE(BG_PRIMARY, BG_SECONDARY);
    EXPECT_NE(BG_PRIMARY, BG_TERTIARY);
    EXPECT_NE(BG_SECONDARY, BG_TERTIARY);
}

} // anonymous namespace
