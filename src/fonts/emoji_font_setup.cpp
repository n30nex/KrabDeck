// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SlopOS-TDeck.
//
// SlopOS-TDeck is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SlopOS-TDeck is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SlopOS-TDeck.  If not, see <https://www.gnu.org/licenses/>.

#include "emoji_font.h"
#include <lvgl.h>
#include <cstring>

// Fallback registration must be extern "C" to match the C declaration
// Writable font wrappers — copies of Montserrat with emoji fallback set.
// Cannot set fallback directly on the const LVGL Montserrat fonts in flash,
// so we create writable copies in RAM at init time.

static lv_font_t wrapped_10;
static lv_font_t wrapped_12;
static lv_font_t wrapped_14;
static lv_font_t wrapped_16;
static lv_font_t wrapped_18;
static lv_font_t wrapped_20;
static lv_font_t wrapped_24;
static lv_font_t wrapped_28;

// Expose wrapped fonts for use in UI code
const lv_font_t* emoji_wrapped_montserrat_10 = &wrapped_10;
const lv_font_t* emoji_wrapped_montserrat_12 = &wrapped_12;
const lv_font_t* emoji_wrapped_montserrat_14 = &wrapped_14;
const lv_font_t* emoji_wrapped_montserrat_16 = &wrapped_16;
const lv_font_t* emoji_wrapped_montserrat_18 = &wrapped_18;
const lv_font_t* emoji_wrapped_montserrat_20 = &wrapped_20;
const lv_font_t* emoji_wrapped_montserrat_24 = &wrapped_24;
const lv_font_t* emoji_wrapped_montserrat_28 = &wrapped_28;

extern "C" void emoji_font_register_fallback()
{
    // Copy const Montserrat fonts into writable wrappers and set fallback
    memcpy(&wrapped_10, &lv_font_montserrat_10, sizeof(lv_font_t));
    wrapped_10.fallback = &emoji_font;

    memcpy(&wrapped_12, &lv_font_montserrat_12, sizeof(lv_font_t));
    wrapped_12.fallback = &emoji_font;

    memcpy(&wrapped_14, &lv_font_montserrat_14, sizeof(lv_font_t));
    wrapped_14.fallback = &emoji_font;

    memcpy(&wrapped_16, &lv_font_montserrat_16, sizeof(lv_font_t));
    wrapped_16.fallback = &emoji_font;

    memcpy(&wrapped_18, &lv_font_montserrat_18, sizeof(lv_font_t));
    wrapped_18.fallback = &emoji_font;

    memcpy(&wrapped_20, &lv_font_montserrat_20, sizeof(lv_font_t));
    wrapped_20.fallback = &emoji_font;

    memcpy(&wrapped_24, &lv_font_montserrat_24, sizeof(lv_font_t));
    wrapped_24.fallback = &emoji_font;

    memcpy(&wrapped_28, &lv_font_montserrat_28, sizeof(lv_font_t));
    wrapped_28.fallback = &emoji_font;
}
