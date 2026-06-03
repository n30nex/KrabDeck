// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Layout regression tests for fixed 320x240 T-Deck screens.

#include <gtest/gtest.h>
#include "ui/responsive.h"

namespace {

using namespace sigurdos::responsive;

constexpr int kStackGap = 4;
constexpr int kFooterBase = BOT_BAR_H + DIVIDER_H;

constexpr int top_for(int row_h, int offset) {
    return bottom_aligned_top(row_h, offset);
}

constexpr int bottom_for(int row_h, int offset) {
    return top_for(row_h, offset) + row_h;
}

static void expect_separated(int lower_h, int lower_offset, int upper_h, int upper_offset) {
    EXPECT_LE(bottom_for(upper_h, upper_offset) + kStackGap, top_for(lower_h, lower_offset));
}

TEST(LayoutTest, ContactDetailNonRoomRowsDoNotOverlap) {
    constexpr bool is_room = false;
    constexpr bool logged_in = false;

    constexpr int list_bottom = CONTENT_Y + CONTENT_H - contact_bottom_reserved(is_room, logged_in);
    EXPECT_LE(list_bottom + 2, top_for(22, contact_acl_offset(is_room, logged_in)));

    expect_separated(26, contact_qr_offset(is_room, logged_in), 22, contact_acl_offset(is_room, logged_in));
    expect_separated(30, 64, 26, contact_qr_offset(is_room, logged_in));      // reset/discover row
    expect_separated(26, 34, 30, 64);                                         // telemetry row
    expect_separated(30, 0, 26, 34);                                          // main action row
}

TEST(LayoutTest, ContactDetailRoomLoggedOutRowsDoNotOverlap) {
    constexpr bool is_room = true;
    constexpr bool logged_in = false;

    constexpr int list_bottom = CONTENT_Y + CONTENT_H - contact_bottom_reserved(is_room, logged_in);
    EXPECT_LE(list_bottom + 2, top_for(22, contact_acl_offset(is_room, logged_in)));

    expect_separated(26, contact_qr_offset(is_room, logged_in), 22, contact_acl_offset(is_room, logged_in));
    expect_separated(30, 34, 26, contact_qr_offset(is_room, logged_in));       // login row
    expect_separated(30, 0, 30, 34);                                          // main action row
}

TEST(LayoutTest, ContactDetailRoomLoggedInRowsDoNotOverlap) {
    constexpr bool is_room = true;
    constexpr bool logged_in = true;

    constexpr int list_bottom = CONTENT_Y + CONTENT_H - contact_bottom_reserved(is_room, logged_in);
    EXPECT_LE(list_bottom + 2, top_for(22, contact_acl_offset(is_room, logged_in)));

    expect_separated(26, contact_qr_offset(is_room, logged_in), 22, contact_acl_offset(is_room, logged_in));
    expect_separated(30, 68, 26, contact_qr_offset(is_room, logged_in));       // fetch row
    expect_separated(30, 34, 30, 68);                                         // login/admin row
    expect_separated(30, 0, 30, 34);                                          // main action row
}

TEST(LayoutTest, ConfiguredSignalChartDoesNotOverlapMetrics) {
    constexpr int metrics_y = CONTENT_Y + 4;
    constexpr int metrics_pad = 4;
    constexpr int font10_line_h = 11;
    constexpr int metrics_lines = 6;
    constexpr int metrics_bottom = metrics_y + metrics_pad * 2 + font10_line_h * metrics_lines;

    constexpr int chart_y = CONTENT_Y + 4 + 112;
    constexpr int chart_h = 60;
    constexpr int label_h = 15;  // lv_font_montserrat_12 line_height
    constexpr int chart_label_top = chart_y - 2 - label_h;

    EXPECT_LE(metrics_bottom + kStackGap, chart_label_top);
    EXPECT_LE(chart_y + chart_h, DISPLAY_H - kFooterBase);
}

} // namespace
