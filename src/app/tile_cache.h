#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Tile cache — LRU eviction for offline map tiles.
// Default 4 entries (~524 KB PSRAM at 256×256 RGB565 each).

#include <cstdint>

// A single tile cache entry
struct CachedTile {
    int      zoom;
    int      tx;
    int      ty;
    uint16_t* pixels;    // RGBA565 pixel data (or nullptr if empty)
    uint64_t  last_used; // monotonic clock stamp (set by cache_lookup)
};

// Default tile cache size
static constexpr int TILE_CACHE_SIZE = 4;

// ── Lifecycle ───────────────────────────────────────────────

// Zero-initialise all entries in the cache array (pixels = nullptr)
void tile_cache_init(CachedTile* cache, int count);

// ── Lookup ──────────────────────────────────────────────────

// Look up a tile by coordinates.
// On hit: updates last_used, increments *clock, returns the entry.
// On miss: returns nullptr.
CachedTile* tile_cache_lookup(CachedTile* cache, int count,
                              int zoom, int tx, int ty,
                              uint64_t* clock);

// ── Eviction ────────────────────────────────────────────────

// Find a slot for a new tile.
// Returns the first empty slot (pixels == nullptr), or the LRU entry
// (oldest last_used) if all slots are full.
// Does NOT free pixels — the caller must free the returned entry's
// pixels pointer before overwriting it.
CachedTile* tile_cache_evict_slot(CachedTile* cache, int count);
