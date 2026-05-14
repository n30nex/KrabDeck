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
