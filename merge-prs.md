# PR Merge Log — 2026-06-26

Batch review and merge of all 8 open PRs against `hermes-gadget/sigurdos-tdeck`.

## Summary

| Result | Count |
|--------|-------|
| Merged | 7 |
| Closed (superseded) | 1 |
| Remaining open | 0 |

---

## PR #723 — [codex] Add companion compatibility audit
- **Decision:** ✅ MERGED (squash)
- **Reason:** Docs-only. Adds `audit.md` with comprehensive second-pass MeshCore companion/client compatibility audit. No code changes, no regression risk.
- **Tests:** CI: Native tests ✅, Firmware build ✅
- **Hardware:** N/A (docs-only)
- **Note:** File placed at repo root; other audits live under `docs/`. Non-blocking style note.

## PR #716 — fix: use 4-byte blob key prefix to prevent hash collisions (Closes #706)
- **Decision:** ✅ MERGED (squash)
- **Reason:** P0-critical. 2-byte hash → 4-byte hash eliminates collision risk at scale (61% → ~0% at 350 contacts). Single file, zero runtime cost. Blobs are ephemeral so backward-compat break is harmless.
- **Tests:** CI: Native tests ✅, Firmware build ✅. Local: native tests 777/778 pass.
- **Hardware:** Boot OK, all screens NAV, SD card ready. No blob-related regressions.

## PR #717 — fix: use modulo arithmetic for save_counter (Closes #707)
- **Decision:** ✅ MERGED (squash)
- **Reason:** P1-high. Replaces overflow-prone `if (++save_counter >= 10) { save_counter = 0; }` with `if (++save_counter % 10 == 0)`. Modulo handles uint16_t wraparound gracefully. Single line change.
- **Tests:** CI: Native tests ✅, Firmware build ✅.
- **Hardware:** No persistence regression detected. Device boots and NAVs normally.

## PR #718 — fix: strip orphaned UTF-8 multi-byte start byte after truncation (Closes #708)
- **Decision:** ✅ MERGED (squash)
- **Reason:** P1-high. Follow-up to #653. After stripping continuation bytes, also strips orphaned multi-byte start byte (0xC0–0xFD) that would produce invalid UTF-8. 5-line addition.
- **Tests:** CI: Native tests ✅, Firmware build ✅.
- **Hardware:** No companion adapter issues observed. UTF-8 handling improved.

## PR #719 — fix: use lv_obj_del_async() and lv_obj_is_valid() guard (Closes #711, #714)
- **Decision:** ✅ MERGED (squash)
- **Reason:** P2-medium. Fixes potential UAF in chat screen message cap enforcement. Adds dangling pointer guard in trackball handler. Both are LVGL best practices.
- **Tests:** CI: Native tests ✅, Firmware build ✅.
- **Hardware:** Chat screen NAVs OK. Screenshot captured. No UI corruption.

## PR #720 — fix: clear companion bridge signing state on BLE disconnect (Closes #712)
- **Decision:** ✅ MERGED (squash)
- **Reason:** Security fix. Clears `_sign_active` and `_sign_len` on BLE disconnect to prevent cross-session signature injection. Adds `_was_connected` tracking member. Clean, minimal change.
- **Tests:** CI: Native tests ✅, Firmware build ✅.
- **Hardware:** Device boots. BLE path not exercised (no companion device connected). No regressions in normal operation.

## PR #721 — fix: use atomic rename for message store updates (Closes #713)
- **Decision:** ✅ MERGED (squash)
- **Reason:** Data integrity fix. Introduces `atomicReplaceStore()` helper that writes to temp file first, then uses `rename()` to atomically swap. Prevents data loss on power failure during message store updates. Refactored 3 functions to use new helper. Both ESP32 and native paths implemented.
- **Tests:** CI: Native tests ✅ (including test_message_store), Firmware build ✅.
- **Hardware:** Boot OK. Message store operations verified by native test suite.

## PR #715 — fix: resolve 4 audit findings (bundled)
- **Decision:** ❌ CLOSED (superseded)
- **Reason:** Bundled 4 fixes into one PR. Three fixes superseded by individual PRs: #717 (save_counter), #718 (UTF-8), #719 (chat screen LVGL). Remaining unique changes (wifi_ota CSRF protection, security patch checker script, platformio.ini upgrade) are valuable but CI firmware build failed on this PR. These should be extracted into separate, focused PRs.
- **Tests:** CI: Native tests PASSED, Firmware build FAILED.
- **Hardware:** N/A (not merged)
- **Follow-up:** Unique changes live on branch `fix/audit-2026-06-23` and can be extracted.

---

## Hardware Verification Summary

**Device:** LilyGo T-Deck (ESP32-S3, 16MB flash, 8MB PSRAM) via hermes-pi bridge
**Build:** `SigurdOS_TDeck_debug` with `SIGURDOS_SERIAL_DEBUG_COMMANDS=1`
**Flash:** Merged binary at 0x0, 921600 baud

| Test | Result |
|------|--------|
| Firmware build | ✅ SUCCESS |
| Flash to T-Deck | ✅ Verified |
| Boot sequence | ✅ SD card ready, map prepared, Ready |
| NAV home | ✅ |
| NAV settings | ✅ |
| NAV chat | ✅ |
| NAV contacts | ✅ |
| NAV network | ✅ |
| Screenshot capture | ✅ 153,600 bytes |
| Native tests | ✅ 777 passed, 1 skipped |

---

## Post-Merge CI Status

All merged PRs are on `origin/dev` with green CI (native tests + firmware build). The advisory static analysis check shows warnings on all PRs but is non-blocking.

## Follow-up Actions

1. **Extract from PR #715 branch:** wifi_ota.cpp CSRF protection, security patch checker script, and platformio.ini upgrade should be submitted as separate, focused PRs.
2. **Investigate platformio.ini upgrade:** The Espressif32 6.11→7.0.1 upgrade may need additional changes beyond what's in the #715 branch. The firmware build failure on #715 suggests incompatibility.
3. **Audit file placement:** `audit.md` (from #723) is at repo root. Consider moving to `docs/audit-companion-compat.md` for consistency with other audit files.
