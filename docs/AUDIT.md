# SigurdOS T-Deck End-To-End Audit

Date: 2026-06-04

This audit covers the SigurdOS T-Deck MeshCore firmware at commit `97fb805fbb63fcbae19ed8e199e9f3659b8b331b`, with MeshCore submodule `9a888541efaf57c38dfb886c1c1e4702f371baf1`. The review focused on stability, performance, tests, debug tooling, and feature parity against [MeshOS](https://meshcore.co.uk/meshos.html) and [MC Term](https://github.com/dabeani/meshcoreterm).

No physical hardware test was performed during this audit. Findings are based on source review, documentation review, native test execution, release build execution, and comparison against the external parity targets.

## Executive Summary

SigurdOS T-Deck is beyond a prototype. It already contains a functional embedded application with a broad UI, MeshCore/BaseChatMesh integration, channel and direct messaging, contacts, regions, GPS, offline maps, OTA entry points, structured telemetry builds, remote-test support, and a strong native test suite.

The main risk is not missing code volume. The main risk is release hardening. The current release build succeeds, but it uses 86.4% of available internal RAM, emits a large warning stream, contains several release/debug policy questions, and has limited automated coverage for hardware, radio interop, sleep/wake, OTA, storage failure, and long-running operation.

The largest feature-parity gap is companion/client behavior. MeshOS and MC Term emphasize phone/terminal-style connected workflows, map/cache behavior, visible connection state, contact/message detail, and management UX. SigurdOS has many of the primitives, but it still needs durable state sync, BLE/WiFi bridge parity, richer message/contact detail, stronger map behavior, and repeatable interop tests.

## Validation Evidence

Commands run during the audit:

| Check | Result |
| --- | --- |
| `git submodule update --init --recursive` | MeshCore submodule checked out at `9a888541efaf57c38dfb886c1c1e4702f371baf1` |
| `pio --version` | PlatformIO Core 6.1.18 |
| `pio test -e native_test -v` | 397 test cases collected; 396 succeeded; 1 skipped; duration 00:04:31.818 |
| `pio run -e SigurdOS_TDeck` | Success; duration 00:10:29.676 |
| Release build RAM | 283,036 of 327,680 bytes used, 86.4% |
| Release build flash | 1,935,401 of 6,553,600 bytes used, 29.5% |

The firmware build generated a merged image at `.pio/build/SigurdOS_TDeck/firmware-merged.bin`. After image generation, PlatformIO printed a Windows cp1252 `UnicodeEncodeError` in its output reader while still reporting environment success. Treat this as a contributor-tooling issue, not a firmware build failure.

## Architecture Map

| Area | Observed implementation |
| --- | --- |
| Boot orchestration | `src/main.cpp` initializes serial, board, battery, buzzer, SPIFFS, GPS gate, display, mesh, UI, SD, maps, WiFi auto-connect, telemetry, then runs display, OTA, WiFi, GPS, mesh, UI, and telemetry loops |
| Mesh wrapper | `src/mesh/mesh_wrapper.cpp` owns radio setup, identity/channel/contact persistence, BaseChatMesh construction, region APIs, advert timing, send/receive bridges, and shutdown saves |
| Mesh extension | `src/mesh/sigurd_mesh_v2.h` extends BaseChatMesh with discovery, telemetry responses, pending requests, ACK tracking, signal history, region-aware scoped send hooks, login/command/fetch helpers, and UI callbacks |
| UI | `src/ui/*` contains the LVGL home grid, chat, contacts, settings, map screen wiring, onboarding, navigation, and remote-display helper behavior |
| Persistence | NVS preferences in `src/hal/prefs.cpp`; SPIFFS contacts/channels/messages/custom vars; SD map tiles |
| Maps | `src/app/map_renderer.cpp` performs Web Mercator tile rendering, SD PNG decode, PSRAM tile cache, markers, panning, and zooming |
| Diagnostics | `src/diagnostics/*`, `src/test/test_controller.cpp`, display screenshot paths, packet logs, and telemetry collectors provide structured and serial debug surfaces |
| OTA/WiFi | `src/hal/wifi_ota.cpp` provides AP upload OTA and STA helpers; `src/hal/github_ota.cpp` provides GitHub release download/write state |

## Feature Parity Snapshot

| Capability | SigurdOS current state | MeshOS / MC Term expectation | Gap |
| --- | --- | --- | --- |
| Standalone channel/DM chat | Present with persisted messages, timestamps, search, ACK indicator, emoji support, and UTF-8 truncation helpers | Standalone messaging with clear delivery and history workflows | Mostly present; message metadata persistence and richer detail remain |
| Contacts | Discovery, LRU cap, details, removal, path reset, telemetry request, QR display pieces | Sort/filter, favorites, badges, public key/manual add, rich metadata | Needs manual/favorite/filter UX and persistent richer metadata |
| Regions | Region APIs and tests are present | Region/channel isolation and scoped sends must interop | Needs hardware and interop proof |
| Maps | Offline SD tiles, contact markers, panning/zooming, cache | MC Term has slippy maps, online tile fetching, negative tile cache, map source selection | Needs online/cache parity and map performance tests |
| Repeater/room management | Login/command/fetch primitives exist through BaseChatMesh path | MeshOS/MC Term expose management workflows and status | Needs UI polish, errors, permissions, and interop validation |
| Telemetry | Structured telemetry builds, collectors, packet logs, commands, battery/GPS response | Visible diagnostics and telemetry history | Needs history UI, release-safe policy, and doc reconciliation |
| Companion client | No confirmed full BLE/WiFi companion bridge parity | MeshOS and MC Term center on connected client workflows | Largest functional gap |
| OTA/update | AP OTA and GitHub OTA entry points exist | Safe update, progress, recovery, and release artifacts | Needs negative tests, rollback docs, manifests |
| Power | Low-battery shutdown, backlight control, sleep paths | Field terminals need predictable runtime and wake behavior | Needs measured hardware profiles and wake-source fixes |

## Stability Findings

### S1 - Release build has high internal RAM pressure

The release build uses 283,036 bytes of 327,680 bytes of RAM, leaving roughly 44 KB of internal RAM headroom. That is risky for BLE bridge work, HTTP/TLS OTA, LVGL allocations, map decode paths, telemetry rings, and future companion sync.

Recommendation: make RAM a release gate. Track internal RAM, PSRAM, minimum heap, stack high-water marks, and worst-case map/chat/OTA scenarios. Reduce release RAM below 80% before adding large companion features.

### S2 - Sleep/wake GPIO warning needs immediate attention

The build reports left-shift warnings around high GPIO numbers in the LoRa DIO wake configuration path. On ESP32-S3, GPIO numbers above 31 require 64-bit masks. A 32-bit shift can break wake-source behavior.

Recommendation: fix the mask type and add hardware sleep/wake tests for LoRa DIO, touch, keyboard/trackball, button wake, and low-battery shutdown.

### S3 - Release/debug command policy is not fully clean

The display path accepts serial commands such as `SCREENSHOT`, `SEND`, and `NAV` in non-remote-test release builds. These are useful, but they should be governed by an explicit release, debug, or remote-test policy.

Recommendation: gate command injection behind `SIGURDOS_REMOTE_TEST` or a documented opt-in debug flag. Keep screenshot capture available for tests, but not accidentally exposed in field release builds.

### S4 - OTA and WiFi paths can starve the main loop

The main loop is cooperative. GitHub OTA performs HTTP connection and stream/write work inside the loop state machine, and AP/STA WiFi paths contain blocking delays and restart delays. These are acceptable for controlled update flows, but they need explicit UI and watchdog expectations.

Recommendation: add OTA state-machine tests, negative tests, watchdog/heartbeat telemetry during OTA, and user-visible "mesh paused/update active" semantics where appropriate.

### S5 - Persistence works but is not yet power-loss robust

Messages are stored in SPIFFS with a bounded record format, but the current path rewrites the message file periodically and on shutdown/config transitions. Contacts persist a compact binary record, but not all route/path/shared metadata survives reboot.

Recommendation: move message history toward journaled or append-friendly storage, version schemas, add migration tests, and persist enough metadata for companion sync and message details.

### S6 - Documentation has source-of-truth drift

The code now implements several capabilities that older roadmap and telemetry-plan documents still present as planned or partial. This creates review risk for parallel contributors.

Recommendation: keep roadmap and audit findings separate, then run dedicated follow-up docs PRs for protected or historical docs that need reconciliation.

## Performance Findings

### P1 - UI modules are large and hard to reason about

`src/ui/screens.cpp`, `src/ui/chat_screen.cpp`, `src/mesh/mesh_wrapper.cpp`, and `src/mesh/sigurd_mesh_v2.h` carry a large amount of behavior. This increases regression risk because small UI or mesh changes can touch broad files.

Recommendation: split large screens and mesh helpers after warning cleanup. Keep `mesh_wrapper.h` stable while extracting screen-specific modules and persistence services.

### P2 - Map rendering is powerful but allocation-heavy

The map renderer decodes PNG tiles, converts to RGB565, and caches tiles in PSRAM. It handles SD/offline workflows, but online fetching, negative cache, cache budget settings, and stress tests are missing.

Recommendation: add map performance tests for cache hit/miss, SD missing, high zoom, contact markers, and low PSRAM. Add online tile fetch only after cache budgets and UI responsiveness are measured.

### P3 - Chat history save strategy can create latency and flash wear

Periodic whole-file message saves are simple and testable, but they can become expensive as history grows. They also do not persist ACK and route metadata.

Recommendation: move to a log/journal design with compaction, stable IDs, deduplication, and migration coverage.

### P4 - Build time and dependency volume are high

The target build took more than ten minutes on this machine and compiled broad dependency sets. That is workable for release builds, but painful for frequent CI and contributor iteration.

Recommendation: keep fast native tests, add targeted build profiles, and make warning categorization concise so contributors can find local regressions quickly.

## Test Findings

### Strengths

- Native tests cover many key areas: battery, build assumptions, channel validation, chat truncation, emoji, GPS, home screen, keyboard, layout, map, mesh messaging, mesh wrapper, navigation, pins, preferences, regions, SD card, terminal, theme, touch, and trackball.
- The remote test controller is unusually valuable for an embedded UI firmware project. It can drive navigation, typing, screenshot capture, widget listing, radio setup, channels, messages, repeaters/rooms, login, fetch, and diagnostics.
- Telemetry and debug builds are already represented in `platformio.ini`.

### Gaps

- No hardware test evidence was produced for this audit.
- No CI evidence was reviewed for physical radio interop, sleep/wake, OTA, GPS, SD map performance, or long-running mesh/UI soak.
- ACK matching and delivery status need deeper tests for multi-contact, repeated ACK, timeout, reboot, and persisted state.
- OTA needs both positive and negative tests.
- Map rendering needs low-resource and missing-resource tests.
- Companion-client behavior has no parity test harness yet.

Recommendation: keep the current native suite as the baseline, then add hardware/remote-test artifacts and interop fixtures before declaring feature parity.

## Debug And Telemetry Findings

### Strengths

- The project has structured telemetry files, heartbeat rings, crash-related helpers, collectors, and remote-test command surfaces.
- Packet logs and UI-accessible diagnostics exist.
- Serial screenshot capture and widget-tree commands are present and useful for automated testing.
- Debug flags are granular across display, mesh, UI, map, and diagnostics.

### Risks

- Release-safe command and serial-output policy needs tightening.
- Telemetry architecture docs describe planned work that now partly exists in code, making it hard to know what is authoritative.
- The build warning stream is noisy enough that important warnings are easy to miss.
- The PlatformIO output encoding exception on Windows can distract from real build issues.

Recommendation: define debug surfaces by build type: release, debug, telemetry, remote test, and hardware CI. Each should have explicit serial command availability and memory budget expectations.

## Warning Categories Observed

Local or local-adjacent warnings to prioritize:

- `src/hal/tdeck_pins.h` / MeshCore board helpers: high-GPIO left shift warning for wake masks
- `src/ui/theme.h`: inline variables require C++17 while target build is not configured for C++17
- `src/hal/github_ota.cpp`: progress string may truncate
- `src/hal/prefs.cpp`: small key-buffer truncation warnings for WiFi profile keys
- `src/ui/screens.cpp`: multi-character constant/comparison issue and possible status string truncation
- `src/mesh/mesh_wrapper.cpp` and `src/mesh/sigurd_mesh_v2.h`: `memset` on non-trivial `ContactInfo`
- `src/ui/navigation.cpp`, `src/ui/home_screen.cpp`: unused variables

Third-party or submodule warnings to isolate:

- Repeated LVGL `lv_conf.h` possible-failure messages
- MeshCore reorder, unused-variable, and non-trivial memset warnings
- RadioLib `RADIOLIB_GODMODE` and USB CDC debug warnings
- QR library unknown pragmas and unsigned comparison warnings
- Melopero RV3028 ambiguous `requestFrom` overload warning

## Recommended Priority Order

1. Fix warning hygiene and release/debug policy before adding major features.
2. Add hardware sleep/wake and OTA negative tests.
3. Reduce RAM pressure and record runtime heap/PSRAM telemetry in release-like scenarios.
4. Replace message persistence with a schema-versioned, power-loss-aware store.
5. Build companion BLE/WiFi bridge parity and state sync.
6. Expand MeshOS/MC Term parity UX: contact filters, message details, map cache, room/repeater management, alerts.
7. Add release artifacts, recovery docs, and field validation checklists.

## Audit Conclusion

SigurdOS T-Deck has a credible foundation and a broad feature surface, but it should be treated as a beta firmware until hardware, warning, persistence, companion-sync, and release-operation gaps are closed. The highest-leverage next step is not a single new feature. It is to stabilize the build, make tests span actual hardware behavior, and turn the existing primitives into reliable MeshOS/MC Term parity workflows.
