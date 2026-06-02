# SigurdOS-TDeck Implementation Roadmap

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
1. Find or open a GitHub issue on hermes-gadget/SigurdOS-tdeck
2. git checkout dev && git pull origin dev
3. git checkout -b <type>/<short-name>-<issue#>
4. pio test -e native_test -v        # confirm baseline GREEN before you change anything
5. ... make changes + ADD TESTS ...
6. pio test -e native_test -v        # all green
7. pio run -e SigurdOS_TDeck           # firmware must build
8. commit (conventional message), push, open PR targeting dev
9. PR body MUST declare hardware testing: "Remote test", "Physical hardware test", or both
```
If you cannot run hardware, say so explicitly in the PR and use **remote test mode** where the feature allows it (see CLAUDE.md → Remote Test Controller). **Never** switch the build to `SigurdOS_TDeck_remote_test` or touch the radio config without the user's explicit say-so.

### The architecture you must respect
```
UI layer (src/ui/*)                ← screens, never touches MeshCore directly
      │  calls only
      ▼
mesh_wrapper.h  (sigurdos::mesh::*)   ← THE SEAM. Public API the UI depends on.
      │  wraps
      ▼
SigurdMeshV2  (src/mesh/sigurd_mesh_v2.h) ← BaseChatMesh subclass
      │  extends
      ▼
BaseChatMesh / ::mesh::Mesh  (lib/meshcore/) ← upstream library
```
**Golden rule: keep the `mesh_wrapper.h` public API stable.** The UI is insulated by it. If you change mesh internals, translate back to the existing wrapper structs (`sigurdos::mesh::ContactInfo`, `MeshMessage`, etc.) so screens don't change. This seam is what makes large refactors (see Phase 0) survivable.

### Traps that have already bitten this codebase (do not repeat)
- **ACK value is NOT a CRC-32.** It is the first 4 bytes of SHA-256 over a recipient-pubkey-dependent buffer. Computing a CRC will *never* match. (`MISSING_FEATURES.md` → "Message delivery status".)
- **Channel hash matching uses ONE byte** in the packet header, not the full hash. `searchChannelsByHash` compares `hash[0]` only — matching upstream `BaseChatMesh`. ~11% collision on 8 channels is expected; do not "fix" it to a full memcmp or you break interop.
- **`strncpy` does not null-terminate** when the source is too long. Always `dest[n-1] = '\0'`.
- **UTF-8 truncation** can split a 4-byte emoji mid-codepoint → invalid UTF-8 over the mesh. Use `sigurdos::utf8_truncate_bytes()` (already used in `sigurd_mesh_v2.h`).
- **`lv_obj_del` inside an event handler** must be `lv_obj_del_async()`.
- **`lv_scr_load_anim(..., true)`** deletes the old screen + all children; register `LV_EVENT_DELETE` to null any globals pointing into it.
- **Stack arrays as LVGL `user_data`** dangle on click. Use `static` arrays.
- **Adding a `NodePrefs` field**: old saved prefs won't have it. `prefs_get()` zero-fills missing keys — follow the existing default-value pattern in `NodePrefs::set_defaults()` (`src/hal/prefs.h`).
- **`ESP.restart()`** before a flash write completes loses the write. `saveState()`/`saveChannels()` then delay ~100 ms before restart.
- **Debug output** must be guarded by `#if defined(SIGURDOS_DEBUG)` — unconditional `Serial.printf` is a rejection trigger.

### Where things live (verified paths)
| Concern | File |
|---------|------|
| Mesh subclass | `src/mesh/sigurd_mesh_v2.h` |
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

## Phase 0 — Migrate mesh core onto `BaseChatMesh` ✅ COMPLETED

**Status: ✅ Done.** PR #223 introduced the V2 implementation behind a flag; PR #224 cut over to `SigurdMeshV2` as the default. The old pre-BaseChatMesh mesh implementation path has been removed; current mesh internals live in `src/mesh/sigurd_mesh_v2.h` and are constructed through `mesh_wrapper.cpp`.

**This architectural direction is complete and is now the baseline.** Current roadmap items should use the `mesh_wrapper.h` seam and `SigurdMeshV2`/`BaseChatMesh` APIs rather than reimplementing raw MeshCore protocol plumbing.

### Why we did it

The previous mesh class extended `::mesh::Mesh` directly — a deliberately minimal subclass. One layer up in the *same* library sits `BaseChatMesh` ([`lib/meshcore/src/helpers/BaseChatMesh.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/BaseChatMesh.h)), which the reference companion radio uses. It already implements, correctly and tested:

- `sendMessage()` **with ACK tracking** (`expected_ack`), `processAck()`
- `sendLogin()`, `sendCommandData()` (remote repeater/room admin), connection keep-alive sessions
- `sendRequest()` x2 (the REQ/RESPONSE framework), `sendAnonReq()`, `sendGroupData()`
- `resetPathTo()`, `removeContact()`, `addContact()`, `lookupContactByPubKey()`
- `exportContact()` / `importContact()` (QR/URI sharing payloads)
- correct `onAckRecv`, path learning, send-timeout handling

**Almost the entire Phase 4 is already written inside `BaseChatMesh`.** Building those on raw `Mesh` would mean reimplementing them by hand — repeatedly hitting the traps listed above. Inheriting `BaseChatMesh` gets them for free.

> **This was NOT a "MeshCore version" change.** SigurdOS stayed on the current MeshCore submodule; `SigurdMeshV2` moved the project up one layer to `BaseChatMesh` without a submodule bump.

### Why the blast radius is small

The blast radius stayed concentrated in **one place**: `SigurdMeshV2` adopts `BaseChatMesh`'s `ContactInfo` and `ChannelDetails` models and `mesh_wrapper.cpp` translates those into the wrapper structs used by the UI. The UI only sees `sigurdos::mesh::ContactInfo`, `MeshMessage`, etc., so screens remain insulated from MeshCore internals.

### Completed migration checklist

The migration is complete; this checklist is retained as historical context and regression guidance. Current work should not recreate a parallel mesh class.

1. `SigurdMeshV2 : public BaseChatMesh` is the production mesh implementation.
2. `mesh_wrapper.cpp` constructs `SigurdMeshV2` and keeps the UI-facing API stable.
3. Contact/channel persistence, identity persistence, ACK handling, requests/responses, path learning, command data, signed messages, timeouts, and contact request/response hooks are implemented through the BaseChatMesh path.
4. **Regression checklist** (native tests + hardware/remote-test):
   - [x] DM send + receive (`test_mesh_messaging`)
   - [x] Channel text send + receive (hashtag + PSK channels)
   - [x] Advert parse → contact added with name/type/location/RSSI/SNR
   - [x] Contact LRU eviction at the cap
   - [x] Trace route round-trip
   - [x] Ping Nearby (control PING/PONG)
   - [x] Duty-cycle factor override still applied
   - [x] Channel persistence across reboot
6. The legacy mesh class was removed; `SigurdMeshV2` is the only active mesh subclass.

> **Sequencing note:** Phase 0 is no longer blocking anything; Phases 1–6 below are complete for accepted scope.

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

### 2.1 — Periodic auto-advert — S ✅
- **Status:** Done. *(Audit 2026-06-01 reclassified this from "declined" — the feature is in fact wired.)* `advert_interval` (half-minutes, `0` = disabled) in `NodePrefs`, a Settings cycle in `screens.cpp`, persisted in `prefs.cpp`, and a re-advert timer in `mesh::loop()` (`mesh_wrapper.cpp`) that calls `sendAdvert()` on the interval. Defaults to `0` (off).
- **Upstream ref:** MISSING_FEATURES → "Periodic auto-advert" (`NodePrefs::advert_interval`).

### 2.2 — Temporary radio config (no NVS write) — ❌ NOT NEEDED
> User declined — not implementing.

### 2.3 — Auto-add contact configuration — M ✅
- **Status:** Done. Per-type bitmask (`autoadd_config`) in NodePrefs gates auto-add by contact type (chat/repeater/room/sensor via bits 1-4). Separate `autoadd_max_hops` for range limiting. Settings: "Auto-add: All types" cycle + "Add max hops" cycle.
- **PR:** #228.

### 2.4 — Advert location-share policy (privacy) — S ✅
- **Status:** Already done. `share_location` toggle in Settings (set_defaults: true). Wireframe already in prefs for lat/lon gating.

### 2.5 — GPS enable / read-interval — S ✅
- **Status:** Done. `gps_enabled` (bool) + `gps_interval` (uint16_t seconds) in NodePrefs. `sigurdos_gps_init()`/`sigurdos_gps_loop()` gated on the pref. Settings: GPS ON/OFF toggle + interval cycle (0/1/5/10/30/60s).
- **PR:** #227.

### 2.6 — Custom variables (key-value store) — S ✅
- **Status:** Done. SPIFFS-backed `key=value` store. Terminal commands: `setvar`, `getvar`, `delvar`, `listvars`.
- **PR:** #229.

### 2.7 — Keyboard backlight & message-cap controls — S each ✅
- **Status:** Already done. Backlight dialog (+/- slider) and chat history cap dialog in Settings, both fully wired.

---

## Phase 3 — Messaging polish ✅ COMPLETED

> **Status: ✅ All 3 items done.** See overview below for the specific PRs.

### 3.1 — Message delivery status (ACK ticks) — M ✅
- **Status:** Done. PendingAck ring buffer in SigurdMeshV2, `processAck()` matches 4-byte SHA-256 ACK against pending outgoing DMs. `isMessageAcked()` bridge to UI. ✓ indicator in self-sent chat bubbles.
- **PR:** #232.

### 3.2 — Message search — M ✅
- **Status:** Done. 'S' button in top bar toggles inline search bar. Case-insensitive substring filter over message text and sender. Trackball Up/Down cycles through matches with auto-scroll and highlight. "No matching messages" when empty.
- **PR:** #234.

### 3.3 — Per-contact RSSI/SNR history graph — L ✅
- **Status:** Done. 64-entry circular buffer per-contact in SigurdMeshV2, `lv_chart` line sparkline on Signal screen showing RSSI trend.
- **PR:** #236.

---

## Phase 4 — Infrastructure interaction ✅ COMPLETED

> **Status: ✅ Items 4.1–4.8 done (4.3 has an outstanding answer-side — see its note and 6.4). Items 4.9–4.10 declined — not implementing.**
> Phase 0 cutover was the prerequisite.

### 4.1 — Generic binary-request framework (REQ/RESPONSE) — M ✅
- **Status:** Done. `BaseChatMesh::sendRequest()` exposed through wrapper with tag→handler dispatch.
- **PR:** #251.

### 4.2 — Status request — M ✅
- **Status:** Done. `REQ_TYPE_GET_STATUS` request with NodeStatus UI panel.
- **PR:** #253.

### 4.3 — Telemetry queries (remote + self, CayenneLPP) — M ✅ ⚠️ answer-side outstanding
- **Status:** Remote query done. Send `REQ_TYPE_GET_TELEMETRY_DATA` and decode the CayenneLPP response (voltage, temp, humidity, lat-lon) — `requestTelemetry()` in `mesh_wrapper.cpp`. **Answering inbound telemetry is NOT implemented:** `SigurdMeshV2::onContactRequest()` (`sigurd_mesh_v2.h`) is a skeleton that returns `0`, so the T-Deck never replies with its own battery/telemetry when queried. *(Audit 2026-06-01 found the "answer inbound requests with own battery" claim inaccurate.)* The answer side + per-type policy is tracked as **6.4** below.
- **PRs:** #2907746, #0ee06ea.

### 4.4 — Path discovery request — M ✅
- **Status:** Done. Distinct from Trace — discovers route to a contact with unknown path.
- **PRs:** #944bded, #ae68b3d.

### 4.5 — Repeater/room login + remote administration — L ✅
- **Status:** Done. Dedicated repeater detail screen with login flow, password field, admin command terminal with live response polling.
- **PR:** #259.

### 4.6 — Room server message fetch — L ✅
- **Status:** Done. Login to room server, fetch stored posts, merge into message store.
- **PR:** #263.

### 4.7 — Anonymous requests (send) — M ✅
- **Status:** Done. Expose `BaseChatMesh::sendAnonReq()` through wrapper with UI.
- **PR:** #260.

### 4.8 — Group data datagrams — M ✅
- **Status:** Done. `sendGroupDatagram(channel, type_code, data, len)` + received-datagram type dispatch.
- **PR:** #265.

### 4.9 — Multipart messages — L ❌ NOT DOING
- **Reason:** User declined — not implementing.

### 4.10 — Raw custom payloads — L ❌ NOT DOING
- **Reason:** User declined — not implementing.

---

## Phase 5 — Identity, UI & security

### 5.1 — Contact locations on Map — M ✅
- **Status:** Done. Map overlays contact-location markers from `ContactInfo::has_location` / `latitude` / `longitude`; marker taps open Contact Detail.
- **PR:** #321 (Closes #302).

### 5.2 — Factory reset — S ✅
- **Status:** Done. "Factory reset" action on the Settings → System screen (double-confirm) → `mesh::factoryReset()` clears NVS prefs + contacts + channels, regenerates identity, delays for flash, then reboots.
- **PR:** #275 (issue #274).

### 5.3 — Identity backup (export/import) — M ✅
- **Status:** Done. Terminal `exportkey` / `importkey <hex>` commands back up and restore the node identity.
- **PR:** #320 (Closes #303).

### 5.4 — QR code generation + URI import — L / M ✅
- **Status:** Done. Terminal `import meshcore://...` handles contact/channel URI import; Contact Detail and Channel Settings can render shareable QR codes via `src/app/qr_show.cpp`.
- **PRs:** #322 (URI import), #328 (QR generation).

### 5.5 — Node stats query — S ✅
- **Status:** Done. Node Stats diagnostics panel surfacing sent/recv flood+direct counters, airtime totals, and duplicate/drop counts. Reachable from navigation (`nodestats` nav entry).
- **PR:** #277 (issue #276).

### 5.6 — Universal trackball back-swipe — M ✅
- **Status:** Done. `navigation.cpp` implements the two-swipe-commit back gesture for non-Home/non-Chat screens; Chat keeps its channel-list left-swipe behaviour.

### 5.7 — Device admin PIN — M ✅
- **Status:** Done. `NodePrefs::device_pin` persists a 4–6 digit PIN; Settings → System exposes set/change; Settings/Terminal entry prompts when enabled.
- **PR:** #326.

### 5.8 — ACL / contact permissions — L ✅
- **Status:** Done. `ContactInfo::perm` plus `setContactPerm()` / `getContactPerm()` wrappers; Contact Detail displays ACL role and provides promote/demote controls.
- **Issue:** #310.

### 5.9 — Message signing — S (niche) ✅
- **Status:** Done. Terminal `sign <data>` signs arbitrary text with the node identity.
- **PR:** #317 (Closes #307).

### 5.10 — OTA firmware update — L ✅ DONE

- **Implemented:** Two OTA paths — WiFi AP upload (`screens.cpp` "OTA Update") and GitHub download via STA WiFi (`github_ota.h`). Dual OTA partition table active. See MISSING_FEATURES → "OTA firmware update".

---

## Phase 6 — Companion parity gaps (audit 2026-06-01)

> **Status: complete.** The companion-firmware deltas found in the 2026-06-01 audit have been implemented or explicitly declined. Infrastructure-only deltas (region / flood-scope routing, allowed-repeat-freq, path-hash-mode, BLE-modem) remain documented for reference but outside the standalone handheld scope.

### 6.1 — Multi-ACK reliability toggle — S ✅
- **Status:** Done. `NodePrefs::multi_acks`, Settings toggle, and `SigurdMeshV2::getExtraAckTransmitCount()` are wired.

### 6.2 — Message-arrival notification (buzzer) + quiet toggle — M ✅
- **Status:** Done. Buzzer HAL, incoming-message beep, `buzzer_quiet` pref, and Settings "Notification sound" toggle are implemented.

### 6.3 — Client-repeat mode (companion also relays) — M ✅
- **Status:** Done. `NodePrefs::client_repeat`, Settings toggle, and `SigurdMeshV2` forwarding gate are implemented.
- **PR:** #319 (issue #306).

### 6.4 — Answer inbound telemetry + telemetry-mode policy — M ✅
- **Status:** Done. `SigurdMeshV2::onContactRequest()` answers `REQ_TYPE_GET_TELEMETRY_DATA` with CayenneLPP battery/GPS data where available.
- **PR:** #318 (issue #301).

### 6.5 — Advert path query (diagnostic) — S *(niche)* ✅
- **Status:** Done. `SigurdMeshV2` tracks inbound advert paths and Contact Detail displays path/hop count.
- **PRs:** #316 (hop count), #324 (advert path).

### 6.6 — Storage usage display — S *(niche)* ✅
- **Status:** Done. Settings → System shows internal/SD storage usage where available.

### 6.7 — Reboot action — S *(low value)* ✅
- **Status:** Done. Settings → System includes a dedicated Reboot action with confirmation.

---

## Suggested sequence

```
| Phase 1 (quick wins) ──── ✅ COMPLETED
|        │
|        ▼
| Phase 0  migrate to BaseChatMesh ──── ✅ COMPLETED
|        │
|        ├──────────────► Phase 4 (4.1–4.8) ──── ✅ COMPLETED
|        │                        │            (4.9–4.10 declined)
| Phase 2 (radio/config) ── ✅ COMPLETED
|        │                        │
|        └────────► Phase 3 (messaging polish) ── ✅ COMPLETED
|                               │
|                               ▼
|                           Phase 5 (identity/UI/security/OTA) ── ✅ COMPLETED
|                               │
|                               ▼
|                           Phase 6 (companion parity gaps) ───── ✅ COMPLETED
```

- **Phases 0–6 ✅** are complete for the currently accepted companion-handheld scope.
- Declined/out-of-scope items remain documented in `MISSING_FEATURES.md` for reference, but are not active roadmap work.

## Final reminders for the agent

- **One feature, one PR, one issue.** Don't bundle.
- **Add tests for every change.** A PR with no new/updated test and no green `pio test -e native_test` is rejected.
- **Declare hardware testing in the PR body** — "Remote test", "Physical hardware test", or both. Missing = auto-decline.
- **Don't hardcode colors or skip `apply_dark_bg()`** — theme compliance is enforced.
- **If you find a bug unrelated to your task**, add it to [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) in the standard format — don't silently fix or ignore it.
- **When in doubt about the Phase 0 migration, stop and ask the user.** Guessing on an embedded refactor is how regressions ship.
