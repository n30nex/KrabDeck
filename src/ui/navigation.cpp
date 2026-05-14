#include "navigation.h"
#include "home_screen.h"
#include "chat_screen.h"
#include "screens.h"
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
    case Screen::Home:       home_screen_show();       break;
    case Screen::Chat:       chat_screen_show();       break;
    case Screen::Contacts:   contacts_screen_show();   break;
    case Screen::Repeaters:  repeaters_screen_show();  break;
    case Screen::Finder:     finder_screen_show();     break;
    case Screen::Heard:      heard_screen_show();      break;
    case Screen::Map:        map_screen_show();        break;
    case Screen::Advertise:  advertise_screen_show();  break;
    case Screen::Settings:   settings_screen_show();   break;
    case Screen::Trace:      trace_screen_show();      break;
    case Screen::Terminal:   terminal_screen_show();   break;
    case Screen::Noise:      noise_screen_show();      break;
    case Screen::Signal:     signal_screen_show();     break;
    default: break;
    }
}

void go_back()
{
    navigate_to(previous);
}

} // namespace slopos::ui
