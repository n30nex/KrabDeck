#pragma once

#include <cstdint>

namespace slopos::mesh {

// ── Message structure ────────────────────────────────────
struct MeshMessage {
    char sender[32];
    char text[256];
    uint32_t timestamp;
    bool is_self;
};

// ── Initialization ───────────────────────────────────────
bool init();
void loop();

// ── Outgoing messaging ───────────────────────────────────
bool send_direct(const char* dest_name, const char* message);
bool send_channel(uint8_t channel_hash[1], const char* message);

// ── Incoming message polling ─────────────────────────────
// Drains queued received messages into out[0..max-1].
// Returns the number of messages retrieved.
// Call from ui::loop() to feed chat.
int poll_messages(MeshMessage* out, int max);

// How many messages are waiting in the receive queue?
int pending_message_count();

// ── Node identity ────────────────────────────────────────
void set_own_name(const char* name);
const char* get_own_name();

// ── Radio stats ──────────────────────────────────────────
int get_noise_floor();
int get_last_rssi();
float get_last_snr();
int get_unread_count();
int get_recent_nodes(char names[][32], int max_count);

} // namespace slopos::mesh
