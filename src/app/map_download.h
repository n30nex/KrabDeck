#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later

#include "map_download_policy.h"

#include <cstdint>

namespace sigurdos::app::map_download {

enum class Provider : uint8_t {
    NrCanWms = 0,
    GenericXyz,
};

enum class State : uint8_t {
    Idle = 0,
    Running,
    Paused,
    Completed,
    CompletedWithErrors,
    Cancelled,
    Failed,
};

struct Request {
    Provider provider;
    Bounds bounds;
    uint8_t min_zoom;
    uint8_t max_zoom;
    char name[32];
    char attribution[96];
    char url_template[MAX_URL_BYTES + 1];
    char ca_path[96];
};

struct Status {
    State state;
    uint32_t total;
    uint32_t completed;
    uint32_t skipped;
    uint32_t failed;
    Cursor current;
    uint64_t free_bytes;
    char detail[96];
};

// Restore an interrupted job from SD. A running job resumes automatically.
void init();

// Stop accepting work and wait until the background task has released HTTP,
// file, and SD-card resources. This is a terminal power-transition barrier;
// a timed-out caller must not remove peripheral power or restart the bus.
bool quiesce(uint32_t timeout_ms = 30000);

// Built-in NRCan Canada Base Map Transportation WMS.
bool startNrCan(const Bounds& bounds, uint8_t min_zoom, uint8_t max_zoom,
                const char* name = "NRCan offline map");

// Generic providers require an HTTPS XYZ template and a PEM trust anchor on
// the SD card. The caller is responsible for permission to cache that source.
bool startXyz(const Bounds& bounds, uint8_t min_zoom, uint8_t max_zoom,
              const char* name, const char* attribution,
              const char* url_template,
              const char* ca_path = "/tiles/provider-ca.pem");

bool pause();
bool resume();
bool cancel();
Status status();
const char* stateName(State state);

}  // namespace sigurdos::app::map_download
