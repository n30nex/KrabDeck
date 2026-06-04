#pragma once

#include <cstdlib>

#define MALLOC_CAP_SPIRAM   0x01
#define MALLOC_CAP_INTERNAL 0x02
#define MALLOC_CAP_8BIT     0x04

inline void* heap_caps_malloc(size_t size, int)
{
    return std::malloc(size);
}

inline void heap_caps_free(void* ptr)
{
    std::free(ptr);
}
