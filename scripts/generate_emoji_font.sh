#!/usr/bin/env bash
set -euo pipefail
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Ben
#
# This script downloads Noto Emoji font (SIL OFL) and generates
# an LVGL-compatible C source with a subset of common emoji.
# Requires: curl, lv_font_conv (npm install -g lv_font_conv)

FONT_URL="https://github.com/google/fonts/raw/main/ofl/notoemoji/NotoEmoji%5Bwght%5D.ttf"
FONT_TMP="/tmp/NotoEmoji.ttf"
OUTPUT_DIR="$(dirname "$0")/../src/fonts"
OUTPUT_C="${OUTPUT_DIR}/emoji_font.c"
OUTPUT_H="${OUTPUT_DIR}/emoji_font.h"
FONT_SIZE=12
BPP=1

# Common emoji subset (Unicode codepoints)
# Faces
RANGES="-r 0x1F600-0x1F603"  # 😀😁😂🤣 -> range: U+1F600..U+1F603 covers 😀😁😂 + U+1F923 is 🤣 (separate)
RANGES+=" -r 0x1F923"         # 🤣
RANGES+=" -r 0x1F60A"         # 😊
RANGES+=" -r 0x1F60D"         # 😍
RANGES+=" -r 0x1F60E"         # 😎
RANGES+=" -r 0x1F914"         # 🤔
RANGES+=" -r 0x1F60F"         # 😏
RANGES+=" -r 0x1F62E"         # 😮
RANGES+=" -r 0x1F622"         # 😢
RANGES+=" -r 0x1F62D"         # 😭
RANGES+=" -r 0x1F624"         # 😤
RANGES+=" -r 0x1F621"         # 😡
RANGES+=" -r 0x1F970"         # 🥰

# Hands
RANGES+=" -r 0x1F44D"         # 👍
RANGES+=" -r 0x1F44E"         # 👎
RANGES+=" -r 0x1F44C"         # 👌
RANGES+=" -r 0x270C"          # ✌️
RANGES+=" -r 0x1F44F"         # 👏
RANGES+=" -r 0x1F64C"         # 🙌
RANGES+=" -r 0x1F64F"         # 🙏
RANGES+=" -r 0x1F4AA"         # 💪
RANGES+=" -r 0x1F91D"         # 🤝
RANGES+=" -r 0x1F44B"         # 👋

# Hearts
RANGES+=" -r 0x2764"          # ❤
RANGES+=" -r 0x1F9E1"         # 🧡
RANGES+=" -r 0x1F49B"         # 💛
RANGES+=" -r 0x1F49A"         # 💚
RANGES+=" -r 0x1F499"         # 💙
RANGES+=" -r 0x1F49C"         # 💜
RANGES+=" -r 0x1F5A4"         # 🖤
RANGES+=" -r 0x1F495"         # 💕
RANGES+=" -r 0x1F49E"         # 💞
RANGES+=" -r 0x1F493"         # 💓

# Objects / Symbols
RANGES+=" -r 0x1F525"         # 🔥
RANGES+=" -r 0x1F389"         # 🎉
RANGES+=" -r 0x1F38A"         # 🎊
RANGES+=" -r 0x2705"          # ✅
RANGES+=" -r 0x274C"          # ❌
RANGES+=" -r 0x1F4AF"         # 💯
RANGES+=" -r 0x2B50"          # ⭐
RANGES+=" -r 0x1F680"         # 🚀
RANGES+=" -r 0x1F388"         # 🎈
RANGES+=" -r 0x1F4A1"         # 💡
RANGES+=" -r 0x1F514"         # 🔔
RANGES+=" -r 0x1F3AF"         # 🎯
RANGES+=" -r 0x1F50B"         # 🔋
RANGES+=" -r 0x2699"          # ⚙
RANGES+=" -r 0x1F4E1"         # 📡
RANGES+=" -r 0x1F30D"         # 🌍

echo "Downloading Noto Emoji..."
curl -sL "$FONT_URL" -o "$FONT_TMP"
echo "Generating emoji font (size=${FONT_SIZE}, bpp=${BPP})..."
lv_font_conv \
    --font "$FONT_TMP" \
    --size $FONT_SIZE \
    --bpp $BPP \
    --format lvgl \
    --output "$OUTPUT_C" \
    --lv-include 'lvgl.h' \
    --lv-font-name emoji_font \
    $RANGES

# Fix the include directive: the generated code has #include "." in the else branch
# when --lv-include is set correctly it should be fine.

echo "Generating header..."
cat > "$OUTPUT_H" << 'HEADER'
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Emoji font with commonly used emoji subset (Noto Emoji, SIL OFL)
// Automatically generated — see scripts/generate_emoji_font.sh
extern const lv_font_t emoji_font;

// Register emoji_font as fallback for all Montserrat fonts used in the UI.
// Call once during UI initialization.
void emoji_font_register_fallback();

#ifdef __cplusplus
}
#endif
HEADER

echo "Done! Generated:"
echo "  $OUTPUT_C"
echo "  $OUTPUT_H"
