# Missing Features

This document catalogs features present in the MeshCore protocol and ecosystem that are not yet implemented in SlopOS-TDeck firmware. It is intended as a roadmap reference — not a bug tracker. Bugs and workarounds belong in `KNOWN_ISSUES.md`.

SlopOS-TDeck is a standalone **companion-radio firmware** for the LilyGo T-Deck. It interoperates with any MeshCore node but is designed for the end-user handheld experience — not for infrastructure roles (dedicated repeaters, room servers, sensors).

Features in this document are tagged to distinguish companion-relevant from infrastructure-only items. The implementation plan (§ below) only covers **companion features**. Truly infrastructure-only items (BLE modem mode, region management, launcher compatibility) are documented for reference but are not planned.

**May 2026 update:** This document has been reviewed against the MeshCore companion protocol (`lib/meshcore/examples/companion_radio/MyMesh.cpp`, 58 CMD_* / 29 RESP_* codes). Most Protocol/Packet Type items below are library-level features that the companion protocol has already solved at the CMD level — if SlopOS were to implement them, the companion source is the reference, not the raw MeshCore library. Items marked *(companion CMD exists)* have a direct companion protocol implementation to use as a reference pattern.

All MeshCore file paths below are relative to the root of `https://github.com/meshcore-dev/MeshCore` (main branch). The submodule in this repo is pinned to a specific commit — if a symbol can't be found, check `lib/meshcore/` directly.

---

## How to use this document

Each entry describes what the feature is, what MeshCore provides, and what would be needed to implement it. Entries are grouped by category and marked with a rough effort level:

- **S** — small: isolated change, few files, testable in native tests
- **M** — medium: touches mesh layer + UI, needs device testing
- **L** — large: architectural change, multiple screens or protocol work

---

## Protocol / Packet Types

### Multipart messages (PAYLOAD_TYPE_MULTIPART 0x0A) — L

MeshCore defines a multipart packet type for segmenting large payloads across multiple LoRa frames. SlopOS currently caps all outgoing messages at 150 bytes of text. Messages longer than 150 characters are silently truncated before transmission.

The library only implements one multipart subtype today — multi-ACK. There is no general reassembly buffer for arbitrary payload types. That work would need to live in SlopOS on top of `onGroupDataRecv` or `onRawDataRecv`.

**What's needed:** Implement multipart send/receive in `SlopMesh`. Increase the message input limit in the chat screen send path. Add a reassembly buffer (PSRAM) per sender.

**MeshCore reference:**
- `src/Packet.h` — `#define PAYLOAD_TYPE_MULTIPART 0x0A`
- `src/Mesh.cpp` — `case PAYLOAD_TYPE_MULTIPART:` dispatch; parses `remaining` (high nibble) and embedded subtype (low nibble)
- `src/Mesh.h` — `Mesh::createMultiAck(uint32_t ack_crc, uint8_t remaining)` — the only multipart factory currently in the library

---

### Group data datagrams (PAYLOAD_TYPE_GRP_DATA 0x06) — M

MeshCore defines typed binary group datagrams with a 16-bit type namespace for application use. `onGroupDataRecv` in `src/mesh/slop_mesh.h` receives both `GRP_TXT` and `GRP_DATA` packets — binary datagrams are rendered as hex in the chat view. There is no API in `mesh_wrapper.h` for **sending** a binary datagram, and received binary data has no type-code dispatch (all `GRP_DATA` packets are treated as generic hex strings rather than routed to app-specific handlers).

This opens up extensible group-channel applications: shared state sync, map tile requests, sensor broadcasts, etc.

**What's needed:** Add `sendGroupDatagram(channel, type_code, data, len)` to the wrapper API. Add a callback or dispatch mechanism for received datagrams. Design a registry of type codes for SlopOS app use.

**MeshCore reference:**
- `src/Packet.h` — `#define PAYLOAD_TYPE_GRP_DATA 0x06`
- `src/Mesh.h` — `Mesh::createGroupDatagram()` (send), `virtual void onGroupDataRecv(Packet*, uint8_t type, const GroupChannel&, uint8_t* data, size_t len)` (receive callback)
- `src/helpers/TxtDataHelpers.h` — `DATA_TYPE_RESERVED` (0x0000) and `DATA_TYPE_DEV` (0xFFFF) as reserved type code boundaries

