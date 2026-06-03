# Feature Status (vs MeshCore)

This document catalogs MeshCore protocol features that SigurdOS-TDeck does **not** implement. Implemented features are tracked in the Git history and ROADMAP.md — this file only lists what's still missing or explicitly declined.

SigurdOS-TDeck is a standalone **companion-radio firmware** for the LilyGo T-Deck. It interoperates with any MeshCore node and is designed for the end-user handheld experience — not for infrastructure roles (dedicated repeaters, room servers, sensors).

> **Status legend:**
> - ❌ **NOT DOING / NOT NEEDED** — explicitly declined.
> - *(unmarked)* — still missing.

## Where to find things in upstream MeshCore

Every reference below links directly into **`https://github.com/meshcore-dev/MeshCore`** (main branch) so other agents can jump straight to the source. The repo submodule (`lib/meshcore/`) is pinned to companion firmware **v1.15.0 / `FIRMWARE_VER_CODE 12`** ([`examples/companion_radio/MyMesh.h`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.h)). Line numbers drift between versions — references cite **symbol names**, so grep the linked file if a line has moved. If a symbol can't be found upstream, check `lib/meshcore/` directly (the pinned commit is authoritative for what SigurdOS actually builds against).

The single most useful reference is the companion radio command dispatcher:
[`examples/companion_radio/MyMesh.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp) — `handleCmdFrame()` defines all **58 `CMD_*` request codes** (numbered up to 65, with gaps), **28 `RESP_CODE_*` reply codes**, and **17 `PUSH_CODE_*` async push codes**. Almost every protocol feature below has a worked example in this one file.

### ⚠️ Architectural note — read before estimating effort

The companion radio (`MyMesh`) extends **`BaseChatMesh`** ([`src/helpers/BaseChatMesh.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/BaseChatMesh.h)), which provides ready-made high-level helpers: `sendLogin()`, `sendCommandData()`, `sendRequest()`, `resetPathTo()`, `processAck()` with an `expected_ack_table`, an offline message queue, and contact-by-pubkey lookup.

**SigurdOS's `SigurdMeshV2` now also extends `BaseChatMesh`** (`src/mesh/sigurd_mesh_v2.h`). The migration off the old raw `::mesh::Mesh` subclass is **complete**. It therefore *inherits* those helpers, which is why most protocol features are thin **wrapper + UI** work rather than bespoke protocol code. Contacts use `BaseChatMesh`'s `::ContactInfo` (with `out_path` / `out_path_len`); the UI is insulated behind `sigurdos::mesh::ContactInfo` in `mesh_wrapper.h`.

---

## How to read effort levels

Effort levels on the still-missing items below estimate the work remaining:

- **S** — small: isolated change, few files, testable in native tests
- **M** — medium: touches mesh layer + UI, needs device testing
- **L** — large: architectural change, multiple screens or protocol work

---

## Protocol / Packet Types

### Multipart messages (PAYLOAD_TYPE_MULTIPART 0x0A) — ❌ NOT DOING

- **Reason:** User declined — not implementing. 150-byte send cap remains.
- **Reference (for posterity):** MeshCore defines a multipart packet type for segmenting large payloads across LoRa frames. [`src/Packet.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/Packet.h) — `#define PAYLOAD_TYPE_MULTIPART 0x0A`

---

### Raw custom payloads (PAYLOAD_TYPE_RAW_CUSTOM 0x0F) — ❌ NOT DOING

- **Reason:** User declined — not implementing.
- **Reference (for posterity):** `onRawDataRecv` is a stub. (Note: SigurdOS uses `createRawData()` internally for PING/PONG, but there is no general app dispatch.) [`src/Packet.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/Packet.h) — `#define PAYLOAD_TYPE_RAW_CUSTOM 0x0F`

---

## Radio Configuration

### Temporary radio config (no reboot) — ❌ NOT NEEDED

- **Reason:** User declined — not implementing. The live-apply plumbing exists (`applyRadioParams()` / `revertRadioParams()` in the wrapper) but no auto-revert timer / "Try" UI is planned.
- **Reference (for posterity):** MeshCore's CLI supports `tempradio freq,bw,sf,cr,timeout_mins` — a trial config that auto-reverts on reboot without writing NVS.
  - [`src/helpers/CommonCLI.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/CommonCLI.cpp) — `handleCommand()` `tempradio` branch; `temp_radio_timeout` in NodePrefs
  - [`src/helpers/radiolib/RadioLibWrappers.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/radiolib/RadioLibWrappers.h) — `setParams(freq, bw, sf, cr)`

---

## Infrastructure-only (documented, not planned)

> These turn the T-Deck into something other than a handheld companion. Listed for completeness; excluded from the implementation plan.

### BLE companion protocol (expose T-Deck as a radio modem) — L

MeshCore's BLE UART companion protocol (Nordic UART service, UUID `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`) lets a phone app use the radio as a modem. The T-Deck *is* the companion already, so this is a different product.

**MeshCore reference:**
- [`examples/companion_radio/MyMesh.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp) — the authoritative `CMD_*` / `RESP_CODE_*` / `PUSH_CODE_*` frame protocol
- [`src/helpers/BaseSerialInterface.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/BaseSerialInterface.h) — frame transport (`MAX_FRAME_SIZE = 172`); [`ArduinoSerialInterface.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/ArduinoSerialInterface.cpp) — concrete impl

---

### Region management / flood scope keys — L

MeshCore v1.10+ supports sub-region routing and per-region transport keys (`CMD_SET_FLOOD_SCOPE_KEY`, `CMD_SET/GET_DEFAULT_FLOOD_SCOPE`, `CMD_SET_PATH_HASH_MODE`). Primarily a repeater feature.

**MeshCore reference:**
- [`src/helpers/RegionMap.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/RegionMap.h) / [`RegionMap.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/RegionMap.cpp) — `RegionMap`, `RegionEntry`, `REGION_DENY_FLOOD`/`REGION_DENY_DIRECT`, `MAX_REGION_ENTRIES`
- [`src/helpers/TransportKeyStore.h`](https://github.com/meshcore-dev/MeshCore/blob/main/src/helpers/TransportKeyStore.h) — per-region transport key lookup
- [`examples/companion_radio/MyMesh.cpp`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp) — `CMD_SET_FLOOD_SCOPE_KEY` (54), `CMD_SET_PATH_HASH_MODE` (61), `CMD_SET/GET_DEFAULT_FLOOD_SCOPE` (63/64)
- [`examples/companion_radio/NodePrefs.h`](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/NodePrefs.h) — `path_hash_mode`, `default_scope_name`, `default_scope_key`

---

### Launcher compatibility — M

A niche build target for running under `bmorcelli/Launcher`. Not relevant to the standalone companion experience. (See `KNOWN_ISSUES.md`.)

*(No MeshCore reference — local build/HAL.)*

---

*Last reviewed: 2026-06-03 against companion firmware v1.15.0 and dev branch. All previously tracked features are ✅ implemented. Declined items remain above for reference.*
