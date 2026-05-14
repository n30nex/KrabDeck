#pragma once

namespace slopos::ui {

// Create and show the chat screen
void chat_screen_show();

// Add a message to the chat display
void chat_screen_add_msg(const char* sender, const char* text, bool is_self);

} // namespace slopos::ui
