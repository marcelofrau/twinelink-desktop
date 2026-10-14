## 1. Measurement harness

- [ ] 1.1 Add per-line timestamped startup logging to the daemon
- [ ] 1.2 Add the same to the UI process (first frame, first input)
- [ ] 1.3 Record baseline cold/warm numbers on real Windows hardware

## 2. Cheap wins

- [ ] 2.1 Windows Defender exclusion for the install dir; document signing
- [ ] 2.2 Do not force Breeze style/icon theme on Windows
- [ ] 2.3 Remove `QQuickStyle::setStyle` from daemon and indicator
- [ ] 2.4 Use embedded QRC icons instead of theme lookups

## 3. Slim the daemon

- [ ] 3.1 Switch to `QCoreApplication`/`QGuiApplication`
- [ ] 3.2 Drop `KIOWidgets`; replace I/O with `QNetworkAccessManager`/`QFile`
- [ ] 3.3 Drop `Notifications`, `ColorScheme`, `QuickControls2`, `Widgets`
- [ ] 3.4 Verify build links only the minimal KF6 set

## 4. Defer initialization

- [ ] 4.1 Register DBus service first
- [ ] 4.2 Move plugin scan, notifications, discovery to an idle timer
- [ ] 4.3 Verify service registration is not delayed

## 5. Single resident process

- [ ] 5.1 Host tray and window in the daemon process
- [ ] 5.2 Remove the app-spawn path
- [ ] 5.3 Add a supervisor/restart policy

## 6. Build performance

- [ ] 6.1 `qmltc` for the UI
- [ ] 6.2 Qt Quick shader disk cache
- [ ] 6.3 LTCG + PGO build

## 7. Verification

- [ ] 7.1 Re-measure cold/warm; meet NFR-1..NFR-5
- [ ] 7.2 Update the documentation table with before/after
- [ ] 7.3 `openspec validate windows-startup-perf`
