#pragma once
// Mock LovyanGFX.hpp for native PlatformIO testing

#define LGFX_USE_V1

#include <cstdint>

namespace lgfx {
    typedef uint16_t rgb565_t;

    class LGFX_Device {
    public:
        void init() {}
        void setRotation(int) {}
        void setBrightness(int) {}
        void fillScreen(int) {}
        void startWrite() {}
        void endWrite() {}
        void setAddrWindow(int, int, int, int) {}
        void writePixels(rgb565_t*, int) {}
    };

    class Panel_ST7789 {
    public:
        void setBus(class Bus_SPI*) {}
        struct config_t {
            int pin_cs, pin_rst;
            int panel_width, panel_height;
            int offset_x, offset_y, offset_rotation;
            bool invert, rgb_order;
            int memory_width, memory_height;
        };
        void config(const config_t&) {}
        config_t config() { return {}; }
    };

    class Bus_SPI {
    public:
        struct config_t {
            int spi_host, spi_mode;
            int freq_write, freq_read;
            bool spi_3wire, use_lock;
            int dma_channel;
            int pin_sclk, pin_mosi, pin_miso, pin_dc;
        };
        void config(const config_t&) {}
        config_t config() { return {}; }
    };

    class Light_PWM {
    public:
        struct config_t {
            int pin_bl;
            bool invert;
            int freq;
            int pwm_channel;
        };
        void config(const config_t&) {}
        config_t config() { return {}; }
    };
}

using LGFX = lgfx::LGFX_Device;

#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
