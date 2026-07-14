# Merge plan for the 30 open PRs

Snapshot: `hermes-gadget/SigurdOS-tdeck`, base `dev`, starting HEAD `df31f8a`.
Scope is PRs #1106 through #1135. This is a filename-overlap plan, not a claim
that GitHub will report a textual conflict. Rebase or update each next PR onto
the new `dev` after every merge, then rerun its stated tests and required
hardware checks. Do not batch-merge this queue.

## Conflict-candidate map

| Shared path or path family | PRs | Risk / handling |
|---|---|---|
| `lib/meshcore` | #1135, #1131, #1115 | Heavy. One gitlink replaces another; merge as a cumulative submodule chain first. |
| `src/mesh/companion_adapter.cpp` | #1135, #1134, #1125, #1116, #1115, #1114, #1112, #1111, #1108, #1106 | Heavy. Rebase and review this file after every merge. |
| `src/main.cpp` | #1133, #1130, #1126, #1123, #1118 | Heavy. Rebuild the boot/loop control flow after every merge. |
| `src/comms/observed_ble_interface.*` and BLE queue tests | #1116, #1114, #1110, #1108 | Heavy BLE lifecycle/concurrency overlap. |
| `src/comms/companion_bridge.*` / companion protocol tests | #1135, #1121, #1117, #1115 | Heavy. #1117 and #1121 also change the same message-store files. |
| `src/mesh/mesh_wrapper*` and shared mocks/tests | #1135, #1126, #1115, #1106 | Heavy persistence/lifecycle overlap. |
| `src/mesh/sigurd_mesh_v2.*` | #1135, #1134, #1125 | Heavy mesh-state overlap. |
| region/scope files | #1135, #1115 | Heavy; #1135 follows the key-install work in #1115. |
| `src/hal/github_ota.cpp` and OTA contract test | #1127, #1126 | Minor-to-medium; merge #1127 before #1126. |
| `platformio.ini` | #1124, #1118, #1112 | Minor configuration overlap; merge #1124 first. |
| `src/app/map_renderer.cpp` | #1133, #1109 | Minor; merge #1109 before #1133. |
| UI screen files | #1135/#1113 (`screen_settings_radio.cpp`), #1134/#1113 (`screen_contacts.cpp`, `screen_repeaters.cpp`) | Minor-to-medium; merge #1113 after the two feature PRs. |
| `src/hal/prefs.h` | #1135, #1111 | Minor; merge #1111 after #1135. |

## 1. Safe PRs — Group A

Merge the documentation/build entries first. The isolated source PRs have no
filename overlap with any other PR in this 30-PR scope and can be merged in any
order.

Documentation/build first:

1. **#1120 — docs: add Noto Emoji license notice and complete third-party inventory**  
   Files: `LICENSES/Noto-Emoji.txt`, `README.md`.
2. **#1124 — fix/build: expand sanitizer suite, add leak sanitizer and coverage env (Closes #999)**  
   Files: `.github/workflows/pr-ci.yml`, `platformio.ini`.  
   No source-code overlap, but `platformio.ini` also appears in #1112 and #1118;
   merge #1124 before both and resolve those later PRs against the final flags.

Isolated source/test PRs, safe in any order:

- **#1132 — fix: keep GPS draining during WiFi work**  
  Files: `src/validation/gps_validation.cpp`,
  `src/validation/gps_validation_wifi.cpp`,
  `src/validation/gps_validation_wifi.h`, `test/test_wifi_sta/main.cpp`.
- **#1129 — fix: bound remote test diagnostics**  
  Files: `src/test/test_controller.cpp`, `src/test/test_controller.h`,
  `test/test_controller/test_controller.cpp`.
- **#1128 — fix: capture real crash context from core dumps**  
  Files: `src/diagnostics/telemetry_crash.cpp`,
  `src/diagnostics/telemetry_crash.h`, `test/test_telemetry_crash/main.cpp`.
- **#1122 — fix: support unknown-size multipart OTA uploads**  
  Files: `src/hal/wifi_ota.cpp`, `src/hal/wifi_ota.h`,
  `test/test_ota_auth/test_ota_auth.cpp`.
- **#1119 — fix: make SD replacement power-failure safe**  
  Files: `src/hal/sdcard.cpp`, `src/hal/sdcard.h`,
  `src/hal/sdcard_replace.h`, `test/test_sdcard/test_sdcard.cpp`.
