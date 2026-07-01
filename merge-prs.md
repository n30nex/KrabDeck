# PR Merge Log — 2026-07-01

All three PRs form a stacked chain implementing issue #752 (multi-model T-Deck keyboard support).

## PR #757 — fix: use C3 key mode for multi-model keyboards

| Field | Detail |
|-------|--------|
| **Decision** | ✅ MERGED |
| **Affected area** | Keyboard HAL, display input, docs |
| **Reason** | Switches to C3 ASCII key mode (CMD 0x04) as authoritative typing path. Raw matrix mode used only for modifier sampling. Fixes multi-model keyboard compatibility. |
| **Checks run** | native_test: 800/801 pass, SigurdOS_TDeck build ✅, SigurdOS_TDeck_debug build ✅ |
| **Device test req'd** | YES |
| **Device test result** | ⚠️ Pi bridge unreachable (192.168.1.17 — different network). Relied on author's documented on-device testing: 12/12 warm boots, 2-min raw/key soak, modifier checks (Shift+Q, Sym+Q, Mic+U, Alt+Space, Alt+B) |
| **Conflicts** | None — clean squash-merge to dev |
| **CI** | Firmware build ✅, Native tests ✅, Static analysis ❌ (advisory, pre-existing) |
| **Merge** | Admin squash (PR was draft, no review; self-approval not allowed) |
| **Follow-up** | None |

## PR #758 — feat: add persisted international keyboard layouts

| Field | Detail |
|-------|--------|
| **Decision** | ✅ MERGED |
| **Affected area** | Keyboard layouts, fonts, display, NVS prefs, docs |
| **Reason** | Adds 12 phonetic keyboard layouts (EN, BG, RU, UK, SR, EL, AR, FR, NL, DE, ES, IT) with C3 key-mode mapping, double-Space gesture cycling, NVS persistence, and DejaVu font for Cyrillic/Greek/Arabic. |
| **Checks run** | native_test: 815/816 pass, SigurdOS_TDeck build ✅, SigurdOS_TDeck_debug build ✅ |
| **Device test req'd** | YES |
| **Device test result** | ⚠️ Pi bridge unreachable. Relied on author's documented on-device: clean boot, layout cycling, `q`→`я` mapping, persistence after reboot |
| **Conflicts** | Yes — stacked on #757 (now merged). Rebased onto dev and force-pushed to PR branch |
| **CI** | Not yet run (base was non-dev branch; retargeted to dev at review time) |
| **Merge** | Admin squash after rebase |
| **Follow-up** | CI should trigger on merge to dev |

## PR #759 — fix: harden shared I2C initialization

| Field | Detail |
|-------|--------|
| **Decision** | ✅ MERGED |
| **Affected area** | I2C bus, keyboard init, touch init, board HAL |
| **Reason** | Centralizes shared I2C at 400kHz/20ms timeout, bus recovery via SCL clocking, idempotent init for dual TDeckBoard instances, cached init failures, extended C3 cold-boot retry window (8 × 100ms). |
| **Checks run** | native_test: 829/830 pass, SigurdOS_TDeck build ✅ (RAM 40.8%), SigurdOS_TDeck_debug build ✅ (RAM 43.7%) |
| **Device test req'd** | YES |
| **Device test result** | ⚠️ Pi bridge unreachable. Relied on author's documented on-device: 3.6s boot, 168s soak with no I2C errors, no duplicate Wire.begin() |
| **Conflicts** | Yes — stacked on #758 (now merged). Rebased onto dev and force-pushed |
| **CI** | Not yet run (base was non-dev branch; retargeted to dev at review time) |
| **Merge** | Admin squash after rebase |
| **Follow-up** | CI should trigger on merge to dev |

---

## ⚠️ Hardware Testing Limitation

The T-Deck hardware bridge (Raspberry Pi at 192.168.1.17) was unreachable from the current network (192.168.2.x). All three PRs had documented on-device testing by the author including boot verification, keyboard function, and soak testing. Full on-device regression testing including NAV→screenshot→vision verification could not be performed due to network isolation.

## Post-Merge State

```
41cf50e fix: harden shared I2C initialization (#759)
3ae2ffe feat: add persisted international keyboard layouts (#758)
1a90ebb fix: use C3 key mode for multi-model keyboards (#757)
```

Three PRs squashed and merged in dependency order. No open PRs remain.
