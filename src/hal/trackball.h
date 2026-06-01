#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <cstdint>

enum class SigurdOSTrackballEvent : uint8_t {
    None = 0,
    Up,
    Down,
    Left,
    Right,
    Click,
};

bool sigurdos_trackball_init();
void sigurdos_trackball_scan();
bool sigurdos_trackball_next_event(SigurdOSTrackballEvent* out);

// Reset internal debouncing/repeat state. Useful for tests and wake recovery.
void sigurdos_trackball_reset_scan_state();

// Inject a simulated trackball event into the queue (for remote test mode).
// The event will be consumed by the normal trackball read path.
void sigurdos_trackball_inject(SigurdOSTrackballEvent event);
