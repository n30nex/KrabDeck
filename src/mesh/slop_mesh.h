// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Minimal MeshCore Mesh subclass for SlopOS-TDeck.
// Based on advice from Claude Code analysis of MeshCore internals.
// MeshCore is MIT licensed (meshcore-dev/MeshCore).

#pragma once
#include <Mesh.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/AdvertDataHelpers.h>
#include <hal/prefs.h>

namespace slopos {
namespace mesh {

static constexpr int SLOP_MAX_CONTACTS  = 64;
static constexpr int SLOP_MAX_CHANNELS  = 8;

struct SlopContact {
    ::mesh::Identity id;
    uint8_t  secret[PUB_KEY_SIZE];
    char     name[32];
    uint32_t last_seen;    // RTC timestamp of last advert
    int      last_rssi;    // RSSI of last received packet (dBm)
};

// Mirrors MeshCore's ChannelDetails for group channel storage
struct SlopChannel {
    ::mesh::GroupChannel channel;
    char name[32];
};

class SlopMesh : public ::mesh::Mesh {
    SlopContact _contacts[SLOP_MAX_CONTACTS];
    int _nContacts = 0;

    SlopChannel _channels[SLOP_MAX_CHANNELS];
    int _nChannels = 0;

    // Match cache for searchPeersByHash/getPeerSharedSecret back-to-back calls
    int  _matchIdxs[SLOP_MAX_CONTACTS];
    int  _nMatches = 0;

    slopos::NodePrefs _prefs;
    void (*_onMessage)(const char* sender, const char* text);

protected:
    // ── Peer DB ──────────────────────────────────────
    int searchPeersByHash(const uint8_t* hash) override {
        _nMatches = 0;
        for (int i = 0; i < _nContacts; i++) {
            if (_contacts[i].id.isHashMatch(hash))
                _matchIdxs[_nMatches++] = i;
        }
        return _nMatches;
    }

    void getPeerSharedSecret(uint8_t* dest, int peer_idx) override {
        if (peer_idx >= 0 && peer_idx < _nMatches)
            memcpy(dest, _contacts[_matchIdxs[peer_idx]].secret, PUB_KEY_SIZE);
    }

    // ── Incoming text ────────────────────────────────
    void onPeerDataRecv(::mesh::Packet* pkt, uint8_t type, int sender_idx,
                        const uint8_t* secret, uint8_t* data, size_t len) override
    {
        if (type != PAYLOAD_TYPE_TXT_MSG || _nMatches == 0 || !_onMessage) return;
        // Data layout: [4-byte LE timestamp][null-terminated text]
        const char* text = (len > 4) ? (const char*)(data + 4) : "";
        int idx = _matchIdxs[sender_idx];
        const char* sender = _contacts[idx].name;
        if (sender[0]) {
            _onMessage(sender, text);
        }
    }

    // ── Contact discovery ─────────────────────────────
    void onAdvertRecv(::mesh::Packet*, const ::mesh::Identity& id, uint32_t timestamp,
                      const uint8_t* app_data, size_t app_data_len) override
    {
        // Parse advert using MeshCore's standard parser
        AdvertDataParser parser(app_data, (uint8_t)app_data_len);
        if (!parser.isValid()) return;

        const char* name = parser.getName();
        if (!name || !name[0]) {
            // No name — generate a fallback from pub_key
            static char fallback[16];
            snprintf(fallback, sizeof(fallback), "node_%02x", id.pub_key[0]);
            name = fallback;
        }

        // Deduplicate
        for (int i = 0; i < _nContacts; i++)
            if (_contacts[i].id.matches(id)) {
                _contacts[i].last_seen = timestamp;
                // Update name if changed (e.g. user renamed device)
                strncpy(_contacts[i].name, name, sizeof(_contacts[i].name) - 1);
                _contacts[i].name[sizeof(_contacts[i].name) - 1] = '\0';
                return;
            }

        if (_nContacts >= SLOP_MAX_CONTACTS) return;

        SlopContact& c = _contacts[_nContacts++];
        c.id = id;
        c.last_seen = timestamp;
        c.last_rssi = 0;
        self_id.calcSharedSecret(c.secret, id);

        strncpy(c.name, name, sizeof(c.name) - 1);
        c.name[sizeof(c.name) - 1] = '\0';
    }

    // ── Group text ────────────────────────────────────
    int searchChannelsByHash(const uint8_t* hash, ::mesh::GroupChannel out[], int max) override {
        int n = 0;
        for (int i = 0; i < _nChannels && n < max; i++) {
            if (_channels[i].channel.hash[0] == hash[0]) {   // first byte match
                out[n++] = _channels[i].channel;
            }
        }
        return n;
    }

    void onGroupDataRecv(::mesh::Packet*, uint8_t type, const ::mesh::GroupChannel&,
                         uint8_t* data, size_t len) override
    {
        if (type != PAYLOAD_TYPE_GRP_TXT || !_onMessage) return;
        // Group text payload: [4-byte LE timestamp][text_type byte][null-terminated text]
        const char* text = (len > 5) ? (const char*)(data + 5) : "";
        _onMessage("[group]", text);
    }

public:
    SlopMesh(::mesh::Radio& r, ::mesh::MillisecondClock& ms, ::mesh::RNG& rng,
             ::mesh::RTCClock& rtc, ::mesh::PacketManager& mgr, ::mesh::MeshTables& tbl)
        : ::mesh::Mesh(r, ms, rng, rtc, mgr, tbl),
          _onMessage(nullptr)
    {
        _prefs.set_defaults();
    }

