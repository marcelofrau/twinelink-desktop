/**
 * SPDX-FileCopyrightText: 2026 twinelink contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "startuptrace.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMutex>
#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

Q_LOGGING_CATEGORY(KDECONNECT_STARTUP, "kdeconnect.startup", QtInfoMsg)

namespace {
/**
 * Started during static initialization, i.e. while the shared library is being
 * loaded and before main() runs: this is the earliest point we can observe from
 * inside the process, and it makes the loader cost visible.
 */
struct EarlyClock
{
    EarlyClock() { timer.start(); }

    QElapsedTimer timer;
};

EarlyClock s_earlyClock;

/**
 * Age of this process according to the operating system. Unlike the early
 * clock, this includes process creation and library loading, so it is the
 * number that reflects what the user waits for.
 */
qint64 processAgeMs()
{
#ifdef Q_OS_WIN
    FILETIME creationTime {};
    FILETIME exitTime {};
    FILETIME kernelTime {};
    FILETIME userTime {};
    if (!GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        return -1;
    }

    ULARGE_INTEGER created;
    created.LowPart = creationTime.dwLowDateTime;
    created.HighPart = creationTime.dwHighDateTime;

    FILETIME nowFileTime {};
    GetSystemTimeAsFileTime(&nowFileTime);
    ULARGE_INTEGER now;
    now.LowPart = nowFileTime.dwLowDateTime;
    now.HighPart = nowFileTime.dwHighDateTime;

    // FILETIME counts 100 ns intervals.
    return static_cast<qint64>((now.QuadPart - created.QuadPart) / 10000ULL);
#else
    const QByteArray stat = QFile(QStringLiteral("/proc/self/stat")).readAll();
    // The second field (comm) may contain spaces and parentheses, so fields are
    // counted from the last ')' instead of by splitting the whole line.
    const int commEnd = stat.lastIndexOf(')');
    if (commEnd < 0) {
        return -1;
    }
    const QList<QByteArray> fields = stat.mid(commEnd + 2).trimmed().split(' ');
    // starttime is field 22, which is field 20 of the remainder.
    if (fields.size() < 20) {
        return -1;
    }
    bool ok = false;
    const qulonglong startTicks = fields.at(19).toULongLong(&ok);
    if (!ok) {
        return -1;
    }

    const QByteArray uptime = QFile(QStringLiteral("/proc/uptime")).readAll();
    if (uptime.isEmpty()) {
        return -1;
    }
    bool uptimeOk = false;
    const double uptimeSeconds = uptime.split(' ').constFirst().toDouble(&uptimeOk);
    if (!uptimeOk) {
        return -1;
    }

    const long ticksPerSecond = sysconf(_SC_CLK_TCK);
    if (ticksPerSecond <= 0) {
        return -1;
    }
    const double startedSeconds = static_cast<double>(startTicks) / ticksPerSecond;
    return static_cast<qint64>((uptimeSeconds - startedSeconds) * 1000.0);
#endif
}

QString processName()
{
#ifdef Q_OS_WIN
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) > 0) {
        return QFileInfo(QString::fromWCharArray(path)).baseName();
    }
#else
    const QString exe = QFile::symLinkTarget(QStringLiteral("/proc/self/exe"));
    if (!exe.isEmpty()) {
        return QFileInfo(exe).baseName();
    }
#endif
    return QStringLiteral("unknown");
}

struct TraceConfig
{
    bool enabled = false;
    QString filePath;
};

/**
 * Read on every mark: it costs a getenv, and it keeps the switch usable from
 * tests and from a wrapper that sets the environment late.
 */
TraceConfig readConfig()
{
    TraceConfig config;
    config.enabled = qgetenv("TWINELINK_STARTUP_TRACE") != QByteArrayLiteral("0");
    if (config.enabled) {
        config.filePath = qEnvironmentVariable("TWINELINK_STARTUP_TRACE_FILE");
    }
    return config;
}

QMutex &traceMutex()
{
    static QMutex mutex;
    return mutex;
}

/**
 * Keeps the trace file open (flushed per line) and reopens it if the configured
 * path changes. Failure is not fatal: the "kdeconnect.startup" logging category
 * still carries the trace.
 */
QFile *traceFile(const QString &path)
{
    struct TraceFile
    {
        QFile file;
        QString path;

        void open(const QString &newPath)
        {
            if (file.isOpen() && path == newPath) {
                return;
            }
            if (file.isOpen()) {
                file.close();
            }
            path = newPath;
            file.setFileName(path);
            const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
            Q_UNUSED(opened)
        }
    };

    static TraceFile traceFile;
    if (path.isEmpty()) {
        return nullptr;
    }
    traceFile.open(path);
    return traceFile.file.isOpen() ? &traceFile.file : nullptr;
}

qint64 s_lastMarkMicroseconds = -1;
}

void StartupTrace::mark(const char *phase)
{
    const TraceConfig traceConfig = readConfig();
    if (!traceConfig.enabled) {
        return;
    }

    // Sample the clock before doing any work, so that the cost of the trace
    // itself is not charged to the phase being measured.
    const qint64 microseconds = s_earlyClock.timer.nsecsElapsed() / 1000;
    const qint64 deltaMicroseconds
        = s_lastMarkMicroseconds < 0 ? microseconds : microseconds - s_lastMarkMicroseconds;
    const qint64 ageMs = processAgeMs();

    const QString line
        = QStringLiteral(
              "[startup] wall=%1 pid=%2 proc=%3 proc_ms=%4 lib_us=%5 delta_us=%6 phase=\"%7\"")
              .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz")),
                   QString::number(QCoreApplication::applicationPid()),
                   processName())
              .arg(ageMs < 0 ? QStringLiteral("n/a") : QString::number(ageMs))
              .arg(microseconds)
              .arg(deltaMicroseconds)
              .arg(QString::fromLatin1(phase));

    QMutexLocker locker(&traceMutex());
    s_lastMarkMicroseconds = microseconds;

    qCInfo(KDECONNECT_STARTUP, "%s", qUtf8Printable(line));

    if (QFile *file = traceFile(traceConfig.filePath)) {
        // One write call: several processes may append to the same trace file,
        // and a line must not be torn by their writes interleaving.
        file->write(QByteArray(line.toUtf8() + '\n'));
        file->flush();
    }
}

bool StartupTrace::isEnabled()
{
    return readConfig().enabled;
}
