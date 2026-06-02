// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// GitHub OTA firmware update — connects to WiFi in STA mode, downloads
// the latest firmware.bin from GitHub releases, and flashes via the
// Arduino Update class (dual OTA partition table required).

#pragma once
#include <cstdint>

namespace sigurdos {
namespace github_ota {

// ── GitHub OTA state ────────────────────────────────────────────
// Shared between the OTA task and the UI for progress display.

enum class GitHubOTAState {
    Idle,
    Connecting,     // Connecting to WiFi
    Downloading,    // Downloading firmware from GitHub
    Writing,        // Writing to flash (Update class)
    Success,        // Done — rebooting
    Failed,         // Error occurred
};

struct GitHubOTAStatus {
    GitHubOTAState state = GitHubOTAState::Idle;
    int  progress_pct = 0;       // 0-100 (download + write combined)
    char status_msg[80] = "";   // Human-readable status
    char error_msg[128] = "";   // Last error (when state == Failed)
};

// ── Public API ──────────────────────────────────────────────────

// Start GitHub OTA update asynchronously.
// Connects to WiFi using saved NVS credentials, downloads the
// latest firmware.bin from GitHub releases, and flashes it.
// Returns true if started (or already running).
bool startGitHubUpdate();

// Call in main loop to service WiFi and HTTP.
void loop();

// Returns true if OTA is currently active.
bool isActive();

// Returns the current status (call from UI to show progress).
const GitHubOTAStatus& getStatus();

// Cancel an in-progress OTA (best effort).
void cancel();

}  // namespace github_ota
}  // namespace sigurdos
