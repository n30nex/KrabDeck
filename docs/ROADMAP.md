# SlopOS-TDeck Implementation Roadmap

**Audience: AI agents implementing features.** This is the *ordered, how-to* companion to [`MISSING_FEATURES.md`](MISSING_FEATURES.md). Read both:

| Document | Answers | Use it for |
|----------|---------|-----------|
| [`MISSING_FEATURES.md`](MISSING_FEATURES.md) | **What** is missing and **where** to find it in upstream MeshCore | The catalog: per-feature description + clickable `meshcore-dev/MeshCore` source links |
| **`ROADMAP.md`** (this file) | **In what order** to build it and **how** | The plan: dependencies, step-by-step guidance, pitfalls, test plans, done-criteria |

> **This file replaces the "Implementation Plan" section that used to live at the bottom of `MISSING_FEATURES.md`.** That section was removed; this is its expanded successor.

---

## ⚠️ READ THIS FIRST — mandatory context for any agent

You are working on embedded C++ firmware. Mistakes here are expensive (they require a hardware reflash to observe). **Slow down and follow the process.**

### Required reading before you touch anything
1. [`CLAUDE.md`](../CLAUDE.md) / [`AGENTS.md`](../AGENTS.md) — architecture, conventions, the **Code Audit Checklist**, and **Rejection triggers**. Do not skip the rejection triggers — they are the exact reasons a PR gets auto-declined.
2. [`CONTRIBUTING.md`](../CONTRIBUTING.md) — the contribution workflow. **Issue-first is mandatory: no issue = no PR.**
3. [`docs/KNOWN_ISSUES.md`](KNOWN_ISSUES.md) — known bugs. Several roadmap items overlap; check before starting.
4. [`docs/MISSING_FEATURES.md`](MISSING_FEATURES.md) — the upstream source references for the feature you're building.

### The non-negotiable loop for every task
```
1. Find or open a GitHub issue on hermes-gadget/SlopOS-tdeck
2. git checkout dev && git pull origin dev
3. git checkout -b <type>/<short-name>-<issue#>
4. pio test -e native_test -v        # confirm baseline GREEN before you change anything
5. ... make changes + ADD TESTS ...
6. pio test -e native_test -v        # all green
7. pio run -e SlopOS_TDeck           # firmware must build
8. commit (conventional message), push, open PR targeting dev
9. PR body MUST declare hardware testing: "Remote test", "Physical hardware test", or both
```
If you cannot run hardware, say so explicitly in the PR and use **remote test mode** where the feature allows it (see CLAUDE.md → Remote Test Controller). **Never** switch the build to `SlopOS_TDeck_remote_test` or touch the radio config without the user's explicit say-so.

### The architecture you must respect
```
UI layer (src/ui/*)                ← screens, never touches MeshCore directly
      │  calls only
      ▼
mesh_wrapper.h  (slopos::mesh::*)   ← THE SEAM. Public API the UI depends on.
      │  wraps
      ▼
SlopMesh  (src/mesh/slop_mesh.h)    ← our subclass of MeshCore
      │  extends
      ▼
::mesh::Mesh  (lib/meshcore/)       ← upstream library
```
**Golden rule: keep the `mesh_wrapper.h` public API stable.** The UI is insulated by it. If you change mesh internals, translate back to the existing wrapper structs (`slopos::mesh::ContactInfo`, `MeshMessage`, etc.) so screens don't change. This seam is what makes large refactors (see Phase 0) survivable.

### Traps that have already bitten this codebase (do not repeat)
- **ACK value is NOT a CRC-32.** It is the first 4 bytes of SHA-256 over a recipient-pubkey-dependent buffer. Computing a CRC will *never* match. (`MISSING_FEATURES.md` → "Message delivery status".)
- **Channel hash matching uses ONE byte** in the packet header, not the full hash. `searchChannelsByHash` compares `hash[0]` only — matching upstream `BaseChatMesh`. ~11% collision on 8 channels is expected; do not "fix" it to a full memcmp or you break interop.
- **`strncpy` does not null-terminate** when the source is too long. Always `dest[n-1] = '\0'`.
- **UTF-8 truncation** can split a 4-byte emoji mid-codepoint → invalid UTF-8 over the mesh. Use `slopos::utf8_truncate_bytes()` (already used in `slop_mesh.h`).
- **`lv_obj_del` inside an event handler** must be `lv_obj_del_async()`.
- **`lv_scr_load_anim(..., true)`** deletes the old screen + all children; register `LV_EVENT_DELETE` to null any globals pointing into it.
- **Stack arrays as LVGL `user_data`** dangle on click. Use `static` arrays.
- **Adding a `NodePrefs` field**: old saved prefs won't have it. `prefs_get()` zero-fills missing keys — follow the existing default-value pattern in `NodePrefs::set_defaults()` (`src/hal/prefs.h`).
- **`ESP.restart()`** before a flash write completes loses the write. `saveState()`/`saveChannels()` then delay ~100 ms before restart.
- **Debug output** must be guarded by `#if defined(SLOPOS_DEBUG)` — unconditional `Serial.printf` is a rejection trigger.

