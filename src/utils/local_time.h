#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>

namespace sigurdos::local_time {

inline bool fromEpoch(uint32_t epoch, std::tm* out)
{
    if (epoch == 0 || !out) return false;
    const std::time_t value = static_cast<std::time_t>(epoch);
#if defined(_WIN32)
    return localtime_s(out, &value) == 0;
#else
    return localtime_r(&value, out) != nullptr;
#endif
}

inline bool formatHm(char* out, size_t capacity, uint32_t epoch)
{
    if (!out || capacity == 0) return false;
    std::tm local{};
    if (!fromEpoch(epoch, &local)) {
        const int written = std::snprintf(out, capacity, "--:--");
        return written >= 0 && static_cast<size_t>(written) < capacity;
    }
    const int written = std::snprintf(
        out, capacity, "%02d:%02d", local.tm_hour, local.tm_min);
    return written >= 0 && static_cast<size_t>(written) < capacity;
}

inline bool formatHms(char* out, size_t capacity, uint32_t epoch)
{
    if (!out || capacity == 0) return false;
    std::tm local{};
    if (!fromEpoch(epoch, &local)) {
        const int written = std::snprintf(out, capacity, "--:--:--");
        return written >= 0 && static_cast<size_t>(written) < capacity;
    }
    const int written = std::snprintf(
        out, capacity, "%02d:%02d:%02d",
        local.tm_hour, local.tm_min, local.tm_sec);
    return written >= 0 && static_cast<size_t>(written) < capacity;
}

inline bool toEpochChecked(int year, int month, int day,
                           int hour, int minute, int second,
                           uint32_t* out)
{
    if (!out || year < 1970 || month < 1 || month > 12 || day < 1 ||
        day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 59) {
        return false;
    }

    std::tm local{};
    local.tm_year = year - 1900;
    local.tm_mon = month - 1;
    local.tm_mday = day;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_sec = second;
    local.tm_isdst = -1;
    const std::time_t epoch = std::mktime(&local);
    if (epoch < 0 || static_cast<uint64_t>(epoch) > UINT32_MAX) return false;

    std::tm round_trip{};
#if defined(_WIN32)
    if (localtime_s(&round_trip, &epoch) != 0) return false;
#else
    if (localtime_r(&epoch, &round_trip) == nullptr) return false;
#endif
    if (round_trip.tm_year != year - 1900 || round_trip.tm_mon != month - 1 ||
        round_trip.tm_mday != day || round_trip.tm_hour != hour ||
        round_trip.tm_min != minute || round_trip.tm_sec != second) {
        return false;
    }

    *out = static_cast<uint32_t>(epoch);
    return true;
}

}  // namespace sigurdos::local_time