- **#1107 — fix: scan all recent contacts in Finder fallback**  
  Files: `src/ui/finder_contact_policy.h`, `src/ui/screens/screen_finder.cpp`,
  `test/test_contact_paging/test_contact_paging.cpp`.

## 2. BLE chain — Group C

Execution order: **#1110 -> #1108 -> #1114 -> #1116**. Run this chain only
after the submodule PRs in section 5, because #1108, #1114, and #1116 also edit
`companion_adapter.cpp`.

1. **#1110 — Synchronize BLE callback and application state**  
   Files: `src/comms/ble_task_mutex.h`,
   `src/comms/observed_ble_interface.cpp`,
   `src/comms/observed_ble_interface.h`,
   `test/test_ble_frame_queue/test_ble_frame_queue.cpp`.
2. **#1108 — Add backpressure to the BLE receive queue**  
   Files: `src/comms/ble_frame_queue.h`,
   `src/comms/observed_ble_interface.cpp`,
   `src/comms/observed_ble_interface.h`, `src/mesh/companion_adapter.cpp`,
   `test/test_ble_frame_queue/test_ble_frame_queue.cpp`.
3. **#1114 — Time out unauthenticated BLE connections**  
   Files: `src/comms/ble_auth_watchdog.h`,
   `src/comms/observed_ble_interface.cpp`,
   `src/comms/observed_ble_interface.h`, `src/mesh/companion_adapter.cpp`,
   `test/test_ble_frame_queue/test_ble_frame_queue.cpp`.
4. **#1116 — Defer BLE controller initialization until enabled**  
   Files: `src/comms/ble_init_gate.h`,
   `src/comms/observed_ble_interface.cpp`,
   `src/comms/observed_ble_interface.h`, `src/mesh/companion_adapter.cpp`,
   `test/test_ble_frame_queue/test_ble_frame_queue.cpp`.

The order establishes callback locking first, then queue pressure handling,
authentication timeout state, and finally deferred controller lifecycle.

## 3. Companion/mesh chain — Group C

Run this section after the submodule and BLE chains. The complete
`companion_adapter.cpp` collision order, including cross-section PRs, is:

**#1115 -> #1135 -> #1108 -> #1114 -> #1116 -> #1106 -> #1111 -> #1112 -> #1125 -> #1134**

Other ordered overlap lanes are:

- Companion backlog: **#1115 -> #1135 -> #1117 -> #1121**.
- Mesh wrapper/persistence: **#1115 -> #1135 -> #1106 -> #1126**.
- `SigurdMeshV2`: **#1135 -> #1125 -> #1134**.
- UI crossover: **#1135 -> #1134 -> #1113**.
- Preferences crossover: **#1135 -> #1111**.

Primary execution order for PRs owned by this section:

1. **#1106 — fix: require durable mesh mutation commits**  
   Files: `src/mesh/companion_adapter.cpp`, `src/mesh/durable_mutation.h`,
   `src/mesh/mesh_wrapper.cpp`, `src/mesh/mesh_wrapper.h`,
   `src/mesh/mesh_wrapper_internal.h`, `test/mocks/mock_mesh_wrapper.cpp`,
   `test/test_mesh_wrapper/test_mesh_wrapper.cpp`.
2. **#1111 — fix: migrate stale BLE preferences**  
   Files: `src/hal/prefs.cpp`, `src/hal/prefs.h`,
   `src/hal/prefs_write_policy.h`, `src/mesh/companion_adapter.cpp`,
   `test/mocks/mock_prefs.cpp`, `test/test_prefs/test_prefs.cpp`,
   `test/test_prefs_defaults/test_prefs_defaults.cpp`.
3. **#1112 — fix: isolate USB protocol from console logs**  
   Files: `platformio.ini`, `src/diagnostics/companion_usb_console.h`,
   `src/diagnostics/log.h`, `src/mesh/companion_adapter.cpp`,
   `test/test_log/test_companion_usb_console.cpp`.
4. **#1117 — fix: page the complete companion backlog**  
   Files: `src/comms/companion_bridge.cpp`, `src/comms/companion_bridge.h`,
   `src/mesh/message_store.cpp`, `src/mesh/message_store.h`,
   `test/test_companion_protocol/test_companion_protocol.cpp`,
   `test/test_message_store/test_message_store.cpp`.
