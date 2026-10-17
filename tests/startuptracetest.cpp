/**
 * SPDX-FileCopyrightText: 2026 twinelink contributors
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTest>

#include "core/startuptrace.h"

namespace {
/**
 * The line format is a contract: the measurement harness and the documentation
 * parse it, so it is asserted here instead of only written down.
 */
const QRegularExpression &linePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\[startup\\] wall=(\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}\\.\\d{3})"
                       " pid=(\\d+) proc=(\\S+) proc_ms=(\\d+|n/a) lib_us=(\\d+)"
                       " delta_us=(\\d+) phase=\"(.*)\"$"));
    return pattern;
}
}

class StartupTraceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void writesOneParseableLinePerPhase();
    void processAgeCoversTheLibraryLoad();
    void appendsInsteadOfTruncating();
    void honoursTheDisableSwitch();

private:
    QString readTrace() const;
    QStringList traceLines() const;

    QString m_traceFile;
};

void StartupTraceTest::init()
{
    const QString directory = QDir::tempPath() + QStringLiteral("/twinelink-startuptrace-")
        + QString::number(QRandomGenerator::global()->generate());
    QVERIFY(QDir().mkpath(directory));
    m_traceFile = directory + QStringLiteral("/trace.log");

    qputenv("TWINELINK_STARTUP_TRACE", "1");
    qputenv("TWINELINK_STARTUP_TRACE_FILE", m_traceFile.toUtf8());
}

void StartupTraceTest::cleanup()
{
    qunsetenv("TWINELINK_STARTUP_TRACE");
    qunsetenv("TWINELINK_STARTUP_TRACE_FILE");
    QFile::remove(m_traceFile);
    QDir().rmdir(QFileInfo(m_traceFile).absolutePath());
}

QString StartupTraceTest::readTrace() const
{
    QFile file(m_traceFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QStringList StartupTraceTest::traceLines() const
{
    return readTrace().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

void StartupTraceTest::writesOneParseableLinePerPhase()
{
    StartupTrace::mark("test: first phase");
    StartupTrace::mark("test: second phase");

    const QStringList lines = traceLines();
    QCOMPARE(lines.size(), 2);

    const QRegularExpressionMatch first = linePattern().match(lines.at(0));
    QVERIFY2(first.hasMatch(), qPrintable(lines.at(0)));
    QCOMPARE(first.captured(7), QStringLiteral("test: first phase"));
    // The process name tells several processes' traces apart in one file.
    QCOMPARE(first.captured(3), QStringLiteral("startuptracetest"));

    const QRegularExpressionMatch second = linePattern().match(lines.at(1));
    QVERIFY2(second.hasMatch(), qPrintable(lines.at(1)));
    QCOMPARE(second.captured(7), QStringLiteral("test: second phase"));

    // Timings move forward, and the delta of the second mark is measured from
    // the first mark, not from the start of the process.
    QVERIFY(second.captured(4).toLongLong() >= first.captured(4).toLongLong());
    QVERIFY(second.captured(5).toLongLong() > first.captured(5).toLongLong());
    QVERIFY(second.captured(6).toLongLong() < second.captured(5).toLongLong());
}

void StartupTraceTest::processAgeCoversTheLibraryLoad()
{
    StartupTrace::mark("test: invariant");

    const QStringList lines = traceLines();
    QCOMPARE(lines.size(), 1);
    const QRegularExpressionMatch match = linePattern().match(lines.at(0));
    QVERIFY2(match.hasMatch(), qPrintable(lines.at(0)));

    // Outside the process creation itself, loading the library is the earliest
    // thing that happens, so the OS-reported process age can never be smaller
    // than the time since the library was loaded.
    if (match.captured(4) == QStringLiteral("n/a")) {
        QSKIP("this platform does not report the process creation time");
    }
    const qint64 processAgeMicroseconds = match.captured(4).toLongLong() * 1000;
    const qint64 libraryAgeMicroseconds = match.captured(5).toLongLong();
    QVERIFY(processAgeMicroseconds >= libraryAgeMicroseconds - 1000);
}

void StartupTraceTest::appendsInsteadOfTruncating()
{
    StartupTrace::mark("test: before reopen");
    QVERIFY(readTrace().size() > 0);

    StartupTrace::mark("test: after reopen");

    const QStringList lines = traceLines();
    QCOMPARE(lines.size(), 2);
    QVERIFY(lines.at(0).endsWith(QStringLiteral("phase=\"test: before reopen\"")));
    QVERIFY(lines.at(1).endsWith(QStringLiteral("phase=\"test: after reopen\"")));
}

void StartupTraceTest::honoursTheDisableSwitch()
{
    QVERIFY(StartupTrace::isEnabled());
    StartupTrace::mark("test: traced");
    QCOMPARE(traceLines().size(), 1);

    qputenv("TWINELINK_STARTUP_TRACE", "0");
    QVERIFY(!StartupTrace::isEnabled());
    StartupTrace::mark("test: silenced");
    QCOMPARE(traceLines().size(), 1);

    qputenv("TWINELINK_STARTUP_TRACE", "1");
    QVERIFY(StartupTrace::isEnabled());
    StartupTrace::mark("test: traced again");
    const QStringList lines = traceLines();
    QCOMPARE(lines.size(), 2);
    QVERIFY(lines.at(1).endsWith(QStringLiteral("phase=\"test: traced again\"")));
}

QTEST_GUILESS_MAIN(StartupTraceTest)

#include "startuptracetest.moc"
