#pragma once
#include <cstdint>

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
