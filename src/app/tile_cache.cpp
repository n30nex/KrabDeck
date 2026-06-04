// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SigurdOS.
//
// Tile cache — LRU eviction for offline map tiles.
// 4 entries default, uint64_t monotonic clock (safe from wrap).

#include "tile_cache.h"

void tile_cache_init(CachedTile* cache, int count) {
    if (!cache || count <= 0) return;

    for (int i = 0; i < count; i++) {
        cache[i].pixels = nullptr;
        cache[i].last_used = 0;
    }
}

CachedTile* tile_cache_lookup(CachedTile* cache, int count,
                               int zoom, int tx, int ty,
                               uint64_t* clock) {
    if (!cache || count <= 0 || !clock) return nullptr;

    for (int i = 0; i < count; i++) {
        if (cache[i].pixels &&
            cache[i].zoom == zoom &&
            cache[i].tx == tx &&
            cache[i].ty == ty) {
            cache[i].last_used = ++(*clock);
            return &cache[i];
        }
    }
    return nullptr;
}

CachedTile* tile_cache_evict_slot(CachedTile* cache, int count) {
    if (!cache || count <= 0) return nullptr;

    int lru = 0;
    for (int i = 0; i < count; i++) {
        if (!cache[i].pixels) return &cache[i];
        if (cache[i].last_used < cache[lru].last_used) lru = i;
    }
    return &cache[lru];
}
