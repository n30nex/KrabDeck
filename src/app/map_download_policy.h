#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace sigurdos::app::map_download {

static constexpr int MIN_ZOOM = 0;
static constexpr int MAX_ZOOM = 18;
static constexpr uint32_t MAX_TILES = 20000;
static constexpr size_t MAX_PNG_BYTES = 320U * 1024U;
static constexpr size_t MAX_URL_BYTES = 511;

struct Bounds {
    double min_lat;
    double min_lon;
    double max_lat;
    double max_lon;
};

struct TileRange {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
};

struct Cursor {
    int zoom;
    int x;
    int y;
};

inline bool boundsValid(const Bounds& bounds)
{
    return std::isfinite(bounds.min_lat) && std::isfinite(bounds.min_lon) &&
        std::isfinite(bounds.max_lat) && std::isfinite(bounds.max_lon) &&
        bounds.min_lat >= -85.05112878 && bounds.max_lat <= 85.05112878 &&
        bounds.min_lon >= -180.0 && bounds.max_lon <= 180.0 &&
        bounds.min_lat < bounds.max_lat && bounds.min_lon < bounds.max_lon;
}

inline int lonToTileX(double lon, int zoom)
{
    const int axis = 1 << zoom;
    int value = static_cast<int>(((lon + 180.0) / 360.0) * axis);
    if (value < 0) value = 0;
    if (value >= axis) value = axis - 1;
    return value;
}

inline int latToTileY(double lat, int zoom)
{
    const int axis = 1 << zoom;
    const double radians = lat * 3.14159265358979323846 / 180.0;
    int value = static_cast<int>(
        (1.0 - std::log(std::tan(radians) + 1.0 / std::cos(radians)) /
                   3.14159265358979323846) /
        2.0 * axis);
    if (value < 0) value = 0;
    if (value >= axis) value = axis - 1;
    return value;
}

inline TileRange tileRange(const Bounds& bounds, int zoom)
{
    return {
        lonToTileX(bounds.min_lon, zoom),
        lonToTileX(bounds.max_lon, zoom),
        latToTileY(bounds.max_lat, zoom),
        latToTileY(bounds.min_lat, zoom),
    };
}

inline uint64_t tileCount(const Bounds& bounds, int min_zoom, int max_zoom)
{
    if (!boundsValid(bounds) || min_zoom < MIN_ZOOM || max_zoom > MAX_ZOOM ||
        min_zoom > max_zoom) {
        return 0;
    }
    uint64_t total = 0;
    for (int zoom = min_zoom; zoom <= max_zoom; ++zoom) {
        const TileRange range = tileRange(bounds, zoom);
        total += static_cast<uint64_t>(range.max_x - range.min_x + 1) *
            static_cast<uint64_t>(range.max_y - range.min_y + 1);
        if (total > MAX_TILES) return total;
    }
    return total;
}

inline Cursor firstCursor(const Bounds& bounds, int min_zoom)
{
    const TileRange range = tileRange(bounds, min_zoom);
    return {min_zoom, range.min_x, range.min_y};
}

inline bool cursorInRequest(const Cursor& cursor, const Bounds& bounds,
                            int min_zoom, int max_zoom)
{
    if (!boundsValid(bounds) || cursor.zoom < min_zoom ||
        cursor.zoom > max_zoom) {
        return false;
    }
    const TileRange range = tileRange(bounds, cursor.zoom);
    return cursor.x >= range.min_x && cursor.x <= range.max_x &&
        cursor.y >= range.min_y && cursor.y <= range.max_y;
}

inline bool progressValid(uint64_t expected, uint32_t total,
                          uint32_t completed, uint32_t skipped,
                          uint32_t failed)
{
    const uint64_t processed = static_cast<uint64_t>(completed) + skipped + failed;
    return expected > 0 && expected <= UINT32_MAX && total == expected &&
        processed <= total;
}

enum class DurableCommitResult : uint8_t {
    Installed,
    StaleGeneration,
    PersistFailed,
};

enum class PngStreamDisposition : uint8_t {
    Validate,
    Retry,
    PermanentFailure,
    Cancelled,
};

inline PngStreamDisposition classifyPngStream(
    int written, int expected_size, bool overflowed, bool generation_running)
{
    if (!generation_running) return PngStreamDisposition::Cancelled;
    if (overflowed) return PngStreamDisposition::PermanentFailure;
    if (written < 24 ||
        (expected_size >= 0 && written != expected_size)) {
        return PngStreamDisposition::Retry;
    }
    return PngStreamDisposition::Validate;
}

