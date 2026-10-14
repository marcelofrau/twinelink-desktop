## Context

Upstream KDE Connect on Windows runs three heavyweight processes
(`kdeconnectd`, `kdeconnect-indicator`, `kdeconnect-app`), each a full Qt6 + KDE
Frameworks application that forces Breeze style and icon theme, uses DBus, and
performs blocking initialization at logon. The main window is spawned on demand.
Full evidence is in the documentation, "Startup performance".

## Goals / Non-Goals

- Goal: meet NFR-1..NFR-5 on Windows.
- Goal: keep the core UI-agnostic (DBus boundary) and the protocol unchanged.
- Non-goal: changing the wire protocol or breaking KDE Connect interop.
- Non-goal: a Linux/Plasma rewrite in this change.

## Decisions

### D1: Single resident process

Merge daemon + tray + window into one process. Opening the UI becomes showing a
window, not launching a process. Trade-off: a crash takes the tray with it; this
is mitigated by a supervisor/restart policy.

### D2: Shed KDE Frameworks on Windows

`KIOWidgets`, `Notifications`, `ColorScheme`, and Qt Quick Controls in the daemon
are replaced (QNetworkAccessManager, native toasts, native style, embedded
icons). Kept: `KConfig`, `KCoreAddons`, `KDBusAddons`, `KCrash`, `KI18n`.

### D3: Defer non-critical init

Register DBus first, then run plugin scan, notifications and discovery on an idle
timer. This protects the metric the user feels (time to reachable).

### D4: AOT + caches

`qmltc` for the UI, Qt Quick shader disk cache, LTCG + PGO.

## Alternatives considered

- **Keep three processes, prelaunch the app at logon.** Still pays DLL cold load
  and wastes RAM; rejected.
- **Move IPC off DBus entirely (named pipes).** Larger, deferred to a later
  change; DBus stays but its startup cost is reduced by ordering.
- **Static-linking Qt.** Strong win but high build complexity; tracked under the
  Windows build workstream.

## Risks / Trade-offs

- R-1: building Qt6/KF6 on Windows without Craft. Spiked before implementation.
- Single-process crash blast radius, mitigated by restart.

## Migration

No user-facing migration. Existing config and device identities are unchanged.

## Measurement

Per-line timestamped logs at: process start, Qt init, style/icon decision, DBus
registration, each deferred task, first window frame, first input. Recorded
before/after in the documentation table. No claim without a measurement.

## Open Questions

- Exact minimal KF6 module set that still builds cleanly on Windows.
