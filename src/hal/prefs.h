#pragma once

// Based on MeshCore's NodePrefs — MIT license (meshcore-dev/MeshCore)
// Adapted for SlopOS-TDeck

#include <cstdint>

namespace slopos {

struct NodePrefs {
    char    node_name[32];
    float   freq;
    float   bw;
    uint8_t sf;
    uint8_t cr;
    int8_t  tx_power_dbm;
    uint8_t gps_enabled;
    uint32_t gps_interval;

    // Default values
    void set_defaults() {
        strncpy(node_name, "SlopOS T-Deck", sizeof(node_name));
        freq = 869.618f;
        bw   = 62.5f;
        sf   = 8;
        cr   = 5;
        tx_power_dbm = 22;
        gps_enabled = 0;
        gps_interval = 60;
    }
};

} // namespace slopos
