// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// MeshCore protocol integration for SlopOS-TDeck.
// Uses SlopMesh, a minimal mesh::Mesh subclass.

#pragma once
#include <cstdint>

namespace slopos {
namespace mesh {

struct MeshMessage {
    char sender[32];
    char text[256];
    uint32_t timestamp;
    bool is_self;
};

bool init();
void loop();

bool sendMessage(const char* dest_name, const char* text);
bool sendChannelMessage(const char* channel_name, const char* text);

int  pollMessages(MeshMessage* out, int max);
int  pendingMessageCount();

int  getContactCount();
int  exportContacts(char names[][32], int max);

int  getChannelCount();
int  exportChannels(char names[][32], int max);
bool addChannel(const char* name, const char* psk_base64);

void setOwnName(const char* name);
const char* getOwnName();

int   getNoiseFloor();
int   getLastRSSI();
float getLastSNR();

bool sendAdvert();
void saveState();

} // namespace mesh
} // namespace slopos