### Where things live (verified paths)
| Concern | File |
|---------|------|
| Mesh subclass | `src/mesh/slop_mesh.h` |
| Wrapper / public API | `src/mesh/mesh_wrapper.cpp` / `.h` |
| Persisted settings | `src/hal/prefs.cpp` / `.h` (`struct NodePrefs`, `prefs_get/set/save`) |
| GPS NMEA | `src/hal/gps.cpp` / `.h` |
| Keyboard (I2C) | `src/hal/keyboard.cpp` / `.h` |
| Settings / Radio Setup / Finder / Signal / Contacts screens | `src/ui/screens.cpp` |
| Chat (DM + channels) | `src/ui/chat_screen.cpp` |
| Navigation / back-stack | `src/ui/navigation.cpp` |
| Theme helpers / colors | `src/ui/theme.h` |
| Mesh tests | `test/test_mesh_messaging/`, `test/test_mesh_wrapper/` |
| Mocks (Arduino, lvgl, RadioLib, etc.) | `test/mocks/` |
| Upstream MeshCore | `lib/meshcore/` (submodule, v1.15.0) |

> **Note for the agent:** the old MISSING_FEATURES plan referenced `test/slop_mesh_test.cpp` — that file does **not** exist. The real mesh tests are the two dirs above.

---

## Phase 0 — Migrate `SlopMesh` onto `BaseChatMesh` (foundational — do before Phase 4) ✅ COMPLETED

**Status: ✅ Done.** PR #223 (V2 behind flag), PR #224 (cutover: V2 as default). All parity items pass.

**This is the decided architectural direction.** It is the single highest-leverage change on the board: it converts most of Phase 4 (and chunks of 3 and 5) from large bespoke protocol work into small "expose an existing method" PRs. **Do not start any Phase 4 feature until this migration has cut over.**

> **Agent note:** This is large and risky. Do it incrementally behind a flag, prove parity before deleting anything, and expect several PRs — not one. If anything in this section is unclear, stop and ask the user rather than guessing.

### Why we're doing it