    void setMessageCallback(void (*cb)(const char* sender, const char* text)) { _onMessage = cb; }

    // ── Send helpers ──────────────────────────────────
    bool sendTextTo(const char* dest_name, const char* text) {
        for (int i = 0; i < _nContacts; i++) {
            if (strcmp(_contacts[i].name, dest_name) != 0) continue;
            // Payload: 4-byte LE timestamp + null-terminated text, max 180 bytes total
            static constexpr size_t MAX_PAYLOAD = 180;
            uint8_t buf[4 + MAX_PAYLOAD];
            uint32_t ts = getRTCClock()->getCurrentTime();
            memcpy(buf, &ts, 4);
            size_t text_len = strnlen(text, MAX_PAYLOAD - 1);
            memcpy(buf + 4, text, text_len);
            buf[4 + text_len] = '\0';
            size_t len = 4 + text_len + 1;
            ::mesh::Packet* pkt = createDatagram(PAYLOAD_TYPE_TXT_MSG,
                                                  _contacts[i].id,
                                                  _contacts[i].secret,
                                                  buf, len);
            if (!pkt) return false;
            sendFlood(pkt);
            return true;
        }
        return false;
    }

    void broadcastAdvert(const char* name) {
        AdvertDataBuilder builder(ADV_TYPE_CHAT, name);
        uint8_t app[MAX_ADVERT_DATA_SIZE];
        uint8_t app_len = builder.encodeTo(app);
        ::mesh::Packet* pkt = createAdvert(self_id, app, app_len);
        if (pkt) sendFlood(pkt);
    }

    void broadcastAdvert(const char* name, double lat, double lon) {
        AdvertDataBuilder builder(ADV_TYPE_CHAT, name, lat, lon);
        uint8_t app[MAX_ADVERT_DATA_SIZE];
        uint8_t app_len = builder.encodeTo(app);
        ::mesh::Packet* pkt = createAdvert(self_id, app, app_len);
        if (pkt) sendFlood(pkt);
    }

    // ── Contact export ────────────────────────────────
    int getContactCount() const { return _nContacts; }
    const SlopContact* getContact(int i) const {
        return (i >= 0 && i < _nContacts) ? &_contacts[i] : nullptr;
    }

    // ── Channel management ────────────────────────────
    // Minimal base64 decode (avoids external dependency — MeshCore uses base64.hpp)
    static int decode_b64(const char* in, size_t in_len, uint8_t* out) {
        static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        int o = 0;
        uint32_t buf = 0;
        int bits = 0;
        for (size_t i = 0; i < in_len && in[i] != '='; i++) {
            const char* p = strchr(T, in[i]);
            if (!p) continue;
            buf = (buf << 6) | (uint32_t)(p - T);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out[o++] = (uint8_t)(buf >> bits);
                buf &= (1U << bits) - 1;
            }
        }
        return o;
    }

    bool addChannel(const char* name, const char* psk_base64) {
        if (_nChannels >= SLOP_MAX_CHANNELS) return false;

        SlopChannel& ch = _channels[_nChannels];
        memset(ch.channel.secret, 0, sizeof(ch.channel.secret));

        // Decode base64 PSK to secret (matches MeshCore addChannel protocol)
        int len = decode_b64(psk_base64, strlen(psk_base64), ch.channel.secret);
        if (len != 32 && len != 16) return false;  // must be 128-bit or 256-bit key

        // Hash the PSK to create the channel hash (matches MeshCore protocol)
        ::mesh::Utils::sha256(ch.channel.hash, sizeof(ch.channel.hash),
                              ch.channel.secret, len);
        strncpy(ch.name, name, sizeof(ch.name) - 1);
        ch.name[sizeof(ch.name) - 1] = '\0';
        _nChannels++;
        return true;
    }

    int getChannelCount() const { return _nChannels; }
    const SlopChannel* getChannel(int i) const {
        return (i >= 0 && i < _nChannels) ? &_channels[i] : nullptr;
    }

    bool sendGroupText(int channel_idx, const char* text) {
        if (channel_idx < 0 || channel_idx >= _nChannels) return false;
        if (!text || !text[0]) return false;

        static constexpr size_t MAX_GRP_PAYLOAD = 180;
        uint8_t buf[5 + MAX_GRP_PAYLOAD];
        uint32_t ts = getRTCClock()->getCurrentTime();
        memcpy(buf, &ts, 4);
        buf[4] = 0;   // text_type = 0 (plain text)
        size_t text_len = strnlen(text, MAX_GRP_PAYLOAD - 1);
        memcpy(buf + 5, text, text_len);
        size_t total = 5 + text_len;
        // ensure null termination
        if (total < sizeof(buf)) buf[total] = '\0';

        ::mesh::Packet* pkt = createGroupDatagram(PAYLOAD_TYPE_GRP_TXT,
                                                   _channels[channel_idx].channel,
                                                   buf, total);
        if (!pkt) return false;
        sendFlood(pkt);
        return true;
    }

    // ── Prefs ─────────────────────────────────────────
    slopos::NodePrefs& prefs() { return _prefs; }
};

} // namespace mesh
} // namespace slopos
