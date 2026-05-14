#pragma once
// Mock Arduino.h for native PlatformIO testing
// Provides minimal stubs for all Arduino functions used by SlopOS

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cmath>

// ── Types ────────────────────────────────────────────────
using byte = uint8_t;
using word = uint16_t;

typedef bool boolean;

// ── Pin modes ────────────────────────────────────────────
#define INPUT          0x01
#define OUTPUT         0x02
#define INPUT_PULLUP   0x05
#define INPUT_PULLDOWN 0x06

#define HIGH  1
#define LOW   0

#define LED_BUILTIN 2

// ── Mock state (manipulated by tests) ────────────────────
namespace arduino_mock {
    extern unsigned long current_millis;
    extern int pin_states[64];       // digital pin states
    extern int analog_values[16];    // analog pin readings
    extern bool serial_output[1024]; // serial TX buffer
    extern int serial_output_len;

    void reset();  // reset all mock state between tests
}

// ── Core functions ───────────────────────────────────────
inline unsigned long millis() { return arduino_mock::current_millis; }
inline unsigned long micros() { return arduino_mock::current_millis * 1000UL; }
inline void delay(unsigned long ms) { arduino_mock::current_millis += ms; }
inline void delayMicroseconds(unsigned int us) { arduino_mock::current_millis += us / 1000 + 1; }

// ── Digital I/O ──────────────────────────────────────────
inline void pinMode(uint8_t pin, uint8_t mode) {
    if (pin < 64) arduino_mock::pin_states[pin] = mode;
}
inline void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin < 64) arduino_mock::pin_states[pin] = val;
}
inline int digitalRead(uint8_t pin) {
    return (pin < 64) ? arduino_mock::pin_states[pin] : 0;
}

// ── Analog I/O ───────────────────────────────────────────
inline void analogReadResolution(int bits) { (void)bits; }
inline int analogRead(uint8_t pin) {
    return (pin < 16) ? arduino_mock::analog_values[pin] : 0;
}
inline void adcAttachPin(uint8_t pin) { (void)pin; }

// ── Serial ───────────────────────────────────────────────
class Stream {
public:
    virtual ~Stream() = default;
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() {}
    virtual size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t* buf, size_t len) { (void)buf; return len; }
};

class HardwareSerial : public Stream {
public:
    void begin(unsigned long) {}
    void print(const char*) {}
    void println(const char*) {}
    void printf(const char*, ...) {}
    operator bool() const { return true; }
};
extern HardwareSerial Serial;
extern HardwareSerial Serial1;

// ── I2C ──────────────────────────────────────────────────
class TwoWire {
public:
    void begin() {}
    void begin(int, int) {}
};
extern TwoWire Wire;

// ── SPI ──────────────────────────────────────────────────
#define VSPI_HOST 2
#define SPI_DMA_CH_AUTO 0

class SPIClass {
public:
    void begin() {}
};
extern SPIClass SPI;

// ── ESP32 specific ───────────────────────────────────────
inline float temperatureRead() { return 45.0f; }
inline void esp_restart() {}
inline void esp_deep_sleep_start() {}

// Reset reasons
#define ESP_RST_POWERON   1
#define ESP_RST_DEEPSLEEP 5

typedef int esp_reset_reason_t;
inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }

// RTC GPIO (stubs for deep sleep)
#define RTC_GPIO_MODE_INPUT_ONLY 0
typedef int gpio_num_t;
typedef int rtc_gpio_mode_t;
inline void rtc_gpio_set_direction(gpio_num_t, rtc_gpio_mode_t) {}
inline void rtc_gpio_pulldown_en(gpio_num_t) {}
inline void rtc_gpio_hold_en(gpio_num_t) {}
inline void rtc_gpio_hold_dis(gpio_num_t) {}
inline void rtc_gpio_deinit(gpio_num_t) {}

#define ESP_PD_DOMAIN_RTC_PERIPH 0
#define ESP_PD_OPTION_ON 0
typedef int esp_sleep_pd_domain_t;
inline void esp_sleep_pd_config(esp_sleep_pd_domain_t, int) {}

#define ESP_EXT1_WAKEUP_ANY_HIGH 0
inline void esp_sleep_enable_ext1_wakeup(uint64_t, int) {}
inline void esp_sleep_enable_timer_wakeup(uint64_t) {}

// ── Utilities ────────────────────────────────────────────
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define map(value, fromLow, fromHigh, toLow, toHigh) \
    ((value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow)