`SlopMesh` currently extends `::mesh::Mesh` directly — a deliberately minimal subclass. One layer up in the *same* library sits `BaseChatMesh` ([`lib/meshcore/src/helpers/BaseChatMesh.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/BaseChatMesh.h)), which the reference companion radio uses. It already implements, correctly and tested:

- `sendMessage()` **with ACK tracking** (`expected_ack`), `processAck()`
- `sendLogin()`, `sendCommandData()` (remote repeater/room admin), connection keep-alive sessions
- `sendRequest()` x2 (the REQ/RESPONSE framework), `sendAnonReq()`, `sendGroupData()`
- `resetPathTo()`, `removeContact()`, `addContact()`, `lookupContactByPubKey()`
- `exportContact()` / `importContact()` (QR/URI sharing payloads)
- correct `onAckRecv`, path learning, send-timeout handling

**Almost the entire Phase 4 is already written inside `BaseChatMesh`.** Building those on raw `Mesh` would mean reimplementing them by hand — repeatedly hitting the traps listed above. Inheriting `BaseChatMesh` gets them for free.

> **This is NOT a "MeshCore version" change.** We are on current MeshCore (v1.15.0). `Mesh` and `BaseChatMesh` both ship in it. We are moving `SlopMesh` up one *layer* of subclassing — no submodule bump required.

### Why the blast radius is small

The cost is concentrated in **one place**: adopting `BaseChatMesh`'s `ContactInfo[MAX_CONTACTS]` and `ChannelDetails` models in place of our `SlopContact`/`SlopChannel`, and implementing its ~12 pure-virtual hooks. Because the UI only ever sees `slopos::mesh::ContactInfo` (the wrapper struct in `mesh_wrapper.h`), the change is contained to **`slop_mesh.h` + `mesh_wrapper.cpp`** — translate `BaseChatMesh::ContactInfo` → the wrapper struct and **the UI layer does not change at all.** That seam is exactly why this migration is survivable.

### Migration plan — spike, prove parity, then cut over (never big-bang)

1. **Spike branch.** Create `SlopMeshV2 : public BaseChatMesh` alongside the existing `SlopMesh`. Implement the pure virtuals (`onMessageRecv`, `onChannelMessageRecv`, `onDiscoveredContact`, `processAck`, `onContactResponse`, `onCommandDataRecv`, `onSignedMessageRecv`, `calcFloodTimeoutMillisFor`, `calcDirectTimeoutMillisFor`, `onSendTimeout`, `onContactRequest`). Use [`examples/companion_radio/MyMesh.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp) as a complete reference implementation of every one of these.
2. **Define `MAX_CONTACTS` and `MAX_GROUP_CHANNELS`** via build flags (`BaseChatMesh` reads them; defaults are 32 / undefined). Match or exceed today's `SLOP_MAX_CONTACTS = 64` / `SLOP_MAX_CHANNELS = 8`.
3. **Persistence.** `BaseChatMesh` stores contacts/channels via `getBlobByKey`/`putBlobByKey` (the `DataStoreHost` pattern in the companion radio). Wire these to NVS/SPIFFS. **Note:** existing devices will lose their saved contacts on upgrade — that is acceptable because contacts are re-learned from adverts within minutes. Identity is stored separately and must be preserved.
4. **Keep the wrapper API byte-identical.** `mesh_wrapper.cpp` translates `BaseChatMesh::ContactInfo` → `slopos::mesh::ContactInfo`. Do not change any signature in `mesh_wrapper.h`.
5. **Parity checklist — prove ALL of these pass before cutover** (native tests + hardware/remote-test):
   - [x] DM send + receive (`test_mesh_messaging`)
   - [x] Channel text send + receive (hashtag + PSK channels)
   - [x] Advert parse → contact added with name/type/location/RSSI/SNR
   - [x] Contact LRU eviction at the cap
   - [x] Trace route round-trip
   - [x] Ping Nearby (control PING/PONG)
   - [x] Duty-cycle factor override still applied
   - [x] Channel persistence across reboot
6. **Cutover.** Replace `SlopMesh` with `SlopMeshV2`, delete the old class, update `mesh_wrapper.cpp` construction. Run the full suite again.
7. **Then** proceed to Phase 4 — each feature is now mostly wrapper plumbing + UI.

> **Sequencing tip:** the Phase 1 quick wins below are independent of this migration and make good warm-up PRs while the migration is being planned/reviewed. Everything in Phase 4, and the ACK work in Phase 3, waits for cutover.

---

## Phase 1 — Quick wins (mostly UI over backend that already exists) ✅ COMPLETED

> **Status: ✅ All 8 items done.** See overview below for the specific PRs. Safe to do before or during Phase 0 migration. Great first tasks. Each is small; do them as separate PRs.

### 1.1 — RX gain boost toggle — S ✅
- **Status:** Done. Toggle in Radio Setup → `s_rx_gain` → `applyRadioParams()` at init. Persisted in NodePrefs.
- **PR:** Part of Phase 0 development (early PRs on dev).

### 1.2 — Duty cycle UI — M ✅
- **Status:** Done. Settings cycle control (0/1/5/10/25/50/100) + remaining budget displayed on Signal screen via `getRemainingTxBudget()`.
- **PR:** Part of Phase 0 development (early PRs on dev).

### 1.3 — Zero-hop ping in Finder UI — M ✅
- **Status:** Done. "Ping Nearby" button with 3 s listening countdown, 30 s cooldown, results sorted by RSSI. All backend `sendPingNearby()`, `pingIsActive()`, etc. wired to Finder screen.
- **PR:** Part of Phase 0 development (early PRs on dev).

### 1.4 — Graceful shutdown from UI — S ✅
- **Status:** Done. "Shut down" button in Settings with confirmation dialog. Calls `saveState()` + `saveChannels()`, delay, then deep sleep.
- **PR:** Part of Phase 0 development (early PRs on dev).

### 1.5 — Message timestamps in chat bubbles — S ✅
- **Status:** Done. `format_time()` renders `HH:MM` in bubble header (sender name + timestamp row). For >24 h old messages shows date.
- **PR:** Part of Phase 0 development (early PRs on dev).

### 1.6 — Channel removal — S ✅
- **Status:** Done. Delete/× button on each channel row in channel list. Calls `removeChannel(idx)` → shifts `_channels` array → persists via `saveChannels()`.
- **PR:** Part of Phase 0 development (early PRs on dev).

### 1.7 — Contact removal — S ✅
- **Status:** Done. "Remove Contact" button on Contact Detail screen with confirmation dialog. Calls `removeContact()` through the wrapper.
- **PR:** Part of Phase 0 development (early PRs on dev).

### 1.8 — Reset path to a contact — S ✅
- **Status:** Done. "Reset Path" button on Contact Detail screen. Calls `resetPathTo()` → clears `out_path_len` → next message floods and re-learns path.
- **PR:** Part of Phase 0 development (early PRs on dev).

---

## Phase 2 — Radio & node configuration

> Some `NodePrefs` fields already exist (`advert_interval`, `advert_type`, `flood_max_hops`) — check `src/hal/prefs.h` before adding new ones.

### 2.1 — Periodic auto-advert — S
- **Upstream ref:** MISSING_FEATURES → "Periodic auto-advert".
- **Backend status:** `NodePrefs::advert_interval` **already exists** (half-minute units). What's missing is the loop timer + Settings toggle.
- **Steps:** in the wrapper `loop()`, re-advert when `advert_interval > 0` and the interval elapsed since `getLastAdvertTime()`. Add a Settings toggle/interval control.
- **Trap:** respect duty cycle; don't advert if the budget is exhausted.

### 2.2 — Temporary radio config (no NVS write) — M
- **Upstream ref:** MISSING_FEATURES → "Temporary radio config".
- **Backend status:** `applyRadioParams()` (live, no NVS) and `revertRadioParams()` exist.
- **Steps:** add a "Try (no save)" button in Radio Setup wired to `applyRadioParams()`; start a revert timer that calls `revertRadioParams()`; a "Keep" button persists via `prefs_save()`.

### 2.3 — Auto-add contact configuration — M
- **Upstream ref:** MISSING_FEATURES → "Auto-add contact configuration".
- **Backend status:** `onAdvertRecv` already gates by `flood_max_hops`. Extend with a per-type bitmask.
- **Steps:** add `autoadd_config` (bitmask) + `autoadd_max_hops` to `NodePrefs`; gate auto-add by `parser.getType()` in `onAdvertRecv`; Settings checklist (chat/repeater/room/sensor) + max-hops slider.
- **Ref constants:** `AUTO_ADD_*` in [`examples/companion_radio/MyMesh.h`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.h).

### 2.4 — Advert location-share policy (privacy) — S
- **Upstream ref:** MISSING_FEATURES → "Advert location-share policy".
- **Steps:** add `advert_loc_policy` to `NodePrefs` (0 = none, 1 = share); gate the lat/lon `broadcastAdvert` overload on it; Settings toggle "Share my location in adverts".

### 2.5 — GPS enable / read-interval — S
- **Upstream ref:** MISSING_FEATURES → "GPS enable / read-interval control".
- **Steps:** add `gps_enabled` + `gps_interval` to `NodePrefs`; gate `gps.cpp` polling; Settings controls. Helps battery life.

### 2.6 — Custom variables (key-value store) — S
- **Upstream ref:** MISSING_FEATURES → "Custom variables".
- **Steps:** small NVS-backed `name:value` store; Terminal `getvar`/`setvar` commands. Lower priority — only do if a concrete need appears.

### 2.7 — Keyboard backlight & message-cap controls — S each
- **Upstream ref:** MISSING_FEATURES → "Keyboard backlight control", "Message history cap control".
- **Backend status:** `NodePrefs::kbd_backlight` exists; a msg-cap field may need adding.
- **Steps:** Settings sliders wired to `keyboard_set_backlight()` and the message store cap.

---

## Phase 3 — Messaging polish

### 3.1 — Message delivery status (ACK ticks) — M
- **Upstream ref:** MISSING_FEATURES → "Message delivery status (ACK display)".
- **Depends on Phase 0 cutover:** `BaseChatMesh` already provides correct `onAckRecv`/`processAck` with the `expected_ack_table[8]` — you only surface the state in the UI. (Pre-migration, `onAckRecv` is a no-op in `slop_mesh.h`, so do not attempt ACK ticks before cutover.)
- **Steps:** track per-DM state (pending/acked/failed) in the message store; match the 4-byte SHA-256 ACK to the pending message; show a tick in the bubble.
- **TRAP:** the ACK is SHA-256-derived, not CRC. See the trap list at the top.

### 3.2 — Message search — M
- **Upstream ref:** MISSING_FEATURES → "Message search". Pure UI.
- **Steps:** search icon in the chat top bar; substring filter over the per-channel cache; trackball navigation between matches.

### 3.3 — Per-contact RSSI/SNR history graph — L
- **Upstream ref:** MISSING_FEATURES → "Per-contact RSSI/SNR history graph". Pure UI.
- **Steps:** per-contact circular sample buffer; `lv_chart` sparkline on Signal/Contact Detail.
- **Trap:** cap the buffer (memory) and beware widget accumulation (rejection trigger).

---

## Phase 4 — Infrastructure interaction

> **Phase 0 cutover is a prerequisite for this entire phase.** Then build the request framework FIRST (4.1) — it unblocks 4.2–4.5.

### 4.1 — Generic binary-request framework (REQ/RESPONSE) — M  ★ keystone
- **Upstream ref:** MISSING_FEATURES → "Generic binary request framework" + "Direct request/response".
- **Steps:** expose `BaseChatMesh::sendRequest()` (both overloads) through the wrapper; add a tag→handler dispatch in `onContactResponse`.
- **Done when:** you can send an arbitrary REQ to a contact and route the tagged RESPONSE to a handler. Add native-test coverage for the parse/dispatch path in `test_mesh_messaging`.

### 4.2 — Status request — M  (depends on 4.1)
- **Upstream ref:** MISSING_FEATURES → "Status request". Send `REQ_TYPE_GET_STATUS`, parse the status blob (uptime/battery/airtime/queue), show on a node-status panel. Reference field layout in [`examples/simple_repeater/MyMesh.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/simple_repeater/MyMesh.cpp).

### 4.3 — Telemetry queries (remote + self, CayenneLPP) — M  (depends on 4.1)
- **Upstream ref:** MISSING_FEATURES → "Telemetry queries". Send `REQ_TYPE_GET_TELEMETRY_DATA`; decode CayenneLPP channels (voltage/temp/humidity/lat-lon); optionally answer inbound requests with our own battery.
- **Note:** CayenneLPP decode is a small amount of byte parsing — see [`src/helpers/sensors/`](https://github.com/meshcore-dev/MeshCore/tree/main/src/helpers/sensors).

### 4.4 — Path discovery request — M  (depends on 4.1)
- **Upstream ref:** MISSING_FEATURES → "Path discovery request". Distinct from Trace. Discover a route to a contact with no known path; store into `out_path`.

### 4.5 — Repeater/room login + remote administration — L
- **Upstream ref:** MISSING_FEATURES → "Repeater / room-server login + remote administration".
- **Steps:** expose `sendLogin()` + `sendCommandData()` + connection sessions through the wrapper; parse the login response permission byte.
- **UI:** "Login / Admin" on repeater/room contacts; password field; command console; show permission level.

### 4.6 — Room server message fetch — L  (depends on 4.1 + 4.5)
- **Upstream ref:** MISSING_FEATURES → "Room server message fetch". Detect `ADV_TYPE_ROOM` contacts (type already parsed); login; fetch stored posts; merge into the message store. Reference: [`examples/simple_room_server/MyMesh.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/simple_room_server/MyMesh.cpp).

### 4.7 — Anonymous requests (send) — M
- **Upstream ref:** MISSING_FEATURES → "Anonymous requests". We already *receive* them. Expose `BaseChatMesh::sendAnonReq()` through the wrapper. UI: "Message unknown node" / Terminal command.

### 4.8 — Group data datagrams — M
- **Upstream ref:** MISSING_FEATURES → "Group data datagrams". Add `sendGroupDatagram(channel, type_code, data, len)` + a received-datagram type dispatch (currently `onGroupDataRecv` renders everything as text).

### 4.9 — Multipart messages — L
- **Upstream ref:** MISSING_FEATURES → "Multipart messages". Per-sender PSRAM reassembly buffer; raise the 150-byte send cap in `sendTextTo`/`sendGroupText`. The library only has multi-ACK today — this is mostly new code in `SlopMesh`.

### 4.10 — Raw custom payloads — L
- **Upstream ref:** MISSING_FEATURES → "Raw custom payloads". `onRawDataRecv` is a stub (note: we already use `createRawData` internally for PING/PONG). Define an app dispatch + registration API. Lowest priority.

---

## Phase 5 — Identity, UI & security

### 5.1 — Contact locations on Map — M
- **Upstream ref:** MISSING_FEATURES → "Contact locations on Map screen". Coordinates already in `SlopContact` (`has_location`/`latitude`/`longitude`). Render labelled markers on the map canvas; tap → contact detail. Pure UI + map math.

### 5.2 — Factory reset — S
- **Upstream ref:** MISSING_FEATURES → "Factory reset". Settings action (double-confirm): clear NVS prefs + contacts + channels, regenerate identity, delay for flash, reboot.
- **Trap:** the `ESP.restart()` flash-write delay (see trap list).

### 5.3 — Identity backup (export/import) — M
- **Upstream ref:** MISSING_FEATURES → "Identity backup". Export the private key (hex/QR); import re-keys the node. **Hard part:** on import, every contact's shared secret must be recomputed (`calcSharedSecret`). See [`src/helpers/IdentityStore.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/IdentityStore.h).

### 5.4 — QR code generation + URI import — L / M
- **Upstream ref:** MISSING_FEATURES → "QR code generation", "QR code / URI import". Add a tiny MIT QR encoder (~2 KB; check GPL-3.0 compatibility — a rejection trigger if not). Post-migration, `BaseChatMesh::exportContact`/`importContact` produce the payloads. URI scheme: `meshcore://contact/add?...` / `meshcore://channel/add?...`.

### 5.5 — Node stats query — S
- **Upstream ref:** MISSING_FEATURES → "Node stats query". Surface the existing counters (`getNumSent*`, `getNumRecv*`, airtime) + drops on a diagnostics panel.

### 5.6 — Universal trackball back-swipe — M
- **Upstream ref:** MISSING_FEATURES → "Universal trackball back-swipe". Also KNOWN_ISSUES. Extract the swipe handler from `chat_screen.cpp` into `navigation.cpp`; apply to all screens; resolve conflicts with screens that use left-swipe themselves (two-swipe-commit pattern suggested).

### 5.7 — Device admin PIN — M
- **Upstream ref:** MISSING_FEATURES → "Device admin password / PIN". Optional hashed PIN in NVS; prompt on Settings/Terminal entry with a grace period.

### 5.8 — ACL / contact permissions — L
- **Upstream ref:** MISSING_FEATURES → "ACL / contact permissions". `perm` field on contacts; promote in contact detail; gate sensitive actions. Levels in [`src/helpers/ClientACL.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/ClientACL.h).

### 5.9 — Message signing — S (niche)
- **Upstream ref:** MISSING_FEATURES → "Message signing". Port the streaming sign API; Terminal command. Low priority for a handheld.

### 5.10 — OTA firmware update — L
- **Upstream ref:** MISSING_FEATURES → "OTA firmware update". OTA partition layout in `platformio.ini`; WiFi or BLE download (neither initialised today); UI progress. Transfer uses ESP-IDF `esp_ota_ops.h`. Biggest single item — do last.

---

## Suggested sequence

```
Phase 1 (quick wins) ──── ✅ COMPLETED
        │
        ▼
Phase 0  migrate to BaseChatMesh ──── ✅ COMPLETED
        │
        ├──────────────► Phase 4  keystone 4.1 → 4.2..4.10
        │                        │
Phase 2 (radio/config)           │
        │                        ▼
        └────────► Phase 3 (3.1 ACK — now unblocked)
                       │
                       ▼
                   Phase 5 (identity/UI/security/OTA)
```

- **Phase 1 ✅** and **Phase 0 ✅** are complete.
- **Phase 0 cutover unlocks Phase 4 and Phase 3.1 (ACK ticks).** Phase 4 keystone (4.1 generic binary-request framework) should be built first if starting Phase 4.
- **Phase 2 and most of Phase 5** are independent of Phase 0.
- **Within a phase, items are independent** unless a dependency is noted — do them as separate small PRs.

## Final reminders for the agent

- **One feature, one PR, one issue.** Don't bundle.
- **Add tests for every change.** A PR with no new/updated test and no green `pio test -e native_test` is rejected.
- **Declare hardware testing in the PR body** — "Remote test", "Physical hardware test", or both. Missing = auto-decline.
- **Don't hardcode colors or skip `apply_dark_bg()`** — theme compliance is enforced.
- **If you find a bug unrelated to your task**, add it to [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) in the standard format — don't silently fix or ignore it.
- **When in doubt about the Phase 0 migration, stop and ask the user.** Guessing on an embedded refactor is how regressions ship.
