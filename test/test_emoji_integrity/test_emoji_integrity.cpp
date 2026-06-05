// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <string>

#include "fonts/emoji_font.h"

namespace {

TEST(EmojiFontIntegrity, CountMatchesGeneratedList) {
    EXPECT_EQ(emoji_font_get_count(), 362);
}

TEST(EmojiFontIntegrity, EveryIndexedEntryIsPresent) {
    const int count = emoji_font_get_count();
    ASSERT_GT(count, 0);

    for (int i = 0; i < count; i++) {
        const char* value = emoji_font_get_by_index(i);
        ASSERT_NE(value, nullptr) << "index " << i;
        EXPECT_GT(strlen(value), 0u) << "index " << i;
    }
}

TEST(EmojiFontIntegrity, IndexedEntriesAreUnique) {
    const int count = emoji_font_get_count();
    ASSERT_GT(count, 0);

    std::set<std::string> seen;
    for (int i = 0; i < count; i++) {
        const char* value = emoji_font_get_by_index(i);
        ASSERT_NE(value, nullptr) << "index " << i;
        EXPECT_TRUE(seen.insert(value).second) << "duplicate at index " << i;
    }
    EXPECT_EQ(seen.size(), static_cast<size_t>(count));
}

TEST(EmojiFontIntegrity, BoundaryIndicesAreValidOnlyInsideRange) {
    const int count = emoji_font_get_count();
    ASSERT_GT(count, 0);

    EXPECT_EQ(emoji_font_get_by_index(-1), nullptr);
    EXPECT_NE(emoji_font_get_by_index(0), nullptr);
    EXPECT_NE(emoji_font_get_by_index(count - 1), nullptr);
    EXPECT_EQ(emoji_font_get_by_index(count), nullptr);
}

} // anonymous namespace