5. **#1121 — fix: refill companion sync beyond 16 messages**  
   Files: `src/comms/companion_bridge.cpp`, `src/comms/companion_bridge.h`,
   `src/mesh/message_store.cpp`, `src/mesh/message_store.h`,
   `test/test_companion_protocol/test_companion_protocol.cpp`,
   `test/test_message_store/test_message_store.cpp`.  
   This is the exact same file set as #1117. Rebase it after #1117 and confirm it
   remains additive; if it supersedes #1117, combine or close instead of merging
   two competing implementations.
6. **#1125 — fix: preserve companion channel slot invariants**  
   Files: `src/mesh/channel_slot_policy.h`, `src/mesh/companion_adapter.cpp`,
   `src/mesh/sigurd_mesh_v2.cpp`, `src/mesh/sigurd_mesh_v2.h`,
   `test/test_channel_validation/test_channel_validation.cpp`.
7. **#1134 — fix: complete repeater session failure UX**  
   Files: `src/mesh/companion_adapter.cpp`, `src/mesh/sigurd_mesh_v2.cpp`,
   `src/mesh/sigurd_mesh_v2.h`, `src/ui/notification_model.h`,
   `src/ui/notifications.cpp`, `src/ui/notifications.h`,
   `src/ui/repeater_transcript.h`, `src/ui/screens/screen_contacts.cpp`,
   `src/ui/screens/screen_repeaters.cpp`, `test/test_notifications/main.cpp`,
   `test/test_terminal/test_terminal.cpp`,
   `test/test_ui_contract/test_ui_contract.cpp`.
8. **#1113 — fix: route nested screens through navigation history**  
   Files: `src/diagnostics/telemetry.cpp`, `src/ui/navigation.cpp`,
   `src/ui/navigation.h`, `src/ui/screens/screen_contacts.cpp`,
   `src/ui/screens/screen_map.cpp`, `src/ui/screens/screen_radio_setup.cpp`,
   `src/ui/screens/screen_repeaters.cpp`,
   `src/ui/screens/screen_settings_radio.cpp`, `test/test_build/test_build.cpp`,
   `test/test_navigation/test_navigation.cpp`,
   `test/test_navigation_contract/test_navigation_contract.cpp`.  
   This is Group B rather than core mesh work, but scheduling it here resolves
   its screen overlaps after #1135 and #1134.

## 4. `main.cpp` chain — Group C

The five direct `main.cpp` PRs should be merged in this order:

**#1118 -> #1123 -> #1126 -> #1130 -> #1133**

Two Group B prerequisites fit into that chain: merge #1127 immediately before
#1126 because they share GitHub OTA code/tests, and merge #1109 before #1133
because they share `map_renderer.cpp`. The full execution order for this section
is therefore **#1118 -> #1123 -> #1127 -> #1126 -> #1130 -> #1109 -> #1133**.

1. **#1118 — fix: add OTA boot-health rollback**  
   Files: `platformio.ini`, `sdkconfig.debug`, `sdkconfig.defaults`,
   `src/hal/ota_boot_health.cpp`, `src/hal/ota_boot_health.h`, `src/main.cpp`,
   `test/test_ota_boot_health/main.cpp`,
   `test/test_ota_boot_health/test_ota_boot_health.cpp`.
2. **#1123 — fix: restore main-loop telemetry dispatch**  
   Files: `src/main.cpp`, `test/README.md`, `test/test_main_loop/main.cpp`,
   `test/test_main_loop/test_main_loop.cpp`.
3. **#1127 — fix: fail closed for GitHub OTA channels**  
   Files: `src/hal/github_ota.cpp`, `src/hal/github_ota_plan.cpp`,
   `src/hal/github_ota_plan.h`,
   `test/test_github_ota_contract/test_github_ota_contract.cpp`.
4. **#1126 — fix: use one orderly persistence-safe deep-sleep path**  
   Files: `src/hal/github_ota.cpp`, `src/hal/github_ota.h`,
   `src/hal/tdeck_board.h`, `src/main.cpp`, `src/mesh/mesh_init_lifecycle.h`,
   `src/mesh/mesh_wrapper.cpp`, `src/mesh/mesh_wrapper.h`,
   `test/mocks/mock_mesh_wrapper.cpp`,
   `test/test_github_ota_contract/test_github_ota_contract.cpp`,
   `test/test_mesh_wrapper_internal/test_mesh_wrapper_internal.cpp`,
   `test/test_tdeck_board/test_tdeck_board.cpp`.
