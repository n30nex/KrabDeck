# Contributing to KrabDeck / KrabOS

Thanks for considering contributing. KrabOS is under active development and
welcomes bug reports, feature requests, documentation and code contributions.

## Quick Start

```bash
git clone --recurse-submodules https://github.com/n30nex/KrabDeck.git
cd KrabDeck
pio test -e native_test -v
```

**Important:** MeshCore lives as a git submodule at `lib/meshcore/`. If you cloned without `--recurse-submodules`, run `git submodule update --init` — nothing compiles without it.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Reporting Bugs](#reporting-bugs)
- [Suggesting Features](#suggesting-features)
- [Submitting Code Changes](#submitting-code-changes)
- [Development Workflow](#development-workflow)
- [Style Guide](#style-guide)
- [Testing](#testing)
- [License](#license)

## Code of Conduct

This project follows a **no-drama** policy. Be respectful, constructive, and
assume good faith. Harassment, trolling, and personal attacks will not be
tolerated.

## Reporting Bugs

1. Search [open issues](https://github.com/n30nex/KrabDeck/issues)
   first — your bug may already be known.
2. If not found, open a new issue with:
   - A clear, descriptive title
   - Steps to reproduce (exact and minimal)
   - Expected vs. actual behavior
   - T-Deck Plus hardware revision and fixture identity, if known
   - Serial monitor logs (run `pio device monitor -b 115200`)
3. Label the issue `bug`.

## Suggesting Features

1. Search existing issues for `[Feature request]`.
2. Open a new issue with `[Feature request]` in the title.
3. Describe the use case and the problem it solves.
4. Sketch the desired behavior or UI — code snippets help.

## Submitting Code Changes

### Before you start

1. **Open an issue first** describing what you want to work on — even for small changes. This lets maintainers know someone is working on it and avoids duplicate effort.
2. Label the issue appropriately (`bug`, `enhancement`, `docs`, etc.).
3. Small fixes (typos, comments, one-line bug fixes) can skip the issue step if discussed with a maintainer first.
4. **Check `docs/MISSING_FEATURES.md`** before implementing a new feature. If the capability is already catalogued there, you have a head start — MeshCore source references and effort estimates are provided.

### Work on it

1. **Fork the repo** on GitHub.
2. **Create a branch on your fork** from the `main` branch:
   ```
   git checkout main
   git pull upstream main
   git checkout -b fix/your-bug-fix
   # or: feature/your-feature
   # or: docs/your-doc-change
   ```
3. **Make your changes** — keep commits atomic and descriptive.
4. **Write or update tests** — all tests must pass before merging:
   ```
   pio test -e native_test -v
   ```
5. **Ensure it builds** for the target hardware:
   ```
   pio run -e KrabOS_TDeckPlus
   ```
6. **Push to your fork** and open a PR against the `main` branch.
7. In the PR description, reference the related issue (`Fixes #123`).
8. If you discovered new issues during testing, add them to `docs/KNOWN_ISSUES.md`.
9. Respond to review feedback promptly.

### After merging

Once the PR is merged, the maintainer closes the original issue with notes describing what was done and the outcome. This keeps the issue tracker clean and provides a record of the fix or feature for future reference.

### Who Can Merge

Only repository maintainers have merge access. Pull request authors and other
contributors cannot merge their own PRs. This keeps every merge subject to
review by someone familiar with the full codebase and release constraints.

### Merging

Pull requests are merged via `gh pr merge --squash --delete-branch`. Squash
merging keeps the commit history on `main` focused — each PR becomes one atomic
commit. Feature branches are deleted after merge.

### Hardware Testing

For any non-trivial change (anything beyond a typo, comment, or obvious one-line fix), the PR must be tested on actual hardware by a maintainer before merging. Do not merge AI agent or contributor PRs with substantive changes until hardware-tested. Small fixes can be merged on code review alone.

Before using a device, follow the authoritative
[`docs/HARDWARE_TESTING.md`](docs/HARDWARE_TESTING.md) six-phase protocol. Use
[`scripts/hw_test/CHECKLIST.md`](scripts/hw_test/CHECKLIST.md) to record the
commit, merged-image hash, fixture, commands, screenshots, feature-specific
coverage, and soak result. Hardware evidence must identify the exact build and
must distinguish physical interaction/RF proof from test-controller injection.

### Pull Request Guidelines

- **One feature / fix = one PR.** Smaller PRs are reviewed faster.
- Use descriptive commit messages:
  - Good: `Fix I2C timeout handling on ESP32`
  - Bad: `update`
- Keep the PR focused — don't sneak in unrelated changes.
- If you change public API or UI, update relevant documentation.
- New screens or features should include LVGL integration tests.

### PR Template

When opening a pull request, use this structure in the description:

```markdown
## Summary
<!-- Brief description of the change and why it's needed -->

Fixes #ISSUE_NUMBER

## Testing
<!-- REQUIRED: State how hardware testing was done -->

Testing method: [Remote test / Physical hardware test / Both]

Environment: [e.g. KrabOS_TDeckPlus]
Device/gateway/port: [T-Deck Plus fixture / gateway / immutable by-id path]
Merged image SHA-256: [hash]
RF parameters: [frequency/SF/BW/CR/power, or not used]
Evidence: [logs, screenshots, soak results, or explicit not-run reason]

<!--
Remote test = serial-controlled simulation using the applicable remote-test environment
Physical hardware test = flashed to a real T-Deck with observable device evidence
-->

## Checklist
- [ ] `pio test -e native_test -v` passes
- [ ] `pio run -e KrabOS_TDeckPlus` builds
- [ ] `pio run -e KrabOS_TDeckPlus_recovery` builds
- [ ] `docs/HARDWARE_TESTING.md` protocol completed for affected phases, or hardware testing is explicitly not applicable
- [ ] Home, Settings, and every changed screen were captured and visually inspected when firmware/UI changed
- [ ] Feature-specific physical/RF/persistence coverage and any test gaps are stated
- [ ] No new warnings or errors
- [ ] docs/KNOWN_ISSUES.md updated (if applicable)
```

### Protected Files

The following files require separate PRs and cannot be bundled with feature or bug fix PRs. Any PR touching these files will require CODEOWNER review and approval:

- `AGENTS.md`, `CLAUDE.md` — AI agent context
- `CONTRIBUTING.md` — contribution guidelines
- `docs/KNOWN_ISSUES.md` — known issues tracker
- `docs/MISSING_FEATURES.md` — missing features roadmap
- `.github/workflows/*` — CI/CD pipeline

If you need to change one of these, open a dedicated PR with only that protected
document or protected-document group. Do not bury protected file changes inside
a feature or bug-fix PR.

Workflow changes are stricter: every `.github/workflows/*` change must be in a
dedicated **workflow-only PR**. Do not combine workflow YAML with firmware,
tests, scripts, release evidence or documentation. The workflow PR must explain
its permissions and trigger changes and receive CODEOWNER review before merge.

## Development Workflow

```
upstream/main ─────► your-fork/main
                          │
                    git checkout -b feature/foo
                          │
                    commits...
                          │
                    push && open PR ──► upstream/main
```

- `main` is the integration and release branch — all PRs target it. Release
  automation binds artifacts and evidence to an exact commit on `main`.
- Rebase your branch on `main` before opening a PR to avoid merge conflicts:
  ```
  git fetch upstream
  git rebase upstream/main
  ```

## Style Guide

Follow the existing C++ style (a `.clang-format` file is tracked in the repo root):

| Convention | Rule |
|------------|------|
| Indentation | 2 spaces, no tabs |
| Functions & variables | `camelCase` |
| Class names | `UpperCamelCase` / `PascalCase` |
| Constants (`#define`) | `ALL_CAPS` |
| Line length | ~100 characters max |

Consistency with surrounding code takes priority over strict rules.

## Testing

```bash
# Run all tests (no hardware needed)
pio test -e native_test -v

# Run a specific module
pio test -e native_test -f test_battery -v
```

### Test structure

PlatformIO discovers tests in `test/test_<name>/` directories. Each suite needs its own directory with a `main.cpp` entry point — `test/unit/` or `test/integration/` is not discovered.

### Coverage expectations

Every module should include unit tests:

| Module | What to test |
|--------|-------------|
| HAL (battery, touch, keyboard, GPS) | ADC formulas, coordinate mapping, I2C protocol, NMEA parsing |
| Mesh wrapper | API contract, return value ranges, message queue |
| Navigation | Screen routing, back-stack logic, same-screen noop |
| Theme | Color contrast, brightness hierarchy, constant consistency |
| Build integration | All headers compile together without conflicts |

New screens or features should include at least basic integration tests (navigation routing, message display, widget lifecycle). Tests run on the native host — no hardware needed.

### Hardware Testing

[`docs/HARDWARE_TESTING.md`](docs/HARDWARE_TESTING.md) is the single source of
truth for on-device validation. Its canonical sequence is native tests, release
and remote-radio builds, smoke, full UI navigation, feature-specific checks,
and a pre-release soak. It also defines the Raspberry Pi gateway flow, serial
command dialects, screenshot capture requirements, recovery steps, and pass/fail
thresholds.

At minimum, a firmware-affecting PR must be built and flashed from its merged
image, reach a clean boot, navigate to Home and Settings, capture and visually
inspect both screens, and exercise the changed feature. Documentation-only and
native-test-only PRs may state that hardware testing is not applicable.

## Design Guide

The UI retains the upstream dark, compact visual language while KrabOS adds
larger touch targets and rounded primary surfaces. Key principles:

### Palette

| Role | Hex | Notes |
|------|-----|-------|
| Background (primary) | `#0F0F0F` | Deep black, not navy |
| Background (secondary) | `#181818` | Status bars, dialog backgrounds |
| Background (tertiary) | `#1E1E1E` | Card backgrounds, incoming bubbles |
| Background (input) | `#252525` | Text input fields |
| Accent | `#00BFFF` | Bright cyan, buttons and highlights |
| Text (primary) | `#F2F3F5` | Crisp white on dark |
| Text (secondary) | `#949BA4` | Subtitles, metadata |

Theme constants are defined in `src/ui/theme.h`.

### Layout

- Use shared theme helpers where a screen already follows the inherited pixel
  treatment; do not hardcode a new colour palette.
- KrabOS Home is a fixed **2x2 grid** with four large routes: CHATS, MAP,
  NETWORK and MORE. Its cards use an 8px radius and 1px resting border.
- The MORE hub uses rounded list rows. Existing specialist screens may retain
  zero-radius controls where that is already their established pattern.
- Touch, keyboard focus and trackball must expose the same actions.

### Typography

- Use LVGL's built-in Montserrat at small sizes (10px-14px) — it reads naturally blocky on the 320x240 TFT.
- Icon labels use LV_SYMBOL_* vector icons (FontAwesome bundle built into LVGL) — not emoji, not raw Unicode.
- Prefer `LV_SYMBOL_LIST` for the hamburger (≡) icon and `LV_SYMBOL_LEFT` for back (←) buttons over raw code points.

### Consistency

- All screens should call `apply_dark_bg()` and `apply_topbar_icon_btn()` from `src/ui/theme.h`.
- Screens share a `make_screen()` helper with a ← back button and dark top bar — use it instead of building custom top bars.
- When in doubt, match the style of an existing screen rather than introducing a new visual pattern.

## AI Use

AI agents and AI-assisted development are welcome — this project makes extensive use of them. However:

- All code generated by AI should be thoroughly reviewed by the contributor before submitting a PR.
- Changes should be tightly scoped to the bug fix or feature — no scope creep.
- The contributor must understand what the code does and how it fits into the broader firmware architecture.
- AI-generated code that introduces non-obvious dependencies, unsafe memory access patterns, or bypasses the theme/test conventions will be rejected.
- Use the [Code Audit Checklist](AGENTS.md#code-audit-checklist) to review your own work before submitting.

## License

By contributing, you agree that your contributions will be licensed under
the GPL-3.0-or-later license (same as the project).
