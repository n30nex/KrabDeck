#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Comprehensive device-wide debug diagnostics for SlopOS-TDeck.
// Enabled by building with -D SLOPOS_DEBUG=1 (see platformio.ini debug env).
//
// When enabled, Serial output includes:
//   - Periodic full system dumps (every N seconds)
//   - LVGL rendering detail on every invalidation/flush
//   - Trackball GPIO state on every scan
//   - Home screen tile/layout diagnostics
//   - Memory, task, and timing info
//
// Usage: connect USB serial at 115200 baud, then interact with the device.
//        Warps/corruption will be visible in the dump output.

#include <cstdint>

namespace slopos {
namespace debug {

void init();
void loop();

void dump_system();
void dump_lvgl_rendering();
void dump_trackball_state();
void dump_home_screen_layout();
void dump_memory();
void dump_display_config();
void dump_mesh_state();

} // namespace debug
} // namespace slopos