inline uint32_t nextGeneration(uint32_t current)
{
    return current == UINT32_MAX ? 1U : current + 1U;
}

// Callers serialize this operation around their generation snapshot. The
// durable copy is written before the non-failing in-memory install, so a
// failed write leaves the previously installed job untouched. A stale worker
// never calls either callback and therefore cannot overwrite a newer job on
// disk.
template <typename PersistFn, typename InstallFn>
inline DurableCommitResult durableCommitIfCurrent(
    uint32_t expected_generation, uint32_t installed_generation,
    PersistFn persist, InstallFn install)
{
    if (expected_generation != installed_generation) {
        return DurableCommitResult::StaleGeneration;
    }
    if (!persist()) return DurableCommitResult::PersistFailed;
    install();
    return DurableCommitResult::Installed;
}

inline bool advanceCursor(Cursor* cursor, const Bounds& bounds, int max_zoom)
{
    if (!cursor || cursor->zoom > max_zoom) return false;
    TileRange range = tileRange(bounds, cursor->zoom);
    if (cursor->y < range.max_y) {
        ++cursor->y;
        return true;
    }
    if (cursor->x < range.max_x) {
        ++cursor->x;
        cursor->y = range.min_y;
        return true;
    }
    if (cursor->zoom >= max_zoom) return false;
    ++cursor->zoom;
    range = tileRange(bounds, cursor->zoom);
    cursor->x = range.min_x;
    cursor->y = range.min_y;
    return true;
}

inline bool privateIpv4Host(const char* host, size_t length)
{
    unsigned a = 0, b = 0, c = 0, d = 0;
    char tail = '\0';
    char copy[64];
    if (!host || length == 0 || length >= sizeof(copy)) return false;
    std::memcpy(copy, host, length);
    copy[length] = '\0';
    if (std::sscanf(copy, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 ||
        a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }
    return a == 0 || a == 10 || a == 127 || (a == 169 && b == 254) ||
        (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) ||
        a >= 224;
}

inline bool httpsUrlValid(const char* url)
{
    if (!url) return false;
    const size_t length = std::strlen(url);
    if (length < 12 || length > MAX_URL_BYTES ||
        std::strncmp(url, "https://", 8) != 0 || std::strchr(url, '#')) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(url[i]);
        if (c <= 0x20 || c == 0x7f || c == '\\') return false;
    }
    const char* authority = url + 8;
    const char* end = authority;
    while (*end && *end != '/' && *end != '?' && *end != '#') ++end;
    if (end == authority || std::memchr(authority, '@', end - authority)) return false;

    const char* host_end = end;
    const char* colon = static_cast<const char*>(
        std::memchr(authority, ':', static_cast<size_t>(end - authority)));
    if (colon) host_end = colon;
    const size_t host_length = static_cast<size_t>(host_end - authority);
    if (host_length == 0 || authority[0] == '[') return false;
    if ((host_length == 9 && std::strncmp(authority, "localhost", 9) == 0) ||
        (host_length >= 6 &&
         std::strncmp(host_end - 6, ".local", 6) == 0) ||
        privateIpv4Host(authority, host_length)) {
        return false;
    }
    return true;
}

inline bool xyzTemplateValid(const char* value)
{
    if (!value || !std::strstr(value, "{z}") || !std::strstr(value, "{x}") ||
        !std::strstr(value, "{y}")) {
        return false;
    }
    char expanded[MAX_URL_BYTES + 1];
    size_t written = 0;
    for (const char* cursor = value; *cursor; ) {
        if ((std::strncmp(cursor, "{z}", 3) == 0) ||
            (std::strncmp(cursor, "{x}", 3) == 0) ||
            (std::strncmp(cursor, "{y}", 3) == 0)) {
            if (written >= MAX_URL_BYTES) return false;
            expanded[written++] = '0';
            cursor += 3;
        } else {
            if (*cursor == '{' || *cursor == '}' || written >= MAX_URL_BYTES) return false;
            expanded[written++] = *cursor++;
        }
    }
    expanded[written] = '\0';
    return httpsUrlValid(expanded);
}

