/*
 * SPDX-FileCopyrightText: 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QApplication>
#include <QIcon>
#include <QPointer>
#include <QProcess>
#include <QQuickStyle>
#include <QThread>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#ifdef Q_OS_WIN
#include <QSystemTrayIcon>
#else
#include <KStatusNotifierItem>
#endif

#include <KAboutData>
#include <KColorSchemeManager>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedString>

#include "dbusinterfaces/dbusinterfaces.h"
#include "deviceindicator.h"
#include "kdeconnect-version.h"
#include "models/devicesmodel.h"
#include "models/devicessortproxymodel.h"

#include <core/startuptrace.h>

#include <dbushelper.h>

#include "indicatorhelper.h"
#include "openconfig.h"

int main(int argc, char **argv)
{
    StartupTrace::mark("indicator: main() entered");

#ifdef Q_OS_WIN
    // If ran from a console, redirect the output there
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    QApplication app(argc, argv);
    StartupTrace::mark("indicator: QApplication constructed");

    IndicatorHelper helper;
    helper.startDaemon();
    StartupTrace::mark("indicator: daemon spawn requested");

    KAboutData aboutData(QStringLiteral("kdeconnect-indicator"),
                         i18n("KDE Connect Indicator"),
                         QStringLiteral(KDECONNECT_VERSION_STRING),
                         i18n("KDE Connect Indicator tool"),
                         KAboutLicense::GPL,
                         i18n("© 2016–2025 KDE Connect Team"));
    aboutData.addAuthor(i18n("Aleix Pol Gonzalez"), {}, QStringLiteral("aleixpol@kde.org"));
    aboutData.addAuthor(i18n("Albert Vaca Cintora"), {}, QStringLiteral("albertvaka@kde.org"));
    aboutData.setProgramLogo(QIcon::fromTheme(QStringLiteral("kdeconnect")));
    KAboutData::setApplicationData(aboutData);

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    // Ensure we have a suitable color theme set for light/dark mode. KColorSchemeManager implicitly applies
    // a suitable default theme.
    KColorSchemeManager::instance();
    StartupTrace::mark("indicator: color scheme manager initialized");
    // Force breeze style to ensure coloring works consistently in dark mode. Specifically tab colors have
    // troubles on windows.
    QApplication::setStyle(QStringLiteral("breeze"));
    // Force breeze icon theme to ensure we can correctly adapt icons to color changes WRT dark/light mode.
    // Without this we may end up with hicolor and fail to support icon recoloring.
    QIcon::setThemeName(QStringLiteral("breeze"));
    StartupTrace::mark("indicator: breeze style and icon theme applied");
#else
    QIcon::setFallbackThemeName(QStringLiteral("breeze"));
#endif

    // Default to org.kde.desktop style unless the user forces another style
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
    StartupTrace::mark("indicator: quick controls style set");

    KCrash::initialize();

    KDBusService dbusService(KDBusService::Unique);
    StartupTrace::mark("indicator: KDBusService acquired");

    DevicesModel model;
    model.setDisplayFilter(DevicesModel::Reachable | DevicesModel::Paired);
    DevicesSortProxyModel proxyModel;
    proxyModel.setSourceModel(&model);
    QMenu *menu = new QMenu;

    DaemonDbusInterface iface;
    StartupTrace::mark("indicator: device model constructed");

    auto refreshMenu = [&iface, &proxyModel, &menu]() {
        menu->clear();
        // Note: This is needed on macOS since a single click on the icon doesn't open the app like on other platforms.
        menu->addAction(i18n("Open app"), menu, []() {
            OpenConfig oc;
            oc.openConfiguration();
        });
        for (int i = 0, count = proxyModel.rowCount(); i < count; ++i) {
            QObject *deviceObject = proxyModel.data(proxyModel.index(i, 0), DevicesModel::DeviceRole).value<QObject *>();
            DeviceDbusInterface *device = qobject_cast<DeviceDbusInterface *>(deviceObject);
            if (device == nullptr)
                continue;

            auto indicator = new DeviceIndicator(device);
            QObject::connect(device, &DeviceDbusInterface::destroyed, indicator, &QObject::deleteLater);

            menu->addMenu(indicator);
        }
        const QStringList requests = iface.pairingRequests();
        if (!requests.isEmpty()) {
            menu->addSection(i18n("Pairing requests"));

            for (const auto &req : requests) {
                DeviceDbusInterface *dev = new DeviceDbusInterface(req, menu);
                auto pairMenu = menu->addMenu(dev->name());
                pairMenu->addAction(i18nc("Accept a pairing request", "Pair"), dev, &DeviceDbusInterface::acceptPairing);
                pairMenu->addAction(i18n("Reject"), dev, &DeviceDbusInterface::cancelPairing);
            }
        }
        // Add quit menu
#if defined(Q_OS_MAC)
        menu->addAction(i18n("Quit"), qApp, []() {
            const auto quitApplication = [](const QString &service) {
                const auto message = QDBusMessage::createMethodCall(service,
                                                                    QStringLiteral("/MainApplication"),
                                                                    QStringLiteral("org.qtproject.Qt.QCoreApplication"),
                                                                    QStringLiteral("quit"));
                QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
            };
            quitApplication(QStringLiteral("org.kde.kdeconnect.app"));
            quitApplication(QStringLiteral("org.kde.kdeconnect.daemon"));
            qApp->quit();
        });
#elif defined(Q_OS_WIN)
        menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), i18n("Quit"), qApp, &QApplication::quit);
#endif
    };

    QObject::connect(&iface, &DaemonDbusInterface::pairingRequestsChanged, &model, refreshMenu);
    QObject::connect(&model, &DevicesModel::rowsInserted, &model, refreshMenu);
    QObject::connect(&model, &DevicesModel::rowsRemoved, &model, refreshMenu);

    // Run icon to add icon path (if necessary)
    helper.iconPathHook();

#ifdef Q_OS_WIN
    QSystemTrayIcon systray;
    helper.systrayIconHook(systray);
    systray.setVisible(true);
    systray.setToolTip(QStringLiteral("KDE Connect"));
    QObject::connect(&model, &DevicesModel::rowsChanged, &model, [&systray, &model]() {
        systray.setToolTip(i18np("%1 device connected", "%1 devices connected", model.rowCount()));
    });
    QObject::connect(&systray, &QSystemTrayIcon::activated, &systray, [](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            OpenConfig oc;
            oc.openConfiguration();
        }
    });

    systray.setContextMenu(menu);
#else
    KStatusNotifierItem systray;
    helper.systrayIconHook(systray);
    systray.setToolTip(QStringLiteral("kdeconnect"), QStringLiteral("KDE Connect"), QStringLiteral("KDE Connect"));
    systray.setCategory(KStatusNotifierItem::Communications);
    systray.setStatus(KStatusNotifierItem::Passive);
    systray.setStandardActionsEnabled(false);
    QObject::connect(&model, &DevicesModel::rowsChanged, &model, [&systray, &model]() {
        const auto count = model.rowCount();
#ifndef Q_OS_MACOS // On MacOS, setting status to Active disables color theme syncing of the menu icon
        systray.setStatus(count == 0 ? KStatusNotifierItem::Passive : KStatusNotifierItem::Active);
#endif
        systray.setToolTip(QStringLiteral("kdeconnect"), QStringLiteral("KDE Connect"), i18np("%1 device connected", "%1 devices connected", count));
    });

    systray.setContextMenu(menu);
#endif
    StartupTrace::mark("indicator: tray icon visible");

    refreshMenu();
    StartupTrace::mark("indicator: tray menu built");

    app.setQuitOnLastWindowClosed(false);

    StartupTrace::mark("indicator: entering event loop");
    return app.exec();
}
