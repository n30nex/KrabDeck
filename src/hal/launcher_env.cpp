// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Launcher environment detection implementation.
//
// Uses the ESP-IDF partition API to probe for a `test`-subtype app
// partition — Launcher's resident slot. Standalone firmware built with
// the standard default_16MB.csv layout never has such a partition.

#include "launcher_env.h"
#include <esp_partition.h>

bool sigurdos_is_under_launcher()
{
    // Look for a test-subtype app partition — Launcher's resident slot.
    // Standard Arduino layouts (default_16MB.csv, etc.) never contain a
    // test-subtype app partition, so this cannot false-positive.
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_TEST,
        NULL);
    bool found = (it != NULL);
    if (it) {
        esp_partition_iterator_release(it);
    }
    return found;
}

const char* sigurdos_launcher_env_name()
{
    return sigurdos_is_under_launcher()
        ? "bmorcelli/Launcher"
        : "standalone";
}
