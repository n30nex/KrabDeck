// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// MeshCore protocol integration using SlopMesh (minimal Mesh subclass).
// MeshCore is MIT licensed (meshcore-dev/MeshCore).

#include "mesh_wrapper.h"
#include "hal/tdeck_board.h"
#include "hal/tdeck_pins.h"
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
    strncpy(m.text, text, sizeof(m.text) - 1);
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
    f.write(buf, len);
    f.close();
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

bool sendChannelMessage(const char* ch, const char* text) {
    (void)ch; (void)text; return false;  // TODO: channels
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

// ── Channels ────────────────────────────────────

int getChannelCount() { return 0; }
int exportChannels(char names[][32], int max) { return 0; }
bool addChannel(const char* name, const char* psk) { return false; }

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
    if (g_mesh) { g_mesh->broadcastAdvert(own_name); return true; }
    return false;
}

void saveState() {
    if (g_mesh) saveIdentity(g_mesh->self_id);
}

} // namespace mesh
} // namespace slopos
