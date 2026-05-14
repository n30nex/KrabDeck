#pragma once

#include <cstdint>

namespace slopos::mesh {

// Initialize the MeshCore networking stack
// Must be called after board.begin() and display init
bool init();

// Main loop tick — call every iteration
void loop();

// Send a text message to a specific node
bool send_direct(const char* dest_name, const char* message);

// Send a message to a group/channel
bool send_channel(uint8_t channel_hash[1], const char* message);

// Get signal stats
int get_noise_floor();
int get_last_rssi();
float get_last_snr();

// Number of unread messages (for badge)
int get_unread_count();

// Recently heard nodes
int get_recent_nodes(char names[][32], int max_count);

} // namespace slopos::mesh