---

### Anonymous requests (PAYLOAD_TYPE_ANON_REQ 0x07) — M *(companion CMD exists)*

SlopOS handles incoming anonymous requests in `onAnonDataRecv` (displays as `anon_XX` sender) but has no way to send one. Anonymous requests allow a node to initiate contact with another node without a prior advert exchange — the sender's public key is embedded in the packet rather than looked up from a contact.

**What's needed:** Add `sendAnonMessage(pubkey_hex, text)` to the wrapper API. Wire a UI entry point (e.g. a "Message unknown node" option in Contacts or a Terminal command).

**MeshCore reference:**
- `src/Packet.h` — `#define PAYLOAD_TYPE_ANON_REQ 0x07`
- `src/Mesh.h` — `Mesh::createAnonDatagram()` (send), `virtual void onAnonDataRecv(Packet*, const uint8_t* secret, const Identity& sender, uint8_t* data, size_t len)` (receive callback)
- `examples/simple_room_server/MyMesh.cpp` — example usage: room server uses anonymous requests for status/telemetry queries

---

### Direct request/response (PAYLOAD_TYPE_REQ 0x00 / PAYLOAD_TYPE_RESPONSE 0x01) — M *(companion CMD exists)*

The Core Protocol defines direct-encrypted REQ and RESPONSE payloads for request/response exchanges between any two nodes. These are not yet used in SlopOS. They enable several companion features:
- **Room server message fetch**: send a REQ to a room server to retrieve stored messages, parse the RESPONSE
- **Path discovery**: send a REQ asking a node for its known path to another node
- **Capability query**: ask a node what features/protocols it supports

`onRecv` in `src/mesh/slop_mesh.h` dispatches these payload types but the response handlers are stubs.

**What's needed:** Map REQ type codes (application-defined, see `REQ_TYPE_*` constants in `examples/simple_room_server/`). Add a `sendRequest(contact, req_type, data)` wrapper API. Wire response dispatch to application callbacks (message store, path registry, etc.).

**Core Protocol spec reference:**
- `§2.9` — Direct-encrypted payloads: REQ (0x00) and RESPONSE (0x01) share the same wire format as TEXT and PATH payloads (destination hash + encrypted data)
- `§2.10` — Anonymous request (separate, listed above)
- `examples/simple_room_server/MyMesh.h` — `REQ_TYPE_GET_STATUS`, `REQ_TYPE_GET_TELEMETRY_DATA`, `REQ_TYPE_KEEP_ALIVE`

---

### Control packets (PAYLOAD_TYPE_CONTROL 0x0B) — L

`onControlDataRecv` in `src/mesh/slop_mesh.h` implements PING/PONG — when a zero-hop control ping is received, SlopOS responds with a PONG advert. The Finder screen's \"Ping Nearby\" button (`sendPingNearby()` in `mesh_wrapper.h`) sends a zero-hop control ping and collects responses. However, no other control packet sub-types are handled (capability queries, sub-mesh management).

The library dispatches control packets only for direct-routed, previously-unseen packets where the first payload byte has bit 7 set (`payload[0] & 0x80`) and the hop count is zero. No named subtype constants are defined — the application layer defines its own.

**What's needed:** Design which control sub-types SlopOS should respond to (neighbor hello, capability query). Implement handlers. The zero-hop ping (see Finder section below) depends on this.

**MeshCore reference:**
- `src/Packet.h` — `#define PAYLOAD_TYPE_CONTROL 0x0B`
- `src/Mesh.h` — `Mesh::createControlData(const uint8_t* data, size_t len)` (send), `virtual void onControlDataRecv(Packet*)` (receive callback)
- `src/Mesh.cpp` — dispatch logic: `case PAYLOAD_TYPE_CONTROL:` (only fires for zero-hop direct packets with `payload[0] & 0x80`)

---

### Raw custom payloads (PAYLOAD_TYPE_RAW_CUSTOM 0x0F) — L *(companion CMD exists)*

