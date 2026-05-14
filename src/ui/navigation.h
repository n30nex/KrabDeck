#pragma once

namespace slopos::ui {

// Screen identifiers for navigation
enum class Screen {
    Home,
    Chat,
    Contacts,
    Repeaters,
    Finder,
    Heard,
    Map,
    Advertise,
    Settings,
    Trace,
    Terminal,
    Noise,
    Signal,
    COUNT
};

// Navigate to a screen
void navigate_to(Screen screen);

// Go back to previous screen
void go_back();

} // namespace slopos::ui
