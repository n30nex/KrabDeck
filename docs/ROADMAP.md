# SigurdOS T-Deck Roadmap

This roadmap is the current source of truth for bringing SigurdOS T-Deck to feature parity with MeshOS and MC Term, then beyond them. It is paired with [AUDIT.md](AUDIT.md), which records the 2026-06-04 end-to-end audit evidence and risk register.

## Current Baseline

- Main repo baseline: `97fb805fbb63fcbae19ed8e199e9f3659b8b331b`
- MeshCore submodule: `9a888541efaf57c38dfb886c1c1e4702f371baf1`
- Target board: LilyGo T-Deck, ESP32-S3, SX1262, ST7789, GT911, I2C keyboard, trackball
- Release build: `pio run -e SigurdOS_TDeck` succeeds, but uses 86.4% of available RAM
- Native tests: `pio test -e native_test -v` passes 396 cases, skips 1
- Audit references: [MeshOS](https://meshcore.co.uk/meshos.html), [MC Term](https://github.com/dabeani/meshcoreterm)

SigurdOS already has a strong base: standalone chat, channel and DM messaging, BaseChatMesh integration, regions, contacts, GPS, offline maps, packet logs, telemetry builds, remote test support, OTA entry points, and a sizeable native test suite. The work below is about hardening that base, closing companion-app parity gaps, and making the firmware reliable enough for field use and repeated development.

## North Star

Feature parity means a T-Deck can operate as a complete MeshCore field terminal without a phone. It should support daily chat, node discovery, repeater and room workflows, maps, telemetry, alerting, diagnostics, secure setup, update recovery, and predictable battery behavior.

Exceeding parity means SigurdOS should also provide stronger on-device observability, repeatable automated hardware tests, safer update and persistence behavior, and better offline-first workflows than phone or companion-terminal tools can provide.

## Workstreams

| Workstream | Outcome |
| --- | --- |
| Stability | Clean boot, predictable radio setup, safe sleep/wake, safe storage, no release-only debug leaks |
| Performance | Smooth UI under mesh load, bounded RAM, fast map/list rendering, controlled flash writes |
| Tests | Native, remote, hardware, interop, soak, OTA, and visual tests with reproducible artifacts |
| Debug | Structured telemetry, crash capture, packet traces, screenshots, and release-safe diagnostic policy |
| Feature parity | MeshOS and MC Term workflows available directly on the T-Deck |
| Release ops | Firmware artifacts, rollback guidance, documentation, issue workflow, and field checklists |

## Phase 0 - Documentation And Source-Of-Truth Cleanup

Goal: make the repository easy to audit and safe for parallel contributors.

Priority tasks:

- Keep this roadmap as the planning source of truth and use [AUDIT.md](AUDIT.md) for findings.
- Open follow-up issues for stale or superseded feature docs instead of mixing fixes into feature PRs.
- Reconcile [MISSING_FEATURES.md](MISSING_FEATURES.md), [FEATURES_OVERVIEW.md](FEATURES_OVERVIEW.md), [TELEMETRY_ARCHITECTURE.md](TELEMETRY_ARCHITECTURE.md), and [TELEMETRY_EXPANSION_PLAN.md](TELEMETRY_EXPANSION_PLAN.md) with the current implementation in dedicated docs PRs.
- Add a short capability matrix to the README that points to feature docs, tests, hardware docs, and this roadmap.
- Tag issues by workstream: `stability`, `performance`, `test`, `debug`, `mesh-parity`, `ui`, `hardware`, `docs`.
- Keep protected process files, contribution rules, and known-issues updates in their own PRs.

Done when:

- New contributors can find current status without reading old implementation history.
- The docs distinguish implemented, experimental, and planned features.
- Every roadmap task links to an issue before code work starts.

## Phase 1 - Stabilization And Warning Cleanup

Goal: make the current release build boring, repeatable, and warning-accounted.

Priority tasks:

- Fix the ESP32 sleep wake mask warning caused by shifting a 32-bit value for high GPIO numbers. This affects LoRa DIO and deep-sleep wake reliability.
- Remove or gate release serial commands such as screenshot, send, and nav behind an explicit debug or remote-test policy.
- Clean project warnings in `src/ui/theme.h`, `src/hal/github_ota.cpp`, `src/hal/prefs.cpp`, `src/ui/screens.cpp`, `src/ui/navigation.cpp`, and `src/ui/home_screen.cpp`.
- Make LVGL config inclusion explicit so the build no longer prints repeated `lv_conf.h` possible-failure messages.
- Resolve `MAX_TEXT_LEN` redefinition between build flags and `BaseChatMesh`.
- Separate upstream MeshCore warnings from local warnings in CI logs so local regressions are visible.
- Investigate the PlatformIO Windows cp1252 output exception printed during merged firmware generation, even though the build exits successfully.
- Add a build-warning budget. Start with "no new local warnings", then move to "no local warnings".
- Confirm release radio flags are intentional, especially `RADIOLIB_GODMODE`, USB CDC debug warnings, and static/excluded module selections.

Done when:

- Release firmware builds with no untriaged local warnings.
- All remaining third-party warnings are documented or isolated.
- Sleep/wake paths are tested on hardware.
- Release builds do not expose debug-only serial commands by accident.

## Phase 2 - Test And CI Expansion

Goal: turn current native coverage into a complete firmware validation system.

Priority tasks:

- Keep the native suite green and add coverage for warning-prone logic: regions, ACK matching, persistence migration, OTA state transitions, map tile cache, telemetry command parsing, and storage failures.
- Add smoke tests for `SigurdOS_TDeck_telemetry`, `SigurdOS_TDeck_remote_test`, and `SigurdOS_TDeck_remote_test_radio` build environments.
- Add a hardware boot matrix: cold boot, configured boot, unconfigured no-transmit boot, SD present/missing, GPS enabled/disabled, low battery, sleep/wake, first-boot onboarding.
- Add interop tests with at least one reference MeshCore node, one room server/repeater path, and one MC Term or official-client scenario.
- Add UI screenshot or widget-tree regression tests for home, chat, contacts, settings, map, telemetry, OTA, and onboarding.
- Add OTA negative tests: no credentials, wrong credentials, TLS failure, 404, interrupted download, write failure, rollback/reboot.
- Add flash-wear and persistence tests for chat history, contacts, channels, regions, WiFi profiles, and key import/export.
- Add soak tests that run mesh loop, UI loop, telemetry, GPS, and map activity for hours with heartbeat artifacts.

Done when:

- CI validates docs-only, native, release build, and telemetry/remote-test build profiles.
- Hardware test results are attached to release PRs.
- Interop failures can be reproduced from documented commands and fixtures.

## Phase 3 - Persistence And State Sync Foundation

Goal: make message, contact, channel, and region state durable enough for companion sync and field use.

Priority tasks:

- Replace whole-file chat-history rewrites with an append-friendly or journaled store that supports compaction and power-loss recovery.
- Persist message metadata beyond sender/text/timestamp/self: ACK state, delivery attempts, route/path hints, RSSI/SNR, channel/DM identity, unread state, and stable message IDs.
- Version every persistent schema and add migration tests for older SPIFFS/NVS data.
- Harden contact persistence so path metadata, shared secrets, permissions, favorite/pinned state, and manual contacts survive reboot.
- Add import/export flows for identity, contacts, channels, regions, and WiFi profiles.
- Track storage budgets for SPIFFS, SD, NVS, and PSRAM-backed caches.
- Add deduplication for repeated packets and repeated companion-sync events.

Done when:

- A power cut during message or preference writes does not corrupt the active profile.
- A companion client can sync without losing local-only history.
- State migrations are tested before schema changes ship.

## Phase 4 - Companion Transport Parity

Goal: close the largest MeshOS and MC Term feature gap: phone, app, and external-terminal workflows.

Priority tasks:

- Implement an official MeshCore BLE bridge mode or clearly documented compatible alternative.
- Add a WiFi bridge or WebUI mode for local companion access where BLE is insufficient.
- Define pairing, device PIN, key exposure, and local-network security policy.
- Support two-way sync for contacts, channels, DMs, channel messages, node config, telemetry history, and map state.
- Add connection-state UI: BLE, WiFi, companion connected, sync pending, sync failed.
- Add offline queueing so outbound companion messages survive disconnects.
- Add a protocol compatibility test plan against official MeshCore clients and MC Term behavior.

Done when:

- A user can pair a companion app or terminal, sync current state, send/receive messages, and recover from reconnects.
- Security-sensitive material is never exposed without a deliberate user action.
- Companion sync survives reboots and transport changes.

## Phase 5 - Mesh Feature Parity And Polish

Goal: match and improve the core field workflows visible in MeshOS and MC Term.

Priority tasks:

- Verify region support over real hardware and interop nodes, including scoped sends, active region switching, `$` private region keys, and collision behavior.
- Add MC Term-like contact sorting, filtering, favorites, manual contact add, and richer contact badges.
- Expand message detail views with path, hop count, RSSI/SNR, ACK status, repeaters, route hints, and packet metadata.
- Add path hash mode and route-reset workflows where compatible with MeshCore behavior.
- Harden room server and repeater management: login status, permissions, commands, fetch message history, errors, and keep-alive behavior.
- Add alerts and popups for new DMs, mentions, battery, GPS fix/loss, companion connect/disconnect, OTA, and storage failures.
- Add telemetry history views for battery/GPS/sensor data and remote node snapshots.
- Add QR/import/export flows for contacts, channels, regions, and identity backup.

Done when:

- The T-Deck can perform the main MeshOS and MC Term workflows without a separate device.
- Feature docs include screenshots or command examples for each workflow.
- Interop results are recorded for each MeshCore packet class the UI exposes.

## Phase 6 - UI, Performance, And Power

Goal: make the device feel responsive and conserve battery during real field use.

Priority tasks:

- Decompose large UI files, especially `src/ui/screens.cpp` and `src/ui/chat_screen.cpp`, into focused screen modules with shared helpers.
- Virtualize long lists for messages, contacts, packet logs, telemetry history, and map markers.
- Reduce release RAM pressure below 80% before adding large BLE or map features.
- Profile LVGL buffer use, PSRAM fallback behavior, map tile decode allocations, and worst-case chat rendering.
- Add online tile fetch and negative tile cache for map parity, while preserving offline SD-first behavior.
- Add map tile prefetch, cache budget settings, and graceful no-SD/no-network states.
- Improve autolock, sleep, wake sources, and backlight behavior around radio receive, trackball, keyboard, and touch.
- Add battery and duty-cycle status surfaces that are visible without opening deep diagnostics.
- Add keyboard editor polish: shortcuts, selection, clipboard or draft persistence, and faster correction flows.

Done when:

- UI remains responsive while receiving mesh packets, rendering maps, and saving state.
- Battery behavior is predictable and documented.
- Large data views do not cause frame spikes or heap fragmentation.

## Phase 7 - Release And Field Operations

Goal: make releases installable, recoverable, and supportable.

Priority tasks:

- Publish merged firmware artifacts with checksums and board/region notes.
- Add a release checklist covering native tests, release build, telemetry build, hardware smoke, radio interop, OTA, SD/map, GPS, and sleep/wake.
- Add recovery docs for factory reset, identity backup/restore, failed OTA, bad radio config, and unconfigured no-transmit boot.
- Add version reporting that includes firmware version, git SHA, MeshCore submodule SHA, build environment, and partition layout.
- Add firmware-manifest support for GitHub OTA and future Web Flasher flows.
- Add issue templates for bugs, hardware test reports, interop failures, and feature parity requests.

Done when:

- A non-developer can install, update, recover, and report useful diagnostics.
- Maintainers can compare field reports against exact firmware artifacts.
- Release PRs carry consistent validation evidence.

## Definition Of Parity

| Capability | Required parity behavior | Exceeds parity when |
| --- | --- | --- |
| Standalone chat | Channel and DM messaging, timestamps, ACK status, unread state | Search, details, delivery history, resilient drafts |
| Contacts | Discovery, manual add, details, route/path reset | Favorites, filters, badges, QR/import/export |
| Maps | Own position, contacts, pan/zoom, offline tiles | Online tile fetch, negative cache, prefetch, route overlays |
| Repeater/room workflows | Login, command, fetch, status, errors | Guided workflows, permission display, recovery hints |
| Companion access | BLE or WiFi client sync and messaging | Two-way offline queue, transport switching, sync diagnostics |
| Diagnostics | Packet log, telemetry, screenshots, heap/status | Crash ring, heartbeat history, reproducible hardware artifacts |
| Power | Backlight, sleep, low-battery shutdown | Wake-source tuning, power profiles, measured runtime |
| Releases | Build artifacts and install docs | Rollback/recovery flows and field-report automation |

## PR Validation Checklist

Every code PR should state which of these were run:

- `pio test -e native_test -v`
- `pio run -e SigurdOS_TDeck`
- Relevant telemetry or remote-test build
- Hardware boot test, if applicable
- Physical radio test or documented reason it was not run
- Companion/client interop test, if applicable
- Screenshot/widget-tree evidence for UI work
- Persistence migration test for storage changes

Docs-only PRs should still run `git diff --check` and should avoid changing protected process docs unless the PR is specifically scoped for that.
