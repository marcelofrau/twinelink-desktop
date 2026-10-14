# desktop-startup-performance

## ADDED Requirements

### Requirement: Daemon service registration latency

The desktop daemon SHALL register its service and become reachable within 500 ms
on a warm start and within 2 s on a cold start after boot, measured with
per-line timestamped instrumentation.

#### Scenario: Warm start

- **WHEN** the daemon starts with the OS file cache warm
- **THEN** the service is registered and reachable within 500 ms

#### Scenario: Cold start

- **WHEN** the daemon starts for the first time after a boot
- **THEN** the service is registered and reachable within 2 s

### Requirement: Main window latency

Opening the main window SHALL take under 300 ms to become visible and under
800 ms to become interactive, without spawning a new process.

#### Scenario: Opening from the tray

- **WHEN** the user opens the main window from the tray in a running app
- **THEN** the window is visible within 300 ms
- **AND** the UI is interactive within 800 ms
- **AND** no new process is spawned

### Requirement: No forced Breeze on Windows

The desktop binaries SHALL NOT force the Breeze QStyle or the Breeze icon theme
on Windows, and SHALL NOT load Qt Quick Controls in processes that render no QML.

#### Scenario: Daemon startup on Windows

- **WHEN** the daemon starts on Windows
- **THEN** it does not load the Breeze style plugin
- **AND** it does not parse a Breeze icon theme
- **AND** it does not load Qt Quick Controls

### Requirement: Deferred non-critical initialization

The daemon SHALL register its DBus service before running non-critical
initialization, and SHALL defer plugin metadata scanning, notification server
initialization and link-provider discovery to an idle phase.

#### Scenario: Idle initialization

- **WHEN** the daemon has registered its service and the event loop is idle
- **THEN** plugin scan, notifications and discovery have run
- **AND** they did not delay service registration

### Requirement: Single resident process

The desktop SHALL host the daemon, the tray and the main window in one resident
process.

#### Scenario: Process inventory

- **WHEN** Twine is running with the window open
- **THEN** there is one Twine desktop process
