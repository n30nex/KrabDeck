# Contributing to SlopOS T-Deck

Thanks for considering contributing! This project is in beta and welcomes
bug reports, feature requests, and code contributions.

## Quick Start

```bash
git clone --recurse-submodules https://github.com/hermes-gadget/SlopOS-tdeck.git
cd SlopOS-tdeck
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

1. Search [open issues](https://github.com/hermes-gadget/SlopOS-tdeck/issues)
   first — your bug may already be known.
2. If not found, open a new issue with:
   - A clear, descriptive title
   - Steps to reproduce (exact and minimal)
   - Expected vs. actual behavior
   - T-Deck hardware revision (v1.0 / v1.1), if known
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

### Work on it

1. **Fork the repo** on GitHub.
2. **Create a branch on your fork** from the `dev` branch:
   ```
   git checkout dev
   git pull upstream dev
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
   pio run -e SlopOS_TDeck
   ```
6. **Push to your fork** and open a PR against the `dev` branch.
7. In the PR description, reference the related issue (`Fixes #123`).
8. Respond to review feedback promptly.

### After merging

Once the PR is merged, the maintainer closes the original issue with notes describing what was done and the outcome. This keeps the issue tracker clean and provides a record of the fix or feature for future reference.

### Merging

Pull requests are merged via `gh pr merge --squash --delete-branch`. Squash merging keeps the commit history on `dev` clean — each PR becomes one atomic commit. Feature branches are deleted after merge.

### Pull Request Guidelines

- **One feature / fix = one PR.** Smaller PRs are reviewed faster.
- Use descriptive commit messages:
  - Good: `Fix I2C timeout handling on ESP32`
  - Bad: `update`
- Keep the PR focused — don't sneak in unrelated changes.
- If you change public API or UI, update relevant documentation.
- New screens or features should include LVGL integration tests.

## Development Workflow

```
upstream/dev  ─────►  your-fork/dev
                          │
                    git checkout -b feature/foo
                          │
                    commits...
                          │
                    push && open PR ──► upstream/dev
```

- The `dev` branch is the integration branch — all PRs merge here.
- The `main` branch contains stable releases only.
- Rebase your branch on `dev` before opening a PR to avoid merge conflicts:
  ```
  git fetch upstream
  git rebase upstream/dev
  ```

## Style Guide

Follow the existing C++ style (`.clang-format` is checked in):

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

## Design Guide

The UI follows a **pixel / blocky retro aesthetic** inspired by Discord's dark theme on an 8-bit display. Key principles:

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

- **Zero radius** everywhere — no pill buttons, no round cards. Use `lv_obj_set_style_radius(obj, 0, 0)`.
- **2px minimum borders** — cards, inputs, and buttons get thick solid outlines from the palette.
- **4-column grid** on the home screen (320px T-Deck), no scrolling, 12 tiles per page. Smaller displays auto-adapt via `src/ui/responsive.h`.

### Typography

- Use LVGL's built-in Montserrat at small sizes (10px-14px) — it reads naturally blocky on the 320x240 TFT.
- Icon labels use LV_SYMBOL_* vector icons (FontAwesome bundle built into LVGL) — not emoji, not raw Unicode.
- Prefer `LV_SYMBOL_LIST` for the hamburger (≡) icon and `LV_SYMBOL_LEFT` for back (←) buttons over raw code points.

### Consistency

- All screens should call `apply_dark_bg()` and `apply_topbar_icon_btn()` from `src/ui/theme.h`.
- Screens share a `make_screen()` helper with a ← back button and dark top bar — use it instead of building custom top bars.
- When in doubt, match the style of an existing screen rather than introducing a new visual pattern.

## License

By contributing, you agree that your contributions will be licensed under
the GPL-3.0-or-later license (same as the project).