`onRawDataRecv` in `src/mesh/slop_mesh.h` is a stub. This payload type allows unstructured encrypted byte strings over the mesh — the building block for custom apps on top of MeshCore without needing to fit the group or peer message formats.

**What's needed:** Define an application dispatch interface. Expose a registration API for future SlopOS app extensions.

**MeshCore reference:**
- `src/Packet.h` — `#define PAYLOAD_TYPE_RAW_CUSTOM 0x0F`
- `src/Mesh.h` — `Mesh::createRawData(const uint8_t* data, size_t len)` (send), `virtual void onRawDataRecv(Packet*)` (receive callback)
- `src/Mesh.cpp` — `case PAYLOAD_TYPE_RAW_CUSTOM:` fires only for direct-routed, previously-unseen packets

---

## Radio Configuration

### RX gain boost toggle — S

The SX1262 hardware supports a boosted RX sensitivity mode. This is not exposed anywhere in SlopOS — not in Settings, Radio Setup, or the Terminal.

**What's needed:** Add an `rxgain` boolean to `NodePrefs`. Expose a toggle in the Radio Setup screen. Apply the setting via the radio wrapper's `setRxBoostedGainMode()` during radio init in `src/mesh/mesh_wrapper.cpp`.

**MeshCore reference:**
- `src/helpers/radiolib/RadioLibWrappers.h` — `RadioLibWrapper::setRxBoostedGainMode(bool en)` (default no-op virtual)
- `src/helpers/radiolib/CustomSX1262Wrapper.h` — concrete override forwarding to RadioLib's `SX1262::setRxBoostedGainMode()`; same pattern in `CustomSX1268Wrapper.h`
- `src/helpers/CommonCLI.h` — `NodePrefs::rx_boosted_gain` (uint8_t) — the persisted setting; `CommonCLICallbacks::setRxBoostedGain(bool)` virtual method wired to the CLI

---

### Temporary radio config (no reboot required) — M

MeshCore companion CLI supports `tempradio freq,bw,sf,cr,timeout_mins` — a trial radio config that reverts automatically on reboot without writing to NVS. SlopOS requires a full `Save & Reboot` cycle to try any new radio parameter.

This is useful for field-testing alternative configs without losing the previous working setup.

**What's needed:** A "Try" button in Radio Setup that applies parameters live via RadioLib without writing to NVS. A timer-based or manual "Revert" that restores the last saved config.

**MeshCore reference:**
- `src/helpers/CommonCLI.h` — `handleCommand()` handles the `tempradio` command; `NodePrefs` stores a `temp_radio_timeout` field alongside the temp freq/sf/bw/cr values
- `src/helpers/radiolib/RadioLibWrappers.h` — `RadioLibWrapper::setParams(freq, bw, sf, cr)` — the live-apply method to call without persisting

---

### Duty cycle enforcement — M

EU ISM 868 MHz regulations limit airtime to 1% per hour on most sub-bands. MeshCore's `Dispatcher` tracks airtime and enforces a configurable duty cycle budget. SlopOS does not expose or display this setting, and does not show how much airtime budget remains.

**What's needed:** Surface the MeshCore duty cycle tracking in the Signal or Settings screen. Add a configurable limit to `NodePrefs`. Display remaining hourly airtime budget.

**MeshCore reference:**
- `src/Dispatcher.h` — `class mesh::Dispatcher`; key fields: `tx_budget_ms`, `duty_cycle_window_ms` (default 3 600 000 ms), `total_air_time`, `rx_air_time`
- `src/Dispatcher.h` — `virtual float getAirtimeBudgetFactor() const` — override to set TX fraction (e.g. `0.01` = 1%); `getRemainingTxBudget()` public accessor
- `src/helpers/CommonCLI.h` — `NodePrefs::duty_cycle` — persisted user setting fed into `getAirtimeBudgetFactor()`

---



## GPS and Location




### Periodic auto-advert — S

Companion nodes can broadcast periodic adverts so they are discoverable without user action. SlopOS currently only sends an advert when the user manually taps the Advertise screen. Nodes powered on for hours without manually advertising are invisible to new nodes. This is optional companion behaviour — the user can toggle it on/off.

