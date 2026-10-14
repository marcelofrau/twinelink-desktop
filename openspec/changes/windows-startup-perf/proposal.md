## Why

On Windows the desktop client is slow to start. The daemon, tray and window are
three heavyweight Qt6 + KDE Frameworks processes; Breeze style and icon theme are
forced even in the daemon; DBus and discovery do blocking work at logon; and the
main window is a fresh process spawned on demand. This directly undermines the
product goal of a fast, modern UX (NFR-1..NFR-5).

## What Changes

- Add a Windows Defender exclusion for the install directory and code-sign the
  binaries.
- Stop forcing the Breeze style and icon theme on Windows; use the native style
  and embedded QRC icons.
- Remove `QQuickStyle::setStyle` from processes that render no QML.
- Slim the daemon: use `QCoreApplication`/`QGuiApplication` and drop
  `KIOWidgets`, `Notifications`, `ColorScheme`, `QuickControls2`, `Widgets`.
- Defer plugin metadata scan, notification init and discovery to an idle timer;
  register the DBus service first.
- **BREAKING** (internal): collapse the daemon, tray and window into a single
  resident process, so opening the UI costs no process spawn.
- Precompile the UI with `qmltc`, enable the Qt Quick shader disk cache, and
  build with LTCG + PGO.

## Capabilities

### New Capabilities

- `desktop-startup-performance`: startup latency targets and the mechanisms that
  guarantee them for the desktop daemon and UI.

### Modified Capabilities

<!-- none: new fork, no existing specs -->

## Impact

- Code: `daemon/`, `app/`, `indicator/`, `core/` startup paths, top-level
  `CMakeLists.txt`.
- Build/packaging: Windows build (off Craft), installer, signing.
- Dependencies: removal of several KDE Frameworks from the Windows target.
- No protocol or wire-format changes.
