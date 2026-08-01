# KrabOS roadmap

This is the implementation ledger for KrabOS. The public summary in
[README.md](README.md) and this file must move together. A phase advances only
after its exit gate produces evidence for the exact tested commit.

## Status contract

- `✓ Complete` — every exit-gate item passed for an immutable exact SHA.
- `▶ Active` — the only phase currently receiving implementation work.
- `○ Planned` — cannot start until every earlier phase is complete.
- `! Blocked` — the active phase cannot safely advance; its issue must name the
  blocker and preserve fail-closed release state.

<!-- roadmap-status:start -->
| Phase | Status | Exit gate | GitHub |
|---|---|---|---|
| M0 | ▶ Active | Exact-device backup and unattended RF-off recovery pass without touching either neighbouring ESP32. | [Milestone](https://github.com/n30nex/KrabDeck/milestone/1) · [Issue #1](https://github.com/n30nex/KrabDeck/issues/1) |
| M1 | ○ Planned | A trusted `main` push builds, flashes, verifies, tests, rolls back on injected failure, and publishes a signed edge release. | [Milestone](https://github.com/n30nex/KrabDeck/milestone/2) · [Issue #2](https://github.com/n30nex/KrabDeck/issues/2) |
| M2 | ○ Planned | Every retained feature is reachable by touch, keyboard, and trackball with clean visual evidence. | [Milestone](https://github.com/n30nex/KrabDeck/milestone/3) · [Issue #3](https://github.com/n30nex/KrabDeck/issues/3) |
| M3 | ○ Planned | Safe client parity, one boot advert, and correlated bot DM/channel delivery pass without Public chat traffic. | [Milestone](https://github.com/n30nex/KrabDeck/milestone/4) · [Issue #4](https://github.com/n30nex/KrabDeck/issues/4) |
| M4 | ○ Planned | SD maps survive interruption and reboot, secrets stay private, and simulated plus live GPS gates pass. | [Milestone](https://github.com/n30nex/KrabDeck/milestone/5) · [Issue #5](https://github.com/n30nex/KrabDeck/issues/5) |
| M5 | ○ Planned | Exact-SHA regression, recovery, accessibility, licensing, and extended soak gates automatically publish `v1.0.0`. | [Milestone](https://github.com/n30nex/KrabDeck/milestone/6) · [Issue #6](https://github.com/n30nex/KrabDeck/issues/6) |
<!-- roadmap-status:end -->

## M0 — Seed and Safeguard

- Import the pinned SigurdOS history, retain GPL attribution, and publish the
  finished KrabDeck front page, milestones, and phase issues.
- Pin the Pi toolchain, shared hardware lock, exact USB/sysfs identity, eFuse
  MAC, and private encrypted test credentials.
- Before any erase, export identity and setup, capture and hash a mode-`0600`
  full-flash backup, and qualify a last-known-good RF-off recovery image.
- **Gate:** clean native/build checks and an unattended recovery drill on the
  exact T-Deck; the D1L and RF peer remain unopened by the flash path.

## M1 — Autonomous KrabOS Edge

- Apply KrabOS branding and Canadian defaults consistently to onboarding,
  factory reset, stored preferences, manifests, and release assets.
- Send one immediate flood advert after each production boot using the latest
  location, including a stored stale fix. Recovery and debug images remain
  RF-off.
- On every trusted `main` push, build, test, flash, verify, reboot, exercise,
  redact, sign, attest, and publish the exact candidate.
- **Gate:** a passing edge release plus an injected failure proving exactly one
  retry and automatic RF-off recovery.

## M2 — Krab Material Interface

- Present four primary surfaces: Chats, Map, Network, and More. Keep every
  advanced capability reachable under More rather than deleting it.
- Use a charcoal/navy hierarchy, coral accent, clear focus states, and large
  touch targets while preserving keyboard and trackball shortcuts.
- **Gate:** input/navigation matrix, visual captures, and memory soak pass for
  every retained surface.

## M3 — MeshCore Client Parity

- Close the safe, wire-representable feature matrix for conversations,
  contacts, channels, rooms, repeaters, routing, telemetry, tools, settings,
  migration, and bonded BLE.
- Retain deliberate refusals for raw packet injection, private-key export
  defaults, and unsafe companion operations.
- **Gate:** parity contract plus correlated boot advert, bot DM, test-channel,
  delivery ACK, and no-Public-chat evidence.

## M4 — SD Maps, Wi-Fi, and GPS

- Preserve canonical FAT32 tiles at `/tiles/<z>/<x>/<y>.png`; add NRCan and
  permitted generic XYZ sources, `.part` downloads, atomic rename, retry,
  pause/resume/cancel, and storage UI.
- Preserve a healthy card. Permit one in-device format only after two
  deterministic filesystem failures, followed by remount and integrity proof.
- Validate GPS at RX44/TX43 with 38400 then 9600 fallback, Toronto time, map
  centring, tracks, simulated NMEA per release, and a weekly live fix.
- **Gate:** offline reboot rendering, interruption recovery, secret redaction,
  simulated GPS, and current live GPS receipt all pass.

## M5 — KrabOS 1.0

- Complete regression, OTA/SD update equivalence, recovery drills,
  accessibility, build instructions, source/licence bundle, and release docs.
- Run a 15-minute edge soak per candidate and a weekly two-hour extended soak.
- **Gate:** exact-SHA physical evidence, no open P0/P1 defects, and automatic
  signed `v1.0.0` publication.

## Update rule

Update both status tables and the README Mermaid symbols/classes in the same
commit. Run `python scripts/check_roadmap.py`; CI runs the same command. Never
mark a phase complete from issue completion percentages or unverified external
state.