**MeshCore reference:**
- `src/helpers/CommonCLI.h` — `NodePrefs::advert_interval` (uint8_t, in minutes/2; value 10 = every 20 min), `NodePrefs::flood_advert_interval` (uint8_t, in hours)
- `src/helpers/CommonCLI.h` — `CommonCLICallbacks::updateAdvertTimer()` and `updateFloodAdvertTimer()` — virtual methods called when these settings change; the concrete firmware implements the actual timer scheduling

---


## Contacts and Discovery


### QR code generation for contact sharing — L

MeshCore defines a URI scheme for sharing contacts (`meshcore://contact/add?name=<name>&public_key=<64hex>&type=<1-4>`) and channels (`meshcore://channel/add?name=<name>&secret=<hex32>`). SlopOS has no QR code generation or display.

The T-Deck's 320×240 display is large enough to show a QR code. A small software QR encoder (e.g. QRCode by Richard Moore, MIT license, ~2 KB) could be added without significant flash cost.

**What's needed:** Add a QR library. Add a "Share" button to the Contact Detail screen and Channels screen. Render the QR code in a full-screen LVGL canvas.

**MeshCore reference:**
- The URI format is documented in the MeshCore README (`meshcore://` deep-link scheme). No dedicated source file defines it — it is produced by the companion radio's `EXPORT_CONTACT` response handler in `examples/companion_radio/MyMesh.cpp`.

---

### QR code / URI import for contacts and channels — M

The inverse of the above: a user on another device shows a `meshcore://` URI. SlopOS has no camera but could accept the URI via keyboard input in a "Add by URI" dialog or Terminal command.

**What's needed:** A `meshcore://contact/add?...` parser. A Terminal command: `add contact meshcore://...`. Optionally a dedicated "Add Contact" screen with a URI text field.

**MeshCore reference:**
- `examples/companion_radio/MyMesh.cpp` — `IMPORT_CONTACT` command handler: shows the binary format the URI encodes (name + 32-byte pub key + type byte) and how it is written into the contact store.

---


## Messaging

### Message delivery status (ACK display) — M

`onAckRecv` in `src/mesh/slop_mesh.h:285` is a stub. When a DM is sent, MeshCore may receive an ACK back from the destination. The chat screen has no sent/pending/delivered/failed state — all sent messages look the same regardless of acknowledgement.

**What's needed:** Track outgoing DM state (pending / acked / failed) in the message store. Display a status indicator in chat bubbles (single tick for sent, double tick for acked). Hook `onAckRecv` to update state by matching the ACK CRC to the pending message.

**MeshCore reference:**
- `src/Mesh.h` — `virtual void onAckRecv(Packet*, uint32_t ack_crc)` — the callback to override
- `src/Mesh.cpp` — `case PAYLOAD_TYPE_ACK:` dispatch
- `src/helpers/BaseChatMesh.h` / `src/helpers/BaseChatMesh.cpp` — `BaseChatMesh::onAckRecv()` → `processAck()`: the reference implementation that matches `ack_crc` to a pending-message table, clears `txt_send_timeout`, and calls `packet->markDoNotRetransmit()`

---

### Room server message fetch — L

A companion node can request stored messages from a room server over the mesh using the REQ/RESPONSE payload types. Companion nodes are not always in range of the room server and may want to synchronise missed messages on reconnection.

**What's needed:** Detect room server contacts by their `ADV_TYPE_ROOM` advert type (needs "Contact node type" feature above). Add a "Fetch from room server" action to the Contacts screen or a dedicated room interaction screen. Implement the REQ/RESPONSE payload exchange (using `PAYLOAD_TYPE_REQ` 0x00 and `PAYLOAD_TYPE_RESPONSE` 0x01 — direct-encrypted request/response packets between any two nodes). Parse the response and merge received messages into the local message store.

