// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// MeshCore protocol integration using SlopMesh (minimal Mesh subclass).
// MeshCore is MIT licensed (meshcore-dev/MeshCore).

#include "mesh_wrapper.h"
#include "hal/tdeck_board.h"
#include "hal/tdeck_pins.h"
#include "hal/gps.h"
#include "slop_mesh.h"

#include <SPIFFS.h>
#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/ESP32Board.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/StaticPoolPacketManager.h>

using slopos::mesh::MeshMessage;

// ════════════════════════════════════════════════════
// Global objects
// ════════════════════════════════════════════════════

static slopos::TDeckBoard        board;
static SPIClass                  lora_spi;
static Module*                   lora_mod = new Module(P_LORA_NSS, P_LORA_DIO_1,
                                                       P_LORA_RESET, P_LORA_BUSY, lora_spi);
static CustomSX1262              radio_module(lora_mod);
static CustomSX1262Wrapper       radio_driver(radio_module, board);
static ESP32RTCClock             fallback_clock;
static AutoDiscoverRTCClock      rtc_clock(fallback_clock);
static StdRNG                    fast_rng;
static SimpleMeshTables          tables;
static ArduinoMillis             millis_clock;
static StaticPoolPacketManager   pkt_mgr(16);
static slopos::mesh::SlopMesh*   g_mesh = nullptr;

static bool initialized = false;
static char own_name[32] = "SlopOS";

// ════════════════════════════════════════════════════
// Message queue
// ════════════════════════════════════════════════════

static constexpr int MAX_QUEUED = 64;
static MeshMessage   msg_buf[MAX_QUEUED];
static int           msg_head = 0, msg_tail = 0, msg_count = 0;

static void queue_push(const char* sender, const char* text) {
    if (msg_count >= MAX_QUEUED) return;
    MeshMessage& m = msg_buf[msg_head];
    strncpy(m.sender, sender, sizeof(m.sender) - 1);
    m.sender[sizeof(m.sender) - 1] = '\0';
    strncpy(m.text, text, sizeof(m.text) - 1);
    m.text[sizeof(m.text) - 1] = '\0';
    m.timestamp = rtc_clock.getCurrentTime();
    m.is_self = false;
    msg_head = (msg_head + 1) % MAX_QUEUED;
    msg_count++;
}

static bool queue_pop(MeshMessage* out) {
    if (msg_count == 0) return false;
    *out = msg_buf[msg_tail];
    msg_tail = (msg_tail + 1) % MAX_QUEUED;
    msg_count--;
    return true;
}

static void onMeshMessage(const char* sender, const char* text) {
    queue_push(sender, text);
    Serial.printf("[mesh] MSG from %s: %s\n", sender, text);
}

// ════════════════════════════════════════════════════
// Identity persistence
// ════════════════════════════════════════════════════

static bool loadIdentity(::mesh::LocalIdentity& id) {
    if (!SPIFFS.exists("/mesh_id")) return false;
    File f = SPIFFS.open("/mesh_id", "r");
    if (!f) return false;
    uint8_t buf[128];
    int len = f.read(buf, sizeof(buf));
    f.close();
    if (len < 64) return false;
    id.readFrom(buf, len);
    return ::mesh::LocalIdentity::validatePrivateKey(buf);
}

static void saveIdentity(::mesh::LocalIdentity& id) {
    uint8_t buf[128];
    size_t len = id.writeTo(buf, sizeof(buf));
    File f = SPIFFS.open("/mesh_id", "w");
    if (!f) return;
    size_t written = f.write(buf, len);
    f.close();
    if (written != len) {
        // Partial write — file may be corrupted, remove it
        SPIFFS.remove("/mesh_id");
    }
}

// ════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════

