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


// Mock implementations of mesh_wrapper functions for native testing
#include "mesh/mesh_wrapper.h"
#include <cstring>

namespace slopos {
namespace mesh {

static int mock_unread = 0;
static int mock_rssi   = 0;
static float mock_snr  = 0.0f;
static int mock_noise  = 0;
static char mock_own_name[32] = "TDeck+";

// Mock message queue (simple linked list)
static constexpr int MOCK_MAX_MSG = 32;
static MeshMessage mock_msgs[MOCK_MAX_MSG];
static int mock_msg_count = 0;

bool init() { return true; }
void loop() {}

bool send_direct(const char* dest_name, const char* message) {
    (void)dest_name; (void)message; return false;
}
bool send_channel(uint8_t channel_hash[1], const char* message) {
    (void)channel_hash; (void)message; return false;
}

int poll_messages(MeshMessage* out, int max) {
    int drained = 0;
    while (drained < max && mock_msg_count > 0) {
        out[drained] = mock_msgs[--mock_msg_count];
        drained++;
    }
    return drained;
}

int pending_message_count() { return mock_msg_count; }

void set_own_name(const char* name) {
    if (name) {
        strncpy(mock_own_name, name, sizeof(mock_own_name) - 1);
        mock_own_name[sizeof(mock_own_name) - 1] = '\0';
    }
}
const char* get_own_name() { return mock_own_name; }

int get_noise_floor()  { return mock_noise; }
int get_last_rssi()    { return mock_rssi; }
float get_last_snr()   { return mock_snr; }
int get_unread_count() { return mock_unread; }
int get_recent_nodes(char names[][32], int max_count) {
    (void)names; (void)max_count; return 0;
}

} // namespace mesh
} // namespace slopos