inline bool pngHeaderValid(const uint8_t* header, size_t header_length,
                           size_t file_size)
{
    static constexpr uint8_t SIGNATURE[8] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (!header || header_length < 24 || file_size < 24 ||
        file_size > MAX_PNG_BYTES || std::memcmp(header, SIGNATURE, 8) != 0 ||
        std::memcmp(header + 12, "IHDR", 4) != 0) {
        return false;
    }
    const uint32_t width = (static_cast<uint32_t>(header[16]) << 24) |
        (static_cast<uint32_t>(header[17]) << 16) |
        (static_cast<uint32_t>(header[18]) << 8) | header[19];
    const uint32_t height = (static_cast<uint32_t>(header[20]) << 24) |
        (static_cast<uint32_t>(header[21]) << 16) |
        (static_cast<uint32_t>(header[22]) << 8) | header[23];
    return width == 256 && height == 256;
}

inline uint32_t pngCrc32Update(uint32_t crc, const uint8_t* data, size_t length)
{
    if (!data && length != 0) return 0;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U &
                static_cast<uint32_t>(0U - (crc & 1U)));
        }
    }
    return crc;
}

inline uint32_t pngReadBe32(const uint8_t* value)
{
    return (static_cast<uint32_t>(value[0]) << 24) |
        (static_cast<uint32_t>(value[1]) << 16) |
        (static_cast<uint32_t>(value[2]) << 8) | value[3];
}

// Validate the complete PNG chunk stream without loading a tile into RAM.
// read_at(offset, output, length) must return true only when every requested
// byte was read. CRC validation makes a power-loss-truncated .part file
// ineligible for promotion even when its signature and IHDR were persisted.
template <typename ReadAt>
inline bool pngCompleteValid(size_t file_size, ReadAt read_at)
{
    static constexpr uint8_t SIGNATURE[8] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (file_size < 57 || file_size > MAX_PNG_BYTES) return false;

    uint8_t signature[sizeof(SIGNATURE)];
    if (!read_at(0, signature, sizeof(signature)) ||
        std::memcmp(signature, SIGNATURE, sizeof(SIGNATURE)) != 0) {
        return false;
    }

    bool first_chunk = true;
    bool saw_idat = false;
    size_t offset = sizeof(SIGNATURE);
    while (offset <= file_size && file_size - offset >= 12) {
        uint8_t header[8];
        if (!read_at(offset, header, sizeof(header))) return false;
        const uint32_t chunk_length = pngReadBe32(header);
        const size_t remaining = file_size - offset;
        if (chunk_length > MAX_PNG_BYTES ||
            static_cast<size_t>(chunk_length) > remaining - 12) {
            return false;
        }

        const uint8_t* type = header + 4;
        const bool is_ihdr = std::memcmp(type, "IHDR", 4) == 0;
        const bool is_idat = std::memcmp(type, "IDAT", 4) == 0;
        const bool is_iend = std::memcmp(type, "IEND", 4) == 0;
        if (first_chunk) {
            if (!is_ihdr || chunk_length != 13) return false;
        } else if (is_ihdr) {
            return false;
        }

        const size_t data_offset = offset + sizeof(header);
        if (is_ihdr) {
            uint8_t dimensions[8];
            if (!read_at(data_offset, dimensions, sizeof(dimensions)) ||
                pngReadBe32(dimensions) != 256 ||
                pngReadBe32(dimensions + 4) != 256) {
                return false;
            }
        }

        uint32_t crc = pngCrc32Update(0xffffffffU, type, 4);
        uint8_t buffer[128];
        size_t consumed = 0;
        while (consumed < chunk_length) {
            const size_t amount = std::min(
                sizeof(buffer), static_cast<size_t>(chunk_length) - consumed);
            if (!read_at(data_offset + consumed, buffer, amount)) return false;
            crc = pngCrc32Update(crc, buffer, amount);
            consumed += amount;
        }
        crc ^= 0xffffffffU;

        uint8_t expected_crc[4];
        const size_t crc_offset = data_offset + chunk_length;
        if (!read_at(crc_offset, expected_crc, sizeof(expected_crc)) ||
            pngReadBe32(expected_crc) != crc) {
            return false;
        }

        const size_t next_offset = crc_offset + sizeof(expected_crc);
        if (is_iend) {
            return chunk_length == 0 && saw_idat && next_offset == file_size;
        }
        if (is_idat) saw_idat = true;
        first_chunk = false;
        offset = next_offset;
    }
    return false;
}

}  // namespace sigurdos::app::map_download
