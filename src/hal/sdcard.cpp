#include "sdcard.h"
#include "tdeck_pins.h"
#include <Arduino.h>
#include <SD.h>
#include <cstdio>
#include <cstring>

static bool mounted = false;
static uint64_t capacity_bytes = 0;
static uint64_t free_bytes = 0;

bool slopos_sdcard_init()
{
    // SPI bus is shared with LoRa and display — already initialized
    // by mesh_wrapper during radio init. Just init the SD card.
    if (!SD.begin(PIN_SD_CS)) {
        mounted = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        mounted = false;
        return false;
    }

    capacity_bytes = (uint64_t)SD.cardSize();
    free_bytes = (uint64_t)(SD.totalBytes() - SD.usedBytes());

    mounted = true;
    return true;
}

bool slopos_sdcard_mounted()
{
    return mounted;
}

uint64_t slopos_sdcard_capacity_bytes()
{
    return capacity_bytes;
}

uint64_t slopos_sdcard_free_bytes()
{
    return free_bytes;
}

const char* slopos_sdcard_format_size(uint64_t bytes, char* buf, size_t buf_sz)
{
    if (!buf || buf_sz == 0) return "";

    if (bytes >= 1024ULL * 1024 * 1024) {
        snprintf(buf, buf_sz, "%.1f GB",
                 (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        snprintf(buf, buf_sz, "%.1f MB",
                 (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(buf, buf_sz, "%llu KB",
                 (unsigned long long)(bytes / 1024));
    } else {
        snprintf(buf, buf_sz, "%llu B", (unsigned long long)bytes);
    }
    return buf;
}

bool slopos_sdcard_exists(const char* path)
{
    if (!mounted || !path) return false;
    return SD.exists(path);
}

size_t slopos_sdcard_read(const char* path, uint8_t* buf, size_t max_len)
{
    if (!mounted || !path || !buf || max_len == 0) return 0;

    File f = SD.open(path, FILE_READ);
    if (!f) return 0;

    size_t read = f.read(buf, max_len);
    f.close();
    return read;
}

bool slopos_sdcard_write(const char* path, const uint8_t* data, size_t len)
{
    if (!mounted || !path || !data || len == 0) return false;

    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;

    size_t written = f.write(data, len);
    f.close();
    return written == len;
}