**MeshCore reference:**
- `examples/simple_room_server/MyMesh.h` + `MyMesh.cpp` — full room server implementation: `PostInfo` struct, cyclic post queue (`MAX_UNSYNCED_POSTS` = 32), `pushPostToClient()` — how stored messages are encoded and sent back to a client via encrypted datagrams
- Request type constants live in the same file: `REQ_TYPE_GET_STATUS`, `REQ_TYPE_GET_TELEMETRY_DATA`, `REQ_TYPE_KEEP_ALIVE`

---


### Message timestamps in chat bubbles — S

Chat message bubbles display sender name and text but no timestamp. The `MeshMessage` struct already stores `timestamp` from the packet header. Users have no way to tell when a message was sent.

**What's needed:** Render a small timestamp below each message bubble (e.g. `14:32`), formatted using `getCurrentLocalDateTime()`. For messages older than 24 hours, show the date.

*(No MeshCore source reference — the timestamp is already in `MeshMessage::timestamp`; this is UI-only work.)*

---

### Channel removal — S

Once a channel is added, there is no way to remove it. The channel list fills up (max 8) with no way to evict old channels.

**What's needed:** A swipe-to-delete or long-press-to-remove gesture on channel list items. A `removeChannel(idx)` function in the mesh wrapper that shifts the channel array and persists the change.

*(No MeshCore source reference — purely local work; `SlopMesh::_channels` array management.)*

---

## Settings and Configuration

### Keyboard backlight control — S

`NodePrefs` stores a `kb_backlight` byte (0–255). The keyboard MCU accepts I2C brightness commands. However, there is no UI control to change it.

**What's needed:** A brightness slider in Settings. Wire it to `keyboard_set_backlight()` and `prefs_set()`.

*(No MeshCore source reference — T-Deck-specific HAL feature.)*

---

### Message history cap control — S

`NodePrefs` stores a `msg_cap` field controlling how many messages to retain per channel. There is no UI to change this.

**What's needed:** A numeric input in Settings for "Message history per channel." Validate against available PSRAM.

*(No MeshCore source reference — purely local.)*

---



### Node type selection — M

Companion nodes advertise as `ADV_TYPE_CHAT` by default, but the firmware could allow selecting other types. For example, a dedicated T-Deck in a fixed location might want to advertise as a repeater or room server. The advert type is a configuration choice.

Note: selecting a non-CHAT advert type changes packet forwarding behaviour — companion nodes do not relay packets by default. This is a significant protocol change.

**What's needed:** An "Advanced" section in Settings with a node type selector. For repeater mode: enable the MeshCore relay path and increase advert frequency.

**MeshCore reference:**
- `src/helpers/AdvertDataHelpers.h` — `ADV_TYPE_CHAT` (1), `ADV_TYPE_REPEATER` (2), `ADV_TYPE_ROOM` (3), `ADV_TYPE_SENSOR` (4); `AdvertDataBuilder` constructor takes the type as first argument
- `src/helpers/CommonCLI.h` — `NodePrefs::advert_type` (uint8_t) — persisted node type; `CommonCLICallbacks` virtual methods for enabling/disabling repeat mode

---

## Diagnostics and Statistics




### Terminal command documentation (help system) — S

Also tracked in `KNOWN_ISSUES.md`. The Terminal screen has no `help` command. Users must read the source to discover commands.

**What's needed:** A `help` command that lists all available commands with one-line descriptions. See `KNOWN_ISSUES.md` for the full command list.

*(No MeshCore source reference — purely local UI work.)*

---

## User Interface

### Contact locations on Map screen — M

The Map screen shows offline tile cartography and the device's own GPS position, but does not show other nodes' positions. When contacts broadcast GPS coordinates in their adverts, those locations could be displayed as labeled markers on the map.

Depends on first implementing "Contact locations not parsed from adverts" above.

**What's needed:** Render labeled contact markers on the map canvas after contact location parsing is implemented. Markers should update on new adverts. Tapping a marker opens the contact detail screen.

*(No additional MeshCore reference beyond the advert parsing entry above.)*

---


### Zero-hop ping in Finder screen — M

Also tracked in KNOWN_ISSUES.md. The Finder screen lists contacts by RSSI but there is no active probe. Nodes that have not recently advertised are invisible even if they are in range.

What's needed: A Ping Nearby button that sends a control packet with TTL=1. A 2-3 second collection window. Display responses sorted by RSSI. 30-second cooldown.

