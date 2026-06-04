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

#include <gtest/gtest.h>

#include "hal/display.h"
#include "hal/gps.h"

namespace {

TEST(HalContractTest, DisplayLifecycleAndPowerApisStayStable) {
    using init_fn = bool (*)();
    using void_fn = void (*)();
    using millis_fn = uint32_t (*)();
    using brightness_fn = void (*)(uint8_t);
    using bool_fn = bool (*)();

    (void)static_cast<init_fn>(sigurdos_display_init);
    (void)static_cast<void_fn>(sigurdos_display_loop);
    (void)static_cast<millis_fn>(sigurdos_display_millis);
    (void)static_cast<void_fn>(sigurdos_display_wake);
    (void)static_cast<bool_fn>(sigurdos_display_is_on);
    (void)static_cast<brightness_fn>(sigurdos_display_set_brightness);
    (void)static_cast<void_fn>(sigurdos_display_reset_auto_off);
    SUCCEED();
}

TEST(HalContractTest, DisplayDebugCaptureApisStayStable) {
    using buffer_fn = void* (*)();
    using dimension_fn = uint32_t (*)();
    using void_fn = void (*)();

    (void)static_cast<buffer_fn>(sigurdos_display_get_buffer);
    (void)static_cast<dimension_fn>(sigurdos_display_get_width);
    (void)static_cast<dimension_fn>(sigurdos_display_get_height);
    (void)static_cast<void_fn>(sigurdos_display_capture_framebuffer);
    SUCCEED();
}

TEST(HalContractTest, GpsLifecycleAndFixApisStayStable) {
    using void_fn = void (*)();
    using float_fn = float (*)();
    using byte_fn = uint8_t (*)();
    using bool_fn = bool (*)();

    (void)static_cast<void_fn>(sigurdos_gps_init);
    (void)static_cast<void_fn>(sigurdos_gps_loop);
    (void)static_cast<float_fn>(sigurdos_gps_latitude);
    (void)static_cast<float_fn>(sigurdos_gps_longitude);
    (void)static_cast<float_fn>(sigurdos_gps_altitude_m);
    (void)static_cast<float_fn>(sigurdos_gps_speed_kn);
    (void)static_cast<float_fn>(sigurdos_gps_heading);
    (void)static_cast<byte_fn>(sigurdos_gps_satellites);
    (void)static_cast<byte_fn>(sigurdos_gps_fix_quality);
    (void)static_cast<bool_fn>(sigurdos_gps_has_fix);
    SUCCEED();
}

TEST(HalContractTest, GpsUtcTimeApisStayStable) {
    using byte_fn = uint8_t (*)();
    using bool_fn = bool (*)();

    (void)static_cast<byte_fn>(sigurdos_gps_hour);
    (void)static_cast<byte_fn>(sigurdos_gps_minute);
    (void)static_cast<byte_fn>(sigurdos_gps_second);
    (void)static_cast<bool_fn>(sigurdos_gps_time_synced);
    SUCCEED();
}

} // namespace
