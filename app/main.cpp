/**
 * SPDX-FileCopyrightText: 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QEvent>
#include <QIcon>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QWindow>

#include <memory>

#include "kdeconnect-version.h"
#include <KAboutData>
#include <KColorSchemeManager>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KWindowSystem>

#include <core/startuptrace.h>

#if defined(Q_OS_MAC)
#include "macosforegroundapp.h"
#endif

namespace {
/**
 * Reports the first input the process handles: the second user-visible
 * milestone of the UI process, after the first frame.
 */
class FirstInputFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (m_armed) {
            switch (event->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::KeyPress:
            case QEvent::TouchBegin:
                m_armed = false;
                StartupTrace::mark("app: first input handled");
                break;
            default:
                break;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    bool m_armed = true;
};

/**
 * Instruments the window the user actually waits for. frameSwapped() is emitted
 * while presenting, so the mark is queued onto the GUI thread; the guard keeps
 * it to a single mark even when several frames are already queued by the time
 * the handler first runs.
 */
void instrumentFirstFrame(QWindow *window)
{
    auto *quickWindow = qobject_cast<QQuickWindow *>(window);
    if (!quickWindow) {
        return;
    }

    auto reported = std::make_shared<bool>(false);
    QObject::connect(
        quickWindow,
        &QQuickWindow::frameSwapped,
        quickWindow,
        [reported]() {
            if (*reported) {
                return;
            }
            *reported = true;
            StartupTrace::mark("app: first frame presented");
        },
        Qt::QueuedConnection);
}

void instrumentFirstInput()
{
    qApp->installEventFilter(new FirstInputFilter(qApp));
}
}

static void raiseWindow(QWindow *window)
{
#if defined(Q_OS_MAC)
    activateWindowForMacOS(window);
#else
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
#endif
}

int main(int argc, char *argv[])
{
    StartupTrace::mark("app: main() entered");

    QApplication app(argc, argv);
    StartupTrace::mark("app: QApplication constructed");

    KLocalizedString::setApplicationDomain("kdeconnect-app");
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("kdeconnect")));
    KAboutData aboutData(QStringLiteral("kdeconnect.app"),
                         i18n("KDE Connect"),
                         QStringLiteral(KDECONNECT_VERSION_STRING),
                         i18n("KDE Connect"),
                         KAboutLicense::GPL,
                         i18n("© 2015–2025 KDE Connect Team"));
    aboutData.addAuthor(i18n("Aleix Pol Gonzalez"), {}, QStringLiteral("aleixpol@kde.org"));
    aboutData.addAuthor(i18n("Albert Vaca Cintora"), {}, QStringLiteral("albertvaka@kde.org"));
    aboutData.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"), i18nc("EMAIL OF TRANSLATORS", "Your emails"));
    aboutData.setBugAddress("https://bugs.kde.org/enter_bug.cgi?product=kdeconnect&component=common");
    aboutData.setProgramLogo(QIcon::fromTheme(QStringLiteral("kdeconnect")));
    KAboutData::setApplicationData(aboutData);
    StartupTrace::mark("app: application metadata set (icons resolved from theme)");

    KCrash::initialize();

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    // Ensure we have a suitable color theme set for light/dark mode. KColorSchemeManager implicitly applies
    // a suitable default theme.
    KColorSchemeManager::instance();
    StartupTrace::mark("app: color scheme manager initialized");
    // Force breeze style to ensure coloring works consistently in dark mode. Specifically tab colors have
    // troubles on windows.
    QApplication::setStyle(QStringLiteral("breeze"));
    // Force breeze icon theme to ensure we can correctly adapt icons to color changes WRT dark/light mode.
    // Without this we may end up with hicolor and fail to support icon recoloring.
    QIcon::setThemeName(QStringLiteral("breeze"));
    StartupTrace::mark("app: breeze style and icon theme applied");
#else
    QIcon::setFallbackThemeName(QStringLiteral("breeze"));
#endif

    // Default to org.kde.desktop style unless the user forces another style
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
    StartupTrace::mark("app: quick controls style set");

    QString device;
    QString config;
    bool replace = false;
    {
        QCommandLineParser parser;
        QCommandLineOption deviceOption(QStringLiteral("device"), i18nc("@info:shell", "Device id to open the app in"), QStringLiteral("device-id"));
        parser.addOption(deviceOption);
        QCommandLineOption configOption(QStringLiteral("plugin-config"),
                                        i18nc("@info:shell", "Configuration module to show, requires --device to be set."),
                                        QStringLiteral("plugin-id"));
        parser.addOption(configOption);

        QCommandLineOption replaceOption({QStringLiteral("replace")}, i18nc("command line description", "Replace an existing instance"));
        parser.addOption(replaceOption);

        parser.addPositionalArgument(QStringLiteral("url"), i18n("URL to share"));
        aboutData.setupCommandLine(&parser);
        parser.process(app);
        aboutData.processCommandLine(&parser);
        if (parser.positionalArguments().count() == 1) {
            const QString urlToShare = parser.positionalArguments().constFirst();
            const QString kdeconnectHandlerExecutable =
                QStandardPaths::findExecutable(QStringLiteral("kdeconnect-handler"), {QCoreApplication::applicationDirPath()});
            if (!kdeconnectHandlerExecutable.isEmpty()) {
                QProcess::startDetached(kdeconnectHandlerExecutable, {urlToShare});
                return 0; // exit the app once kdeconnect-handler is started
            }
        }
        replace = parser.isSet(replaceOption);
        if (parser.isSet(deviceOption)) {
            device = parser.value(deviceOption);
            replace = true;
        }
        if (parser.isSet(configOption)) {
            config = parser.value(configOption);
        }
    }
    StartupTrace::mark("app: command line parsed");

    KDBusService::StartupOptions flags = KDBusService::Unique;
    if (replace) {
        flags |= KDBusService::Replace;
    }
    KDBusService dbusService(flags);
    StartupTrace::mark("app: KDBusService acquired");

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("org.kde.kdeconnect.app", "Main");
    StartupTrace::mark("app: QML module loaded");

    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load the app" << engine.hasError();
        return 1;
    }
    auto obj = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    instrumentFirstFrame(obj);
    instrumentFirstInput();

    if (!device.isEmpty()) {
        auto mo = obj->metaObject();
        mo->invokeMethod(obj, "openDevice", Qt::QueuedConnection, Q_ARG(QVariant, device), Q_ARG(QVariant, config));
    }
    raiseWindow(obj);
    QObject::connect(&dbusService, &KDBusService::activateRequested, obj, [obj](const QStringList & /*arguments*/, const QString & /*workingDirectory*/) {
        raiseWindow(obj);
    });
    StartupTrace::mark("app: entering event loop");
    return app.exec();
}