5. **#1130 — fix: validate RTC display retry state**  
   Files: `src/hal/display_retry_state.h`, `src/main.cpp`,
   `test/test_display_retry_state/main.cpp`,
   `test/test_display_retry_state/test_display_retry_state.cpp`.
6. **#1109 — fix: reuse map tile buffers before replacement allocation**  
   Files: `src/app/map_renderer.cpp`, `src/app/tile_cache.h`,
   `test/test_map/test_map.cpp`.
7. **#1133 — fix: add staged loopTask watchdog coverage**  
   Files: `src/app/map_renderer.cpp`, `src/hal/boot_watchdog.cpp`,
   `src/hal/boot_watchdog.h`, `src/main.cpp`,
   `test/test_boot_watchdog/main.cpp`,
   `test/test_boot_watchdog/test_boot_watchdog.cpp`.

For every step, inspect the complete setup/loop order rather than accepting a
mechanical conflict resolution: OTA validation, telemetry dispatch, sleep
shutdown, display retry, and watchdog feeding all alter lifecycle control flow.

## 5. Submodule chain — Group C, merge first among overlapping groups

Required order: **#1131 -> #1115 -> #1135**. Execute this chain before the BLE
and companion/mesh chains despite its later section position in this document.

1. **#1131 — fix: make RadioLib ISR completion race-free**  
   Files: `lib/meshcore`.
2. **#1115 — fix: install private flood-scope keys before activation**  
   Files: `lib/meshcore`, `src/comms/companion_bridge.cpp`,
   `src/comms/companion_bridge.h`, `src/mesh/companion_adapter.cpp`,
   `src/mesh/mesh_wrapper.cpp`, `src/mesh/mesh_wrapper.h`,
   `src/mesh/regions.cpp`, `src/mesh/regions.h`,
   `src/mesh/scope_activation_policy.h`, `src/mesh/scope_key_hex.h`,
   `test/mocks/SHA256.h`,
   `test/test_companion_protocol/test_companion_protocol.cpp`,
   `test/test_mesh_wrapper_internal/test_mesh_wrapper_internal.cpp`,
   `test/test_transport_key_store/main.cpp`,
   `test/test_transport_key_store/test_transport_key_store.cpp`.
3. **#1135 — fix: prove scoped flood interop**  
   Files: `lib/meshcore`, `src/comms/companion_bridge.cpp`,
   `src/comms/companion_bridge.h`, `src/hal/prefs.h`,
   `src/mesh/companion_adapter.cpp`, `src/mesh/mesh_wrapper.cpp`,
   `src/mesh/regions.cpp`, `src/mesh/regions.h`,
   `src/mesh/scope_key_hex.h`, `src/mesh/sigurd_mesh_v2.cpp`,
   `src/ui/chat_screen.cpp`, `src/ui/screens/screen_regions.cpp`,
   `src/ui/screens/screen_settings_radio.cpp`,
   `test/mocks/mock_mesh_wrapper.cpp`,
   `test/test_companion_protocol/test_companion_protocol.cpp`,
   `test/test_mesh_wrapper_internal/test_mesh_wrapper_internal.cpp`,
   `test/test_regions/test_regions.cpp`.

Submodule gate: after each merge, the next PR must be rebased and its gitlink
updated to a fetchable MeshCore commit containing the earlier submodule changes.
Do not resolve this by choosing one pointer and silently dropping the other two.

## 6. Remaining PRs with no conflicts

There are no additional PRs outside the chains above. The complete set with no
filename overlap is already in Group A: **#1120, #1132, #1129, #1128, #1122,
#1119, and #1107**. #1124 has no source-code overlap but is excluded from this
strict list because it shares `platformio.ini` with #1112 and #1118.

## 7. Flat merge sequence

`#1120 #1124 #1132 #1129 #1128 #1122 #1119 #1107 #1131 #1115 #1135 #1110 #1108 #1114 #1116 #1106 #1111 #1112 #1117 #1121 #1125 #1134 #1113 #1118 #1123 #1127 #1126 #1130 #1109 #1133`