Note: the underlying control packet PING/PONG is already implemented in the mesh layer and sendPingNearby() exists in mesh_wrapper.h. What's missing is the Finder screen UI - a button to trigger the ping and a results list to display responses.

**MeshCore reference:**
- `src/Mesh.h` — `Mesh::createControlData()` — the send path for a zero-hop control ping
- `src/Mesh.cpp` — `case PAYLOAD_TYPE_CONTROL:` — how a zero-hop control packet is received and dispatched to `onControlDataRecv()`

---

### Universal trackball back-swipe — M

Also tracked in `KNOWN_ISSUES.md`. Trackball left-swipe triggers `go_back()` only on the Chat screen. All other screens require the top-left back button.

**What's needed:** Extract the swipe handler from `src/ui/chat_screen.cpp` into `src/ui/navigation.cpp`. Apply to every screen. Handle conflict with screens that use left-swipe for their own navigation (two-swipe commit pattern recommended).

*(No MeshCore source reference — purely local UI work.)*

---

### Per-contact RSSI/SNR history graph — L

The Signal screen shows the most recent RSSI and SNR as a bar chart snapshot. There is no trend view — users cannot tell if signal quality is improving or degrading over time.

**What's needed:** A circular buffer of recent RSSI/SNR samples per contact (last N packets). A sparkline or step graph on the Signal or Contact Detail screen drawn with LVGL's `lv_chart`.

*(No MeshCore source reference — purely local UI work.)*

---

### Message search — M

There is no way to search message history. As channel history grows, finding a specific message requires manual scrolling.

**What's needed:** A search icon in the chat screen top bar. A text input that filters visible messages by substring match. Navigation between matches via trackball.

*(No MeshCore source reference — purely local UI work.)*

---

## System

### OTA firmware update — L

MeshCore companion firmware supports `start ota` to initiate an over-the-air update via BLE or serial. SlopOS has no OTA mechanism — firmware updates require a USB cable and flashing tool.

**What's needed:** An OTA partition layout in `platformio.ini`. An OTA download mechanism (WiFi or BLE — the ESP32-S3 has both, neither is currently initialized). A progress indicator in the UI.

**MeshCore reference:**
- `src/helpers/CommonCLI.h` — `handleCommand()` under `"ota"` subcommand: shows when OTA mode is triggered and what pre-OTA state cleanup looks like in reference implementations
- The actual OTA transfer uses ESP-IDF's `esp_ota_ops.h` — outside the MeshCore library

---

### Graceful shutdown from UI — S

There is no way to shut down the device from the UI. The user must hold the power button. On shutdown, pending NVS writes may not complete (separate known issue in `KNOWN_ISSUES.md`).

**What's needed:** A "Shut down" option in Settings (or long-press home button). Call `saveState()`, `saveChannels()`, delay 100ms for flash writes, then enter deep sleep with no wakeup configured.

*(No MeshCore source reference — T-Deck HAL feature using ESP-IDF `esp_deep_sleep_start()`.)*

---

### Launcher compatibility — M *(infrastructure)*

> **Not planned for companion.** This is a niche build target for running under `bmorcelli/Launcher`. Not relevant to the core companion experience.

**What's needed:** A `launcher-compatible` build target (`pio run -e SlopOS_TDeck_launcher`) that skips hardware init steps already performed by the launcher and handles shared peripheral state gracefully.

*(No MeshCore source reference — purely local build/HAL work.)*

---

## Security

### ACL / contact permissions — L

MeshCore defines permission levels for contacts: Guest, Read-only, Read-write, Admin. SlopOS treats all contacts identically — any node can send a message, and all messages are displayed.

**What's needed:** Add a `uint8_t perm` field to `SlopContact`. Default to Guest. Allow the user to promote specific contacts in the Contact Detail screen. Gate certain actions (channel management, terminal commands) behind permission checks.

**MeshCore reference:**
- `src/helpers/CommonCLI.h` — `setperm` command handler in `handleCommand()`: parses permission levels 0–3 and associates them with a contact public key; `NodePrefs` stores ACL entries
- `src/helpers/CommonCLI.h` — `NodePrefs::allow_read_only` (uint8_t) — controls whether unauthenticated reads are permitted

