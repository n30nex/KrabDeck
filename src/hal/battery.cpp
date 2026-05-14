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


#include "battery.h"
#include "tdeck_pins.h"
#include <Arduino.h>

void slopos_battery_init()
{
    // ADC pin already configured by TDeckBoard::begin()
    // (analogReadResolution(12), adcAttachPin, pinMode)
}

uint16_t slopos_battery_mv()
{
    const int samples = 8;
    uint32_t raw = 0;
    for (int i = 0; i < samples; i++) {
        raw += analogRead(PIN_BAT_ADC);
    }
    raw /= samples;
    return (uint16_t)((BAT_ADC_MULT * (float)raw) / 4096.0f);
}

uint8_t slopos_battery_pct()
{
    int32_t mv = (int32_t)slopos_battery_mv();
    if (mv <= BAT_MIN_MV) return 0;
    if (mv >= BAT_MAX_MV) return 100;
    return (uint8_t)(((mv - BAT_MIN_MV) * 100) / (BAT_MAX_MV - BAT_MIN_MV));
}

bool slopos_battery_charging()
{
    // T-Deck doesn't have a dedicated charge-detect pin
    return false;
}
