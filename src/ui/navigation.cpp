#include "navigation.h"
#include "home_screen.h"
#include "chat_screen.h"
#include "theme.h"
#include <lvgl.h>

namespace slopos::ui {

static Screen current = Screen::Home;
static Screen previous = Screen::Home;

void navigate_to(Screen screen)
{
    if (screen == current) return;
    previous = current;
    current = screen;

    switch (screen) {
    case Screen::Home:
        home_screen_show();
        break;
    case Screen::Chat:
        chat_screen_show();
        break;
    case Screen::Contacts:
    case Screen::Repeaters:
    case Screen::Finder:
    case Screen::Heard:
    case Screen::Map:
    case Screen::Advertise:
    case Screen::Settings:
    case Screen::Trace:
    case Screen::Terminal:
    case Screen::Noise:
    case Screen::Signal:
        // Placeholder — show coming-soon overlay
        {
            lv_obj_t* scr = lv_obj_create(nullptr);
            theme::apply_dark_bg(scr);
            lv_obj_t* label = lv_label_create(scr);
            lv_label_set_text(label, "Coming soon");
            lv_obj_set_style_text_color(label, lv_color_hex(theme::TEXT_SECONDARY), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_obj_center(label);

            // Back button
            lv_obj_t* btn = lv_btn_create(scr);
            lv_obj_set_size(btn, 80, 30);
            lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
            lv_obj_t* btn_label = lv_label_create(btn);
            lv_label_set_text(btn_label, LV_SYMBOL_LEFT " Back");
            lv_obj_center(btn_label);

            lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
        }
        break;
    default:
        break;
    }
}

void go_back()
{
    navigate_to(previous);
}

} // namespace slopos::ui
