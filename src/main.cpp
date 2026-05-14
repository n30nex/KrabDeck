#include <Arduino.h>
#include "hal/tdeck_pins.h"
#include "hal/display.h"

// Forward declares from UI module (to be implemented)
namespace slopos::ui {
    void init();
    void loop();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Enable peripheral power
    pinMode(PIN_PERIPH_PWR, OUTPUT);
    digitalWrite(PIN_PERIPH_PWR, HIGH);

    // Battery ADC
    pinMode(PIN_BAT_ADC, INPUT);
    analogReadResolution(12);
    adcAttachPin(PIN_BAT_ADC);

    // Trackball button
    pinMode(PIN_TRACKBALL, INPUT_PULLUP);

    // Initialize display + LVGL
    if (!slopos_display_init()) {
        Serial.println("FATAL: Display init failed");
        while (1) delay(1000);
    }

    Serial.println("SlopOS T-Deck booted");
    slopos::ui::init();
}

void loop()
{
    slopos_display_loop();
    slopos::ui::loop();
}
