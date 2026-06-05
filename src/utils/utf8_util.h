#pragma once

#include <cstddef>
#include <cstring>

namespace sigurdos {

static inline bool utf8_is_continuation(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

static inline size_t utf8_sequence_length(unsigned char lead)
{
    if ((lead & 0x80) == 0) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

// Safely truncate a UTF-8 string to at most max_bytes without splitting a
// multi-byte character. Returns the number of bytes to keep.
//
// Only bytes inside [0, max_bytes) are inspected, so bounded buffers do not
// need to be null-terminated one byte past max_bytes.
static inline size_t utf8_truncate_bytes(const char* str, size_t max_bytes)
{
    if (!str || max_bytes == 0) return 0;

    size_t len = strnlen(str, max_bytes);
    if (len < max_bytes) return len;

    size_t lead = len - 1;
    while (lead > 0 && utf8_is_continuation((unsigned char)str[lead])) {
        lead--;
    }

    const size_t expected = utf8_sequence_length((unsigned char)str[lead]);
    if (expected == 1) return lead + 1;

    const size_t available = len - lead;
    if (available < expected) return lead;

    return len;
}

} // namespace sigurdos
