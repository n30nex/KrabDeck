#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#ifdef ESP32_PLATFORM
#include <driver/rtc_io.h>
#endif
#include "tdeck_pins.h"

namespace slopos {

class TDeckBoard : public mesh::MainBoard {
    uint8_t  _startup_reason;
    bool     _inhibit_sleep;

public:
    TDeckBoard() : _startup_reason(BD_STARTUP_NORMAL), _inhibit_sleep(false) {}

    void begin() {
        _startup_reason = BD_STARTUP_NORMAL;

        // Enable peripheral power
        pinMode(PIN_PERIPH_PWR, OUTPUT);
        digitalWrite(PIN_PERIPH_PWR, HIGH);

        // Trackball button as input
        pinMode(PIN_TRACKBALL, INPUT_PULLUP);

        // LoRa DIO1 pullup
        pinMode(PIN_LORA_DIO1, INPUT_PULLUP);

        // Battery ADC
        analogReadResolution(12);
        adcAttachPin(PIN_BAT_ADC);

        // I2C for touch / RTC
        Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    }

    uint16_t getBattMilliVolts() override {
        uint32_t raw = 0;
        for (int i = 0; i < 8; i++) {
            raw += analogRead(PIN_BAT_ADC);
        }
        raw /= 8;
        return (uint16_t)((BAT_ADC_MULT * (float)raw) / 4096.0f);
    }

    float getMCUTemperature() override {
        uint32_t raw = 0;
        for (int i = 0; i < 4; i++) raw += temperatureRead();
        return (float)raw / 4.0f;
    }

    const char* getManufacturerName() const override {
        return "LilyGo T-Deck";
    }

    uint8_t getStartupReason() const override {
        return _startup_reason;
    }

    void reboot() override {
        esp_restart();
    }

    void sleep(uint32_t secs) override {
        if (_inhibit_sleep) return;
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        rtc_gpio_set_direction((gpio_num_t)PIN_LORA_DIO1, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pulldown_en((gpio_num_t)PIN_LORA_DIO1);
        rtc_gpio_hold_en((gpio_num_t)PIN_LORA_NSS);

        esp_sleep_enable_ext1_wakeup((1LL << PIN_LORA_DIO1), ESP_EXT1_WAKEUP_ANY_HIGH);
        if (secs > 0) {
            esp_sleep_enable_timer_wakeup(secs * 1000000ULL);
        }
        esp_deep_sleep_start();
    }

    void setInhibitSleep(bool inhibit) { _inhibit_sleep = inhibit; }
};

} // namespace slopos