---

### Device admin password — M

There is no password protecting the Settings or Terminal screens. Anyone with physical access to the device can change radio parameters, read all messages, and modify node identity.

**What's needed:** An optional PIN or passphrase stored in NVS (hashed). Prompt on Settings/Terminal entry. Allow a grace period after recent input activity.

**MeshCore reference:**
- `src/helpers/CommonCLI.h` — `NodePrefs::admin_password` (char array) — the pattern for storing an admin password in NodePrefs; `handleCommand()` checks it for privileged commands in reference firmware

---

## Interoperability

### BLE companion protocol (expose T-Deck as a companion radio) — L *(infrastructure)*

> **Not planned for companion.** This turns the T-Deck into a BLE modem for a phone app — a different product than a standalone handheld companion. The T-Deck IS the companion already.

MeshCore defines a BLE UART companion protocol (Nordic UART service, UUID `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`) that allows a smartphone app to use the T-Deck as a mesh radio modem. SlopOS does not expose this protocol — it is a standalone UI device, not a radio modem.

**What's needed:** Enable the ESP32-S3 BLE stack. Implement the companion protocol command set. Add a "BLE modem mode" toggle in Settings that suspends the standalone UI and enters companion mode.

**MeshCore reference:**
- `examples/companion_radio/MyMesh.h` + `MyMesh.cpp` — authoritative companion radio implementation; defines all `CMD_*` request bytes (e.g. `APP_START`, `SEND_TXT_MSG`, `SEND_CHANNEL_TXT_MSG`, `GET_CONTACTS`, `SET_CHANNEL`, `SEND_PATH_DISCOVERY_REQ`, `SEND_TRACE_PATH`) and all `RESP_CODE_*` / `PUSH_CODE_*` response bytes
- `src/helpers/BaseSerialInterface.h` — abstract frame transport; `MAX_FRAME_SIZE = 172` bytes; `src/helpers/ArduinoSerialInterface.h` / `.cpp` — concrete Arduino implementation used by the companion radio

---

### Region management — L *(infrastructure)*

> **Not planned for companion.** Multi-region routing is a repeater feature. Companion nodes join one region and don't need to manage region tables.

MeshCore v1.10+ allows configuring which mesh sub-regions a node participates in. This is primarily a repeater feature but affects routing in multi-region deployments.

SlopOS has no region concept. In a multi-region deployment, a T-Deck may not receive messages from all regions it could potentially reach.

**What's needed:** Design a simplified region configuration UI. Add region filter settings to `NodePrefs`. Implement region header parsing in the MeshCore packet path.

**MeshCore reference:**
- `src/helpers/RegionMap.h` — `class RegionMap`; `struct RegionEntry` (fields: `uint16_t id`, `uint16_t parent`, `uint8_t flags`, `char name[31]`); flag constants `REGION_DENY_FLOOD` (0x01), `REGION_DENY_DIRECT` (0x02); `MAX_REGION_ENTRIES` = 32
- `src/helpers/RegionMap.cpp` — `putRegion()`, `findMatch()`, `getHomeRegion()`, `setHomeRegion()`, `load()`/`save()` to filesystem
- `src/helpers/TransportKeyStore.h` — per-region transport key lookup, referenced by `RegionMap`
- `src/helpers/CommonCLI.h` — `CommonCLI::handleRegionCmd()` — CLI entry point for all `region` subcommands

---

