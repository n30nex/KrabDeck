#pragma once

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

#include <cstdint>

// Initialize the GT911 touch controller over I2C
// Must be called after Wire.begin() and before LVGL init
// Returns true on successful initialization
bool sigurdos_touch_init();

// Call this each frame to poll for new touch data
// (called from sigurdos_display_loop)
void sigurdos_touch_loop();

// Get the current touch state
// Returns true if a touch is active, and fills x/y with position
// Coordinates are already mapped to display space (0-319, 0-239)
bool sigurdos_touch_get(int* out_x, int* out_y, bool* out_pressed);

// Is the touch controller initialized and responding?
bool sigurdos_touch_ready();
