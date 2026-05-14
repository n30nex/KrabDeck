#include "ui.h"
#include "home_screen.h"
#include "theme.h"
#include <lvgl.h>

namespace slopos {
namespace ui {

static lv_obj_t* splash_scr = nullptr;
static uint32_t splash_start = 0;
static bool home_shown = false;

void init()
{
    // ── Splash Screen ─────────────────────────────────
    splash_scr = lv_obj_create(nullptr);
    theme::apply_dark_bg(splash_scr);

    // Logo text
    lv_obj_t* logo = lv_label_create(splash_scr);
    lv_label_set_text(logo, "SlopOS");
    lv_obj_set_style_text_color(logo, lv_color_hex(theme::ACCENT), 0);
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_28, 0);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -16);

    // Subtitle
    lv_obj_t* sub = lv_label_create(splash_scr);
    lv_label_set_text(sub, "T-Deck");
    lv_obj_set_style_text_color(sub, lv_color_hex(theme::TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_16, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 16);

    // Loading bar at bottom
    lv_obj_t* load_bar = lv_obj_create(splash_scr);
    lv_obj_set_size(load_bar, 200, 4);
    lv_obj_set_style_bg_color(load_bar, lv_color_hex(theme::BG_TERTIARY), 0);
    lv_obj_set_style_bg_opa(load_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(load_bar, 2, 0);
    lv_obj_set_style_border_width(load_bar, 0, 0);
    lv_obj_align(load_bar, LV_ALIGN_BOTTOM_MID, 0, -40);

    lv_obj_t* fill = lv_obj_create(load_bar);
    lv_obj_set_size(fill, 60, 4);
    lv_obj_set_style_bg_color(fill, lv_color_hex(theme::ACCENT), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(fill, 2, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);

    lv_scr_load(splash_scr);
    splash_start = millis();
    home_shown = false;
}

void loop()
{
    // Transition from splash to home after 2 seconds
    if (!home_shown && (millis() - splash_start > 2000)) {
        home_screen_create();
        lv_scr_load_anim(lv_scr_act(), LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        home_shown = true;

        // Clean up splash screen
        if (splash_scr) {
            lv_obj_del(splash_scr);
            splash_scr = nullptr;
        }
    }
}

} // namespace ui
} // namespace slopos
