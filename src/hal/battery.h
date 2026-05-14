#pragma once
#include <cstdint>

void slopos_battery_init();
uint16_t slopos_battery_mv();        // millivolts
uint8_t  slopos_battery_pct();       // 0-100
bool     slopos_battery_charging();
