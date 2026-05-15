// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Minimal MeshCore Mesh subclass for SlopOS-TDeck.
// Based on advice from Claude Code analysis of MeshCore internals.
// MeshCore is MIT licensed (meshcore-dev/MeshCore).

#pragma once
#include <Mesh.h>
#include <helpers/ArduinoHelpers.h>
#include <hal/prefs.h>

namespace slopos {
namespace mesh {

static constexpr int SLOP_MAX_CONTACTS = 64;

struct SlopContact {
    ::mesh::Identity id;
    uint8_t  secret[PUB_KEY_SIZE];
    char     name[32];
    uint32_t last_seen;    // RTC timestamp of last advert
};

class SlopMesh : public ::mesh::Mesh {
    SlopContact _contacts[SLOP_MAX_CONTACTS];
    int _nContacts = 0;

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
    void onPeerDataRecv(::mesh::Packet*, uint8_t type, int sender_idx,
                        const uint8_t* secret, uint8_t* data, size_t len) override
    {
        if (type != PAYLOAD_TYPE_TXT_MSG || _nMatches == 0 || !_onMessage) return;
        // Data layout: [4-byte LE timestamp][null-terminated text]
        const char* text = (len > 4) ? (const char*)(data + 4) : "";
        const char* sender = _contacts[_matchIdxs[sender_idx]].name;
        if (sender[0]) _onMessage(sender, text);
    }

    // ── Contact discovery ─────────────────────────────
    void onAdvertRecv(::mesh::Packet*, const ::mesh::Identity& id, uint32_t timestamp,
                      const uint8_t* app_data, size_t app_data_len) override
    {
        // Deduplicate
        for (int i = 0; i < _nContacts; i++)
            if (_contacts[i].id.matches(id)) {
                _contacts[i].last_seen = timestamp;
                return;
            }

        if (_nContacts >= SLOP_MAX_CONTACTS) return;

        SlopContact& c = _contacts[_nContacts++];
        c.id = id;
        c.last_seen = timestamp;
        self_id.calcSharedSecret(c.secret, id);

        // MeshCore advert format: [name_len(1)][name...]
        if (app_data_len > 1) {
            uint8_t nlen = app_data[0];
            if (nlen >= sizeof(c.name)) nlen = sizeof(c.name) - 1;
            memcpy(c.name, app_data + 1, nlen);
            c.name[nlen] = '\0';
        } else {
            snprintf(c.name, sizeof(c.name), "node_%02x", id.pub_key[0]);
        }
    }

    // ── Group text ────────────────────────────────────
    int searchChannelsByHash(const uint8_t* hash, ::mesh::GroupChannel out[], int max) override {
        return 0;  // channels not implemented yet
    }

    void onGroupDataRecv(::mesh::Packet*, uint8_t type, const ::mesh::GroupChannel&,
                         uint8_t* data, size_t len) override
    {
        if (type != PAYLOAD_TYPE_GRP_TXT || !_onMessage) return;
        const char* text = (len > 4) ? (const char*)(data + 4) : "";
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
            uint8_t buf[4 + 180];
            uint32_t ts = getRTCClock()->getCurrentTime();
            memcpy(buf, &ts, 4);
            strncpy((char*)(buf + 4), text, 175);
            size_t len = 4 + strlen(text) + 1;
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
        uint8_t app[33];
        uint8_t nlen = (uint8_t)strlen(name);
        if (nlen > 31) nlen = 31;
        app[0] = nlen;
        memcpy(app + 1, name, nlen);
        ::mesh::Packet* pkt = createAdvert(self_id, app, 1 + nlen);
        if (pkt) sendFlood(pkt);
    }

    // ── Contact export ────────────────────────────────
    int getContactCount() const { return _nContacts; }
    const SlopContact* getContact(int i) const {
        return (i >= 0 && i < _nContacts) ? &_contacts[i] : nullptr;
    }

    // ── Prefs ─────────────────────────────────────────
    slopos::NodePrefs& prefs() { return _prefs; }
};

} // namespace mesh
} // namespace slopos
