#include "ui.h"
#include <lvgl.h>

namespace slopos {
namespace ui {

static lv_obj_t* splash_label = nullptr;

void init()
{
    // Splash screen — dark background centered text
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    splash_label = lv_label_create(scr);
    lv_label_set_text(splash_label, "SlopOS\nT-Deck");
    lv_obj_set_style_text_color(splash_label, lv_color_hex(0x5865f2), 0);
    lv_obj_set_style_text_font(splash_label, &lv_font_montserrat_28, 0);
    lv_obj_center(splash_label);
    lv_label_set_align(splash_label, LV_TEXT_ALIGN_CENTER);
}

void loop()
{
    // UI loop is driven by lv_timer_handler() in display.cpp
}

} // namespace ui
} // namespace slopos
