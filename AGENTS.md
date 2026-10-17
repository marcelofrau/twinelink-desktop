# AGENTS.md - twinelink-desktop

The twinelink desktop client - a hard fork of KDE Connect (`kdeconnect-kde`).
Windows-first. Stack: C++20, Qt 6, a minimal set of KDE Frameworks 6,
CMake + Ninja (built off Craft on Windows).
The core is UI-agnostic and exposed via DBus; the UI is Qt Quick
(no Kirigami/Plasma). Protocol identifiers stay `kdeconnect.*`.

## Upstream
`upstream` remote = KDE/kdeconnect-kde. This is a hard fork: do NOT open
upstream merge requests (KDE forbids AI-engineered MRs). Pull security fixes
manually.

## Build & test (Windows)
Full recipe in `docs/development/build-desktop.md` (docs repo). Quick start:
```
. C:\CraftRoot\craft\craftenv.ps1
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\CraftRoot -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build
```
Binaries land in `build/bin`. Format: `clang-format` 18 — only the lines a
change touches are enforced (`git clang-format <base>`; the CI job does the
same). The fork keeps upstream KDE formatting, which is not clang-format clean
as a whole, so never reformat a file you are only touching in one place.
Static analysis: `clang-tidy`, `cppcheck`.

## Continuity
See `twinelink-docs/docs/plan/session-log.md`.

## Working rules (all twinelink repositories)
- Everything committed is in English (docs, code, comments, commits, PRs).
  The maintainer chats in Portuguese and uses a terse, compressed style.
- OpenSpec-first: every feature/fix is an OpenSpec change (proposal, specs,
  design, tasks) approved before code is written.
- Branches `feat|fix|chore`; Conventional Commits; SOLID; unit tests for new logic.
- Lint/format/static analysis per language; CI must stay green.
- Do NOT rename KDE Connect protocol identifiers (`kdeconnect.*`), the config
  directory or the DBus service (interop with stock KDE Connect).
- No performance claim without a measurement method.