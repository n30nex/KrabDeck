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

#include "tdeck_pins.h"
#include <cstdint>

inline uint16_t sigurdos_battery_mv_from_adc_raw(uint16_t raw)
{
    return (uint16_t)((BAT_ADC_MULT * (float)raw) / 4096.0f);
}

inline uint8_t sigurdos_battery_pct_from_mv(uint16_t mv)
{
    const int32_t m = (int32_t)mv;
    if (m <= BAT_MIN_MV) return 0;
    if (m >= BAT_MAX_MV) return 100;
    return (uint8_t)(((m - BAT_MIN_MV) * 100) / (BAT_MAX_MV - BAT_MIN_MV));
}

void sigurdos_battery_init();
uint16_t sigurdos_battery_mv();        // millivolts
uint8_t  sigurdos_battery_pct();       // 0-100
bool     sigurdos_battery_charging();
