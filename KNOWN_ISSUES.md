# Known Issues

This document tracks known issues, bugs, and missing features in SlopOS. Contributions welcome — pick something from the list and open a PR.

---

## Chat Screen

### Message scroll limiting
The chat view has no upper bound on how many messages it buffers in memory. After extended use (especially on busy mesh networks with many nodes), the message list grows unbounded, consuming PSRAM and eventually causing slowdowns or crashes.

**What's needed:** A configurable message cap (e.g. 200-500 messages), with oldest messages evicted as new ones arrive. The cap should be per-channel so a busy channel doesn't starve quieter ones.

### Channel selection — message preview clipping
When scrolling through channels in the channel selector, message previews (the last message in each channel) don't properly truncate. Long messages overflow the preview area and clip visually, overlapping adjacent UI elements.

**What's needed:** Proper string truncation in the channel list — clamp preview text to fit the available width, appending "..." when truncated. The `build_channel_string` function in `home_screen.cpp` was recently hardened (PR #29) — a similar approach should be applied to the preview text in the channel selector.

---

## Contacts Screen

### Theme integration
Contacts are rendered too dark, blending into the background. On the dark pixel theme (`#0A0A0A` / `#111111` backgrounds), the secondary text color makes contact names and RSSI values nearly invisible.

**What's needed:**
- Bump contrast on contact names and labels to at least `#EAEAEA` (TEXT_PRIMARY)
- Use `#00BFFF` (ACCENT) for the selected/highlighted contact
- Ensure RSSI/SNR values use a visible muted color like `#949BA4` (TEXT_SECONDARY)
- The `#5C6067` (TEXT_MUTED) range is too dark for body text on this theme

---

## Signal Bars

### RSSI-based signal strength indicator
There's no visual signal strength indicator anywhere in the UI. Users have to navigate to the Heard screen and read raw RSSI numbers to gauge link quality.

**What's needed:** A small signal bar widget (1-5 bars) based on the last received message's RSSI from each contact. Bars should be rendered with the pixel aesthetic — blocky, no curves. Reference threshold levels:

| Bars | RSSI Range |
|------|-----------|
| 5    | > -70 dBm |
| 4    | -70 to -85 dBm |
| 3    | -85 to -95 dBm |
| 2    | -95 to -105 dBm |
| 1    | < -105 dBm |

Could be shown next to each contact in the Contacts screen, in the chat header, and on the home screen mesh status.

---

## Finder

### Zero-hop ping for nearby discovery
The finder feature needs a proper implementation that sends a zero-hop (TTL=1) ping to discover nearby repeaters. Currently there's no way to probe what's in immediate radio range without relying on periodic adverts.

**What's needed:**
- A "Ping Nearby" action that sends a broadcast with hop limit = 1
- A response handler that collects replies over a short window (2-3 seconds)
- Display results grouped by RSSI (strongest first)
- Cooldown of 30 seconds between pings to avoid flooding

---

## Heard Screen

### Needs redesign or replacement
The Heard screen currently shows a raw list of heard nodes with RSSI values, but it duplicates functionality present in the Network screen and lacks useful sorting or filtering.

**Possible directions:**
- **Re-implement as a signal history view** — show RSSI/SNR trends per node over time (last 10 heard events), so users can see if a node is getting closer or farther
- **Merge into Network screen** — add a signal strength column and remove Heard entirely
- **Keep as a raw packet log** — show the last N heard transmissions with timestamps, source, RSSI, SNR, and payload type. Useful for debugging mesh behavior but not for daily use

Open to ideas from the community.

---

## Map Screen

### Insufficient testing
The offline tile map feature hasn't been thoroughly tested in the field. Known unknowns:

- SD card detection and mount reliability across different card brands/formats
- Tile downloader script compatibility with different tile sources
- Pan/zoom performance with large tile sets on the ESP32-S3 PSRAM canvas
- LRU cache eviction behavior when the tile cache exceeds available PSRAM
- GPS pin integration with map centering

**What's needed:** Field testing with various SD cards, tile sets, and GPS conditions. A test script for the tile downloader would also help contributors validate their tile packs before putting them on the SD card.

---

## Trace Screen

### Theme contrast
The Trace screen (path quality to a selected contact) uses text colors that are too dark against the background, making it hard to read hop-by-hop metrics.

**What's needed:** Same treatment as the Contacts screen — bump text colors to at least TEXT_SECONDARY (`#949BA4`) for labels and TEXT_PRIMARY (`#EAEAEA`) for values. Use ACCENT (`#00BFFF`) for the direct/active path highlight.

---

## Terminal

### Undocumented commands
The built-in serial/diagnostics terminal exposes several internal commands but there's no documentation on what's available or what each command does. Users have to read the source code to discover features.

**What's needed:** A `help` command that lists all available commands with a one-line description. A `help <command>` variant that shows usage details. The help text should be stored as a single `const char*` array in `src/ui/terminal.cpp` so it stays easy to update.

Common commands that should be documented:
| Command | Description |
|---------|-------------|
| `help` | List available commands |
| `status` | Show mesh status, node count, uptime |
| `channels` | List joined channels |
| `nodes` | List known nodes |
| `signal` | Show RSSI/SNR for last heard transmission |
| `send <text>` | Send a text message to the current channel |
| `save` | Force save state to NVS |
| `reset` | Reboot the device |
| `gps` | Show current GPS fix data |

---

## How to Help

Pick any item from the list above and open a PR against the `dev` branch. See [`CONTRIBUTING.md`](./CONTRIBUTING.md) for the full contribution workflow.
