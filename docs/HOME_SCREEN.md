# Home Screen

KrabOS uses a small, touch-first launcher with four primary surfaces. The
inherited SigurdOS screens remain available; they are grouped under **MORE**
instead of occupying twelve small launcher tiles.

This document describes the candidate implementation in the source tree. It is
not release or on-device validation evidence.

---

## Source Files

| File | Purpose |
|---|---|
| `src/ui/home_screen.cpp` | Launcher layout, status bars, touch and trackball activation, unread badge |
| `src/ui/home_screen.h` | Public update and input API |
| `src/ui/home_routes.cpp` | The four stable launcher routes |
| `src/ui/home_routes.h` | Route model and `HOME_ROUTE_COUNT` contract |
| `src/ui/screens/screen_settings.cpp` | **MORE** list containing the retained advanced screens |
| `src/ui/responsive.h` | Shared status-bar and content-area dimensions |
| `src/ui/theme.h` | Runtime colours and common style helpers |

---

## Layout

At the T-Deck Plus's 320x240 landscape resolution, the four routes are shown as
a 2x2 grid of large targets:

```text
+----------------------------------+
| KRABOS  Canada 902-928    .....  |
+----------------------------------+
| +--------------+ +-------------+ |
| |    CHATS     | |     MAP     | |
| |      o       | |             | |
| +--------------+ +-------------+ |
| +--------------+ +-------------+ |
| |   NETWORK    | |    MORE     | |
| |              | |             | |
| +--------------+ +-------------+ |
+----------------------------------+
| device-name                 72%  |
+----------------------------------+
```

The exact glyphs are LVGL symbols. The sketch represents placement, not a
pixel-exact screenshot.

### Primary surfaces

| Tile | Destination | Purpose |
|---|---|---|
| **CHATS** | `Screen::Chat` | Channels, rooms, direct messages and message history |
| **MAP** | `Screen::Map` | Offline maps, GPS tracks and the on-device tile downloader |
| **NETWORK** | `Screen::Network` | Nearby-node discovery and mesh status |
| **MORE** | `Screen::Settings` | Contacts, channels, repeaters, radio, GPS, files, diagnostics and other retained tools |

`home_routes.cpp` is the route table and source of truth. Keeping it separate
from rendering makes route count, labels, filters and destinations testable
without LVGL.

### Status bars

The top bar shows:

- the text wordmark `KRABOS`;
- the exact matched radio profile (for example `Canada 902-928` or
  `USA 902-928`), `Custom RF` for an edited tuple, or the red warning
  `Radio setup required` before configuration;
- current mesh time;
- RSSI dots and companion status.

The bottom bar shows the device name and battery percentage. Battery text uses
the warning colour at 20% or below.

---

## Tile Style

Primary tiles are intentionally different from the inherited zero-radius,
twelve-tile launcher:

- 8px corner radius;
- 1px divider-colour border at rest;
- accent-colour border for the selected tile;
- dark tertiary background with a darker pressed state;
- centred LVGL symbol and short uppercase label;
- 3px outer padding and 3px gaps.

Only **CHATS** owns an unread badge. It stays hidden at zero, shows the stable
per-conversation unread total otherwise, caps display at `99+`, and uses the
mention colour when an unread mention exists.

Rounded launcher cards are a KrabOS-specific surface choice. Other screens
should continue to use the appropriate shared theme helper or match their
existing KrabOS pattern; this document does not redefine every control in the
firmware.

---

## Input Behaviour

Touch, the keyboard focus group and the trackball activate the same four route
objects.

| Trackball input | Result |
|---|---|
| Left / Right | Move within the current row and wrap at its edge |
| Up / Down | Move within the current column and wrap between rows |
| Click | Activate the selected route |

The initial selection is **CHATS**. Before navigation, the shared activator
applies the route's chat/contact filters so touch and trackball cannot inherit a
stale filter from an earlier visit.

---

## MORE Hub

**MORE** opens the existing Settings route, whose visible title is `More`. It
is a scrollable list rather than a fifth launcher page. The current list links
to:

- Contacts, Channels / Rooms, Repeaters and Advertise;
- Packets, Message search, Terminal, Signal, Mesh dashboard and Telemetry;
- Wi-Fi, Bluetooth, Radio / Mesh and Flood regions;
- GPS / Location, Display / UI, SD files, Setup, System and Node Stats.

The normal navigation PIN gate still protects this route when a device PIN is
configured.

---

## Lifecycle and Updates

`home_screen_create()` and `home_screen_show()` both rebuild the launcher and
load it through `show_screen()` with no transition animation. Using the same
manual screen-deletion policy as the rest of the UI avoids mixing LVGL
auto-delete modes.

`ScreenLifetime` clears cached LVGL object pointers when the root is deleted.
The public update functions therefore no-op when Home is not active instead of
touching freed widgets:

| Function | Update |
|---|---|
| `home_screen_update_battery()` | Battery text and low-battery colour |
| `home_screen_update_time()` | Top-bar time |
| `home_screen_update_channels()` | Configuration warning or exact active radio profile status |
| `home_screen_update_badges()` | CHATS unread count and mention colour |

---

## Relationship to Upstream SigurdOS

KrabOS retains the upstream screen implementations and GPL attribution while
changing how users reach them. References to a 4x3 grid, twelve primary tiles,
zero-radius launcher cards or animated Home transitions describe the inherited
SigurdOS launcher, not the current KrabOS candidate.
