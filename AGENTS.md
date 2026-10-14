# AGENTS.md - twinelink-desktop

The Twine desktop client - a hard fork of KDE Connect (`kdeconnect-kde`).
Windows-first. Stack: C++20, Qt 6, a minimal set of KDE Frameworks 6,
CMake + Ninja (built off Craft on Windows).
The core is UI-agnostic and exposed via DBus; the UI is Qt Quick
(no Kirigami/Plasma). Protocol identifiers stay `kdeconnect.*`.

## Upstream
`upstream` remote = KDE/kdeconnect-kde. This is a hard fork: do NOT open
upstream merge requests (KDE forbids AI-engineered MRs). Pull security fixes
manually.

## Build & test
- Configure: `cmake -S . -B build -G Ninja` (dependencies via vcpkg/Craft).
- Build: `cmake --build build`.
- Format: `clang-format`. Static analysis: `clang-tidy`, `cppcheck`.
- Tests: `ctest` (Qt Test).

## Continuity
See `twinelink-docs/docs/plan/session-log.md`.
## Working rules (all Twine repositories)
- Everything committed is in English (docs, code, comments, commits, PRs).
  The maintainer chats in Portuguese and uses a terse, compressed style.
- OpenSpec-first: every feature/fix is an OpenSpec change (proposal, specs,
  design, tasks) approved before code is written.
- Branches `feat|fix|chore`; Conventional Commits; SOLID; unit tests for new logic.
- Lint/format/static analysis per language; CI must stay green.
- Do NOT rename KDE Connect protocol identifiers (`kdeconnect.*`), the config
  directory or the DBus service (interop with stock KDE Connect).
- No performance claim without a measurement method.
