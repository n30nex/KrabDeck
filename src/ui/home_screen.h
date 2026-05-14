#pragma once

namespace slopos::ui {

void home_screen_create();
void home_screen_show();
void home_screen_update_battery(int pct);
void home_screen_update_time(const char* time_str);
void home_screen_update_signal(int rssi);

} // namespace slopos::ui
