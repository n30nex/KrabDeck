# Contributing to SlopOS T-Deck

Thanks for considering contributing! This project is in beta and welcomes
bug reports, feature requests, and code contributions.

## Quick Start

```bash
git clone --recurse-submodules https://github.com/hermes-gadget/SlopOS-tdeck.git
cd SlopOS-tdeck
pio test -e native_test -v
```

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

### For small fixes (typos, comments, minor bug fixes)

Open a pull request directly — no prior issue needed.

### For larger changes or new features

1. **Open an issue first** to discuss the approach with maintainers.
2. Wait for consensus before investing significant work.

### Pull Request Process

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

All new code should include unit tests. See `test/` for examples.

## License

By contributing, you agree that your contributions will be licensed under
the GPL-3.0-or-later license (same as the project).
