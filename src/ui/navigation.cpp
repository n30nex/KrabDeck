#include "navigation.h"
#include "home_screen.h"
#include "chat_screen.h"
#include "screens.h"
#include <lvgl.h>

namespace slopos::ui {

static Screen current = Screen::Home;

// ── Back history stack (circular, max 8 entries) ─────────
static constexpr int MAX_HISTORY = 8;
static Screen history[MAX_HISTORY];
static int   history_top = -1;  // index of top (empty stack before any nav)

static void push_history(Screen s) {
    history_top = (history_top + 1) % MAX_HISTORY;
    history[history_top] = s;
}

static Screen pop_history() {
    if (history_top < 0) return Screen::Home;
    Screen s = history[history_top];
    history_top--;
    return s;
}

static bool history_empty() {
    return history_top < 0;
}

static void clear_history() {
    history_top = -1;
}

void navigate_to(Screen screen)
{
    if (screen == current) return;

    // Push current screen onto history before navigating away
    push_history(current);
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
    if (history_empty()) return; // nowhere to go back to

    Screen target = pop_history();
    // Navigate directly without pushing current (we're going back, not forward)
    current = target;

    switch (target) {
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

} // namespace slopos::ui
