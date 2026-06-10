# Known Issues

This document tracks currently open known issues, bugs, and missing features in SigurdOS T-Deck firmware. All historically tracked issues that have been resolved are maintained in the Git history — check merged PRs and commit logs for the full record.

---

## Launcher Compatibility

### Supported — bmorcelli/Launcher (v2.7.2+)

SigurdOS can now be installed as a Launcher app. See [`firmware/README.md`](../firmware/README.md) for the full installation guide and caveats.

**What's implemented (Phase 1/4):**
- ✅ Launcher install via SD, WebUI, or direct GitHub URL — use `SigurdOS-tdeck-launcher.bin`
- ✅ Runtime Launcher detection (probes for test-subtype app partition)
- ✅ Self-OTA gated with on-screen explanation when under Launcher
- ✅ Boot-time diagnostics when app-only install loses persistence
- ✅ Self-OTA disabled to prevent flash corruption of co-installed apps
- ✅ SPIFFS partition created for persistence (when using merged image)

**Remaining gaps (Phase 2/3/5 — in progress):**
- 🔄 Phase 2: Boot/flash compatibility testing — **in progress**
- ⚠️ Warm-handoff peripheral state: Launcher's I2C/touch/keyboard init before ESP.restart() may leave peripherals in unexpected states. The keyboard init currently uses a single-NACK-abort which may fail after soft reset. Expected fix: keyboard-init retry/timing window.
- ⚠️ Warm-handoff soak testing (10+ power-cycle loops)
- ⚠️ Multi-app coexistence test (install Bruce alongside SigurdOS, switch back)
- ⏳ **Phase 5:** Full regression matrix (T1–T14)
- ❌ Not yet listed in LauncherHub catalog (requires maintainer coordination)

---

## How to Help

Pick any item from the list above and open a PR against the `dev` branch. See [`CONTRIBUTING.md`](./CONTRIBUTING.md) for the full contribution workflow.
