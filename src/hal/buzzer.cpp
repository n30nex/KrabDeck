// SPDX-License-Identifier: GPL-3.0-or-later

#include "buzzer.h"
#include "tdeck_pins.h"
#include <Arduino.h>

#ifndef PLATFORMIO_UNIT_TESTING

namespace sigurdos {
namespace hal {

namespace {

void buzzer_play_pattern(BuzzerPatternKind kind) {
    std::size_t count = 0;
    const BuzzerPatternStep* pattern = sigurdos_buzzer_pattern(kind, &count);
    for (std::size_t i = 0; i < count; ++i) {
        digitalWrite(PIN_BUZZER, pattern[i].level_high ? HIGH : LOW);
        if (pattern[i].duration_ms > 0) {
            delay(pattern[i].duration_ms);
        }
    }
}

} // namespace

void buzzer_init() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

void buzzer_beep_short() {
    buzzer_play_pattern(BuzzerPatternKind::Short);
}

void buzzer_beep_double() {
    buzzer_play_pattern(BuzzerPatternKind::Double);
}

} // namespace hal
} // namespace sigurdos

#else  // PLATFORMIO_UNIT_TESTING - no-op stubs

namespace sigurdos {
namespace hal {
void buzzer_init() {}
void buzzer_beep_short() {}
void buzzer_beep_double() {}
} // namespace hal
} // namespace sigurdos

#endif
