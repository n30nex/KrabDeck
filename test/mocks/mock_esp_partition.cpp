// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Mock ESP partition API for native testing.

#include "esp_partition.h"
#include <cstddef>

// ── Mock state ───────────────────────────────────────
static bool s_has_test_partition = false;

namespace sigurdos {
namespace test {
void mock_launcher_partition(bool present) {
    s_has_test_partition = present;
}
} // namespace test
} // namespace sigurdos

// ── ESP-IDF API stubs ─────────────────────────────────
esp_partition_iterator_t esp_partition_find(
    esp_partition_type_t type,
    esp_partition_subtype_t subtype,
    const char* label)
{
    // Only our specific query (APP + TEST) is mockable.
    // Everything else returns NULL (no match).
    if (type == ESP_PARTITION_TYPE_APP &&
        subtype == ESP_PARTITION_SUBTYPE_APP_TEST &&
        label == NULL) {
        return s_has_test_partition ? reinterpret_cast<esp_partition_iterator_t>(1) : NULL;
    }
    return NULL;
}

void esp_partition_iterator_release(esp_partition_iterator_t iterator)
{
    // No-op in mock.
    (void)iterator;
}

const esp_partition_t* esp_partition_get(esp_partition_iterator_t iterator)
{
    // Not used by our launcher_env implementation.
    (void)iterator;
    return NULL;
}
