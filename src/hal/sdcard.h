#pragma once

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

#include <cstdint>
#include <cstddef>

// Initialize SD card over SPI
// Returns true if card detected and mounted
bool slopos_sdcard_init();

// Check if SD card is currently mounted
bool slopos_sdcard_mounted();

// Filesystem info
uint64_t slopos_sdcard_capacity_bytes();
uint64_t slopos_sdcard_free_bytes();

// Format size string for display
const char* slopos_sdcard_format_size(uint64_t bytes, char* buf, size_t buf_sz);

// File operations (optional — for map tiles, logs)
bool slopos_sdcard_exists(const char* path);
size_t slopos_sdcard_read(const char* path, uint8_t* buf, size_t max_len);
bool slopos_sdcard_write(const char* path, const uint8_t* data, size_t len);
