#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SlopOS-TDeck.
//
// SlopOS-TDeck is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SlopOS-TDeck is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SlopOS-TDeck.  If not, see <https://www.gnu.org/licenses/>.


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
