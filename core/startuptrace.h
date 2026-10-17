/**
 * SPDX-FileCopyrightText: 2026 twinelink contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef STARTUPTRACE_H
#define STARTUPTRACE_H

#include <QLoggingCategory>
#include <QtGlobal>

// The trace lives in kdeconnectcore, which every desktop process links. The UI
// process deliberately does not link kdeconnectcore (it must stay slim), so it
// compiles its own copy of the implementation instead and defines
// KDECONNECT_STARTUPTRACE_STATIC.
#if defined(KDECONNECT_STARTUPTRACE_STATIC)
#define STARTUPTRACE_EXPORT
#else
#include "kdeconnectcore_export.h"
#define STARTUPTRACE_EXPORT KDECONNECTCORE_EXPORT
#endif

/**
 * Per-line startup timing instrumentation.
 *
 * Startup latency is a product requirement, so it is measured instead of
 * guessed: every participating process records named phases with a wall-clock
 * timestamp, the age of the process at that point, and the time since the
 * previous phase. See the "windows-startup-perf" change and the documentation
 * page "Startup performance".
 *
 * The trace is enabled by default and can be turned off with
 * TWINELINK_STARTUP_TRACE=0. Lines go to the "kdeconnect.startup" logging
 * category (enabled at info level by default) and, when
 * TWINELINK_STARTUP_TRACE_FILE points at a writable path, are also appended
 * there and flushed immediately so that a hung or killed process still leaves
 * a complete trace behind.
 */
Q_DECLARE_LOGGING_CATEGORY(KDECONNECT_STARTUP)

namespace StartupTrace {
/**
 * Logs one startup phase, e.g. StartupTrace::mark("daemon: DBus service registered").
 * The phase name is a compile-time string literal and is stored, not copied.
 */
STARTUPTRACE_EXPORT void mark(const char *phase);

/** Whether the trace is active in this process. */
STARTUPTRACE_EXPORT bool isEnabled();
}

#endif // STARTUPTRACE_H
