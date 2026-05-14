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


#include "mesh_wrapper.h"
#include "hal/tdeck_board.h"
#include "hal/tdeck_pins.h"

// MeshCore core headers (from lib/meshcore)
#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/ESP32Board.h>
#include <helpers/ArduinoHelpers.h>

using slopos::mesh::MeshMessage;

// ── Global objects ──────────────────────────────────────
static slopos::TDeckBoard    board;
static SPIClass              lora_spi;
static Module*               lora_mod = new Module(P_LORA_NSS, P_LORA_DIO_1,
                                                   P_LORA_RESET, P_LORA_BUSY, lora_spi);
static CustomSX1262          radio_module(lora_mod);
static CustomSX1262Wrapper   radio_driver(radio_module, board);
static ESP32RTCClock         fallback_clock;
static AutoDiscoverRTCClock  rtc_clock(fallback_clock);
static StdRNG                fast_rng;
static SimpleMeshTables      tables;

static bool initialized = false;
static int  unread_count = 0;

// ── Message queue ───────────────────────────────────────
static constexpr int MAX_QUEUED = 32;
static MeshMessage    msg_buf[MAX_QUEUED];
static int            msg_head = 0;
static int            msg_tail = 0;
static int            msg_count = 0;

static char own_name[32] = "TDeck+";

static bool queue_push(const MeshMessage& msg) {
    if (msg_count >= MAX_QUEUED) return false;
    msg_buf[msg_head] = msg;
    msg_head = (msg_head + 1) % MAX_QUEUED;
    msg_count++;
    return true;
}

static bool queue_pop(MeshMessage* out) {
    if (msg_count == 0) return false;
    *out = msg_buf[msg_tail];
    msg_tail = (msg_tail + 1) % MAX_QUEUED;
    msg_count--;
    return true;
}

namespace slopos {
namespace mesh {

bool init()
{
    fallback_clock.begin();
    rtc_clock.begin(Wire);

    if (!radio_module.std_init(&lora_spi)) {
        return false;
    }

    fast_rng.begin(radio_module.random(0x7FFFFFFF));
    initialized = true;

    // Reset message queue
    msg_head = 0;
    msg_tail = 0;
    msg_count = 0;

    return true;
}

void loop()
{
    if (!initialized) return;
    radio_driver.loop();
    rtc_clock.tick();

    // TODO: Check for incoming packets, parse MeshCore PathPacket,
    // decrypt, extract text, and queue via queue_push().
    // For now, the queue is populated by on-hardware test injections
    // or by the companion radio bridge.
}

// ── Outgoing ────────────────────────────────────────────
bool send_direct(const char* dest_name, const char* message)
{
    // TODO: Full MeshCore packet construction + transmission
    // 1. Look up dest_name in mesh tables to get node ID
    // 2. Construct PathPacket with DESTINATION flag
    // 3. Encrypt with shared key
    // 4. Transmit via radio_driver.send()
    (void)dest_name;
    (void)message;
    return false;
}

bool send_channel(uint8_t channel_hash[1], const char* message)
{
    // TODO: Channel-based flood send
    (void)channel_hash;
    (void)message;
    return false;
}

// ── Incoming ────────────────────────────────────────────
int poll_messages(MeshMessage* out, int max)
{
    int drained = 0;
    while (drained < max && queue_pop(&out[drained])) {
        drained++;
    }
    return drained;
}

int pending_message_count()
{
    return msg_count;
}

void set_own_name(const char* name)
{
    if (!name) return;
    strncpy(own_name, name, sizeof(own_name) - 1);
    own_name[sizeof(own_name) - 1] = '\0';
}

const char* get_own_name()
{
    return own_name;
}

// ── Radio stats (unchanged) ─────────────────────────────
int get_noise_floor()    { return radio_driver.getNoiseFloor(); }
int get_last_rssi()      { return (int)radio_driver.getLastRSSI(); }
float get_last_snr()     { return radio_driver.getLastSNR(); }
int get_unread_count()   { return unread_count; }

int get_recent_nodes(char names[][32], int max_count)
{
    (void)names;
    (void)max_count;
    return 0;
}

} // namespace mesh
} // namespace slopos
