#include "display.h"
#include "touch.h"
#include "keyboard.h"
#include "tdeck_pins.h"
#include <lvgl.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

static LGFX tft;
static lv_display_t* lv_disp = nullptr;
static lv_color_t draw_buf[TFT_WIDTH * 20];

// ── LovyanGFX configuration for T-Deck ST7789 ────────────
class LGFX_SlopOS : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;

public:
    LGFX_SlopOS()
    {
        {
            auto cfg = _bus.config();
            cfg.spi_host   = VSPI_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = PIN_TFT_SCL;
            cfg.pin_mosi   = PIN_TFT_SDA;
            cfg.pin_miso   = -1;
            cfg.pin_dc     = PIN_TFT_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs    = PIN_TFT_CS;
            cfg.pin_rst   = PIN_TFT_RST;
            cfg.panel_width  = TFT_WIDTH;
            cfg.panel_height = TFT_HEIGHT;
            cfg.offset_x  = 0;
            cfg.offset_y  = 0;
            cfg.offset_rotation = 0;
            cfg.invert    = true;
            cfg.rgb_order = false;
            cfg.memory_width  = 320;
            cfg.memory_height = 240;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl = PIN_TFT_BL;
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 0;
            _light.config(cfg);
        }
        setPanel(&_panel);
    }
};

// ── LVGL flush callback ─────────────────────────────────
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((lgfx::rgb565_t*)px_map, w * h);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

// ── Touch read callback ──────────────────────────────────
static void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    int x, y;
    bool pressed = false;
    if (slopos_touch_get(&x, &y, &pressed) && pressed) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ── Keyboard read callback ───────────────────────────────
static void lvgl_kb_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    uint32_t key = slopos_keyboard_get_key();
    if (key != 0 && slopos_keyboard_has_new_event()) {
        data->key = key;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ── Public API ───────────────────────────────────────────
bool slopos_display_init()
{
    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    lv_init();
    lv_disp = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    lv_display_set_buffers(lv_disp, draw_buf, nullptr, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, lvgl_touch_cb);

    lv_indev_t* kb = lv_indev_create();
    lv_indev_set_type(kb, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(kb, lvgl_kb_cb);

    // Initialize touch controller
    if (!slopos_touch_init()) {
        // Touch init failed — device works with keyboard only
    }

    // Initialize keyboard matrix scanner
    slopos_keyboard_init();

    return true;
}

void slopos_display_loop()
{
    slopos_touch_loop();
    slopos_keyboard_scan();
    uint32_t next = lv_timer_handler();
    delay(next > 5 ? 5 : next);
}

uint32_t slopos_display_millis()
{
    return millis();
}
