#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <cstdint>

enum class SlopOSTrackballEvent : uint8_t {
    None = 0,
    Up,
    Down,
    Left,
    Right,
    Click,
};

bool slopos_trackball_init();
void slopos_trackball_scan();
bool slopos_trackball_next_event(SlopOSTrackballEvent* out);

// Reset internal debouncing/repeat state. Useful for tests and wake recovery.
void slopos_trackball_reset_scan_state();

// Inject a simulated trackball event into the queue (for remote test mode).
// The event will be consumed by the normal trackball read path.
void slopos_trackball_inject(SlopOSTrackballEvent event);