*Last updated: 2026-05-28. Phase 1 complete (advert parsing + contact details). 15 stale entries removed (all implemented by merged PRs #150-#187). Entries with incomplete understanding fixed (control packets, group datagrams, zero-hop ping). Cross-reference with `KNOWN_ISSUES.md` for bugs and workarounds in implemented features.*

---

## Implementation Plan

This section provides a phased roadmap for implementing **companion features only** — truly infrastructure-only items (BLE modem mode, region management, launcher compatibility) are documented above for reference but excluded from the plan.

Phases are ordered by dependency, effort, and practical value. Items within a phase can be done in any order.

### Dependency Summary

The MeshCore protocol analysis reveals **no strict topological ordering** among the payload types. The only companion-relevant dependency chain is:

```
Advert parsing → Location field → Contact locations on Map
                   └→ Data fields → Contact details screen
```

Everything else is independent and can be implemented in any sequence.

---

### Phase 2 — Radio Configuration

Medium-effort radio features that enhance configurability for the companion user.

| # | Feature | Effort | Why here |
|---|---------|--------|----------|
| 1 | RX gain boost toggle | S | `NodePrefs` flag, `RadioLibWrapper::setRxBoostedGainMode()` |
| 2 | Temporary radio config | M | Live-apply without NVS write, with revert timer |
| 3 | Duty cycle enforcement | M | Surface MeshCore budget, add configurable limit |

---

### Phase 3 — Messaging Polish

UI and protocol improvements to the chat experience.

| # | Feature | Effort | Why here |
|---|---------|--------|----------|
| 1 | Channel removal | S | Swipe-to-delete gesture, channel array management |
| 2 | Message delivery status (ACK) | M | Pending/acked/failed state in chat, `onAckRecv` hook |
| 3 | Message search | M | Chat screen search mode, substring filter |
| 4 | Per-contact RSSI/SNR history | L | `lv_chart` sparkline in Contact Detail or Signal |

**Phase 1 is complete** — contact details screen includes location, node type, and trace history.

---

### Phase 4 — Advanced Protocol

New packet types and application features that extend what a companion can do on the mesh.

| # | Feature | Effort | Why here |
|---|---------|--------|----------|
| 1 | Anonymous requests | M | Send path + UI entry for messaging unknown nodes |
| 2 | Direct request/response (REQ/RESPONSE) | M | `sendRequest()` wrapper, type code registry, room server fetch, path discovery |
| 3 | Group data datagrams | M | Type-code registry, `sendGroupDatagram()` API |
| 4 | Multipart messages | L | Reassembly buffer per sender, segmentation send |
| 5 | Raw custom payloads | L | Application dispatch interface, registration API |

---

### Phase 5 — UI & Hardware

Self-contained larger features for the companion experience.

| # | Feature | Effort | Why here |
|---|---------|--------|----------|
| 1 | QR code generation | L | QR library (2KB), LVGL canvas rendering, Share buttons |
| 2 | QR code / URI import | M | URI parser, Terminal command, Add Contact dialog |
| 3 | Contact locations on Map | M | **Depends on:** advert location parsing (Phase 1 ✅ complete — data available in `SlopContact`) |
| 4 | Room server message fetch | M | **Phase 1 ✅ complete** (contact type parsed in `SlopContact`). Still needs Phase 4 #2 (REQ/RESPONSE) for the actual fetch. UI action to fetch messages from ADV_TYPE_ROOM contacts. |
| 5 | OTA firmware update | L | WiFi/BLE init, partition layout, download + flash progress |
| 6 | ACL / contact permissions | L | Permission field on contacts, UI for promotion, action gating |
| 7 | Device admin password | M | Optional PIN hashed in NVS, prompt on Settings/Terminal entry |

---

### Suggested Sequence

```
Phase 2  →  Phase 3  →  Phase 4  →  Phase 5
(radio      (messaging  (advanced   (hard/
 config)     polish)     protocol)   infra)
```

Within each phase, items are in rough priority order. Start with the first ones as they unblock or inform the rest.

### Implementation Tips
- **Phase 1 is complete** — contact locations, node type, and details screen implemented. `docs/CONTACTS_SCREEN.md` and `docs/MESH_NETWORKING.md` should be updated to reflect the new advert fields.
- **After Phase 3**, add ACK status to `docs/CHAT_SCREEN.md`.
- **Any PR adding a `NodePrefs` field** must validate NVS migration — old firmware's saved prefs won't have the new field. `prefs_get()` uses `Preferences::getBytes()` which zero-fills missing keys; use the default-value pattern already in `prefs_get()`.
- **Protocol payload type features** (Phase 4) should include native-test mock coverage for new parse/dispatch paths in `test/slop_mesh_test.cpp`.