namespace slopos {
namespace mesh {

bool init()
{
    fallback_clock.begin();
    rtc_clock.begin(Wire);

    lora_spi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
    if (!radio_module.std_init(&lora_spi)) {
        Serial.println("[mesh] ERROR: Radio init failed");
        return false;
    }

    fast_rng.begin(radio_module.random(0x7FFFFFFF));

    g_mesh = new SlopMesh(radio_driver, millis_clock, fast_rng, rtc_clock, pkt_mgr, tables);
    if (!g_mesh) {
        Serial.println("[mesh] ERROR: SlopMesh allocation failed");
        return false;
    }
    g_mesh->setMessageCallback(onMeshMessage);

    // Generate or load identity
    if (!loadIdentity(g_mesh->self_id)) {
        g_mesh->self_id = ::mesh::LocalIdentity(&fast_rng);
        saveIdentity(g_mesh->self_id);
    }

    g_mesh->begin();
    g_mesh->broadcastAdvert(own_name);

    initialized = true;
    Serial.println("[mesh] SlopMesh initialized");
    return true;
}

void loop()
{
    if (!initialized || !g_mesh) return;
    g_mesh->loop();  // handles radio_driver.loop() internally
    rtc_clock.tick();
}

// ── Send ────────────────────────────────────────

bool sendMessage(const char* dest, const char* text) {
    return g_mesh ? g_mesh->sendTextTo(dest, text) : false;
}

bool sendChannelMessage(const char* channel_name, const char* text) {
    if (!g_mesh) return false;
    // Find channel by name
    for (int i = 0; i < g_mesh->getChannelCount(); i++) {
        auto* ch = g_mesh->getChannel(i);
        if (ch && strcmp(ch->name, channel_name) == 0) {
            return g_mesh->sendGroupText(i, text);
        }
    }
    return false;
}

// ── Message queue ───────────────────────────────

int pollMessages(MeshMessage* out, int max) {
    int n = 0;
    while (n < max && queue_pop(&out[n])) n++;
    return n;
}

int pendingMessageCount() { return msg_count; }

// ── Contacts ────────────────────────────────────

int getContactCount() { return g_mesh ? g_mesh->getContactCount() : 0; }

int exportContacts(char names[][32], int max) {
    if (!g_mesh) return 0;
    int n = 0;
    for (int i = 0; i < g_mesh->getContactCount() && n < max; i++) {
        auto* c = g_mesh->getContact(i);
        if (c) { strncpy(names[n], c->name, 31); names[n][31] = '\0'; n++; }
    }
    return n;
}

// ContactInfo is declared in mesh_wrapper.h — exportContactsFull uses it

int exportContactsFull(ContactInfo* out, int max) {
    if (!g_mesh) return 0;
    int n = 0;
    for (int i = 0; i < g_mesh->getContactCount() && n < max; i++) {
        auto* c = g_mesh->getContact(i);
        if (c && c->name[0]) {
            strncpy(out[n].name, c->name, 31);
            out[n].name[31] = '\0';
            out[n].rssi = c->last_rssi;
            out[n].last_seen = c->last_seen;
            n++;
        }
    }
    return n;
}

// ── Channels ────────────────────────────────────

int getChannelCount() { return g_mesh ? g_mesh->getChannelCount() : 0; }

int exportChannels(char names[][32], int max) {
    if (!g_mesh) return 0;
    int n = 0;
    for (int i = 0; i < g_mesh->getChannelCount() && n < max; i++) {
        auto* ch = g_mesh->getChannel(i);
        if (ch) { strncpy(names[n], ch->name, 31); names[n][31] = '\0'; n++; }
    }
    return n;
}

bool addChannel(const char* name, const char* psk) {
    return g_mesh ? g_mesh->addChannel(name, psk) : false;
}

// ── Identity ────────────────────────────────────

void setOwnName(const char* name) {
    if (!name) return;
    strncpy(own_name, name, sizeof(own_name) - 1);
    own_name[sizeof(own_name) - 1] = '\0';
}

const char* getOwnName() { return own_name; }

// ── Radio stats ─────────────────────────────────

int getNoiseFloor()   { return radio_driver.getNoiseFloor(); }
int getLastRSSI()     { return (int)radio_driver.getLastRSSI(); }
float getLastSNR()    { return radio_driver.getLastSNR(); }

bool sendAdvert() {
    if (!g_mesh) return false;
    if (slopos_gps_has_fix()) {
        g_mesh->broadcastAdvert(own_name,
            slopos_gps_latitude(), slopos_gps_longitude());
    } else {
        g_mesh->broadcastAdvert(own_name);
    }
    return true;
}

void saveState() {
    if (g_mesh) saveIdentity(g_mesh->self_id);
}

} // namespace mesh
} // namespace slopos
