// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <chrono>

#include <QtCore/QtCore>

#include <QtAppManMain/configuration.h>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM // ADL for op==

// we only need this operator== for the test here
inline bool operator==(const ConfigurationData::Wayland::ExtraSocket &wes1,
                       const ConfigurationData::Wayland::ExtraSocket &wes2)
{
    return (wes1.path == wes2.path)
           && (wes1.permissions == wes2.permissions)
           && (wes1.userId == wes2.userId)
           && (wes1.groupId == wes2.groupId);
}

QT_END_NAMESPACE_AM

#include <QtTest/QtTest>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_Configuration : public QObject
{
    Q_OBJECT

public:
    tst_Configuration();

private Q_SLOTS:
    void defaultConfig();
    void simpleConfig();
    void mergedConfig();
    void commandLineConfig();

private:
    QDir pwd;
};


tst_Configuration::tst_Configuration()
{ }

void tst_Configuration::defaultConfig()
{
    Configuration c;
    c.parseWithArguments({ u"test"_s, u"--no-cache"_s });

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), true); // legacy
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), u""_s);
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.yaml.ui.mainQml, u""_s);

    QCOMPARE(c.yaml.applications.builtinAppsManifestDir, {});
    QCOMPARE(c.yaml.applications.documentDir, u""_s);

    QCOMPARE(c.yaml.applications.installationDir, u""_s);
    QCOMPARE(c.yaml.installer.disable, false);
    QCOMPARE(c.yaml.intents.disable, false);
    QCOMPARE(c.yaml.intents.timeouts.disambiguation.count(), 10000);
    QCOMPARE(c.yaml.intents.timeouts.startApplication.count(), 3000);
    QCOMPARE(c.yaml.intents.timeouts.replyFromApplication.count(), 5000);
    QCOMPARE(c.yaml.intents.timeouts.replyFromSystem.count(), 20000);

    QCOMPARE(c.yaml.ui.fullscreen, false);
    QCOMPARE(c.yaml.ui.windowIcon, u""_s);
    QCOMPARE(c.yaml.ui.importPaths, {});
    QCOMPARE(c.yaml.ui.pluginPaths, {});
    QCOMPARE(c.yaml.ui.loadDummyData, false);
    QCOMPARE(c.yaml.flags.noSecurity, false);
    QCOMPARE(c.yaml.flags.developmentMode, false);
    QCOMPARE(c.yaml.flags.allowUnsignedPackages, false);
    QCOMPARE(c.yaml.flags.allowUnknownUiClients, false);
    QCOMPARE(c.yaml.flags.forceSingleProcess, false);
    QCOMPARE(c.yaml.flags.forceMultiProcess, false);
    QCOMPARE(c.yaml.logging.rules, {});
    QCOMPARE(c.yaml.logging.messagePattern, u""_s);
    QCOMPARE(c.yaml.logging.useAMConsoleLogger, QVariant());
    QCOMPARE(c.yaml.ui.style, u""_s);
    QCOMPARE(c.yaml.ui.iconThemeName, u""_s);
    QCOMPARE(c.yaml.ui.iconThemeSearchPaths, {});
    QCOMPARE(c.yaml.logging.dlt.id, u""_s);
    QCOMPARE(c.yaml.logging.dlt.description, u""_s);
    QCOMPARE(c.yaml.ui.resources, {});

    QCOMPARE(c.yaml.ui.opengl.desktopProfile, u""_s);
    QCOMPARE(c.yaml.ui.opengl.esMajorVersion, -1);
    QCOMPARE(c.yaml.ui.opengl.esMinorVersion, -1);

    QCOMPARE(c.yaml.containers.selection, {});
    QCOMPARE(c.yaml.containers.configurations, QVariantMap {});
    QCOMPARE(c.yaml.runtimes.additionalLaunchers, QStringList {});
    QCOMPARE(c.yaml.runtimes.configurations, QVariantMap {});

    QVERIFY(c.yaml.dbus.policies.isEmpty());
    QVERIFY(c.yaml.dbus.registrations.isEmpty());

    QCOMPARE(c.yaml.systemProperties, QVariantMap {});

    QCOMPARE(c.yaml.quicklaunch.idleLoad, qreal(0));
    QVERIFY(c.yaml.quicklaunch.runtimesPerContainer.isEmpty());

    QCOMPARE(c.yaml.wayland.socketName, u""_s);
    QVERIFY(c.yaml.wayland.extraSockets.isEmpty());

    QCOMPARE(c.yaml.crashAction.printBacktrace, true);
    QCOMPARE(c.yaml.crashAction.printQmlStack, true);
    QCOMPARE(c.yaml.crashAction.waitForGdbAttach.count(), 0);
    QCOMPARE(c.yaml.crashAction.dumpCore, true);
    QCOMPARE(c.yaml.crashAction.stackFramesToIgnore.onCrash, -1);
    QCOMPARE(c.yaml.crashAction.stackFramesToIgnore.onException, -1);

    QCOMPARE(c.yaml.installer.caCertificates, {});

    QCOMPARE(c.yaml.plugins.container, {});
    QCOMPARE(c.yaml.plugins.startup, {});

    QCOMPARE(c.yaml.watchdog.disable, false);
    QCOMPARE(c.yaml.watchdog.eventloop.checkInterval, 1s);
    QCOMPARE(c.yaml.watchdog.eventloop.warnTimeout, 1s);
    QCOMPARE(c.yaml.watchdog.eventloop.killTimeout, 10s);
    QCOMPARE(c.yaml.watchdog.quickwindow.checkInterval, 1s);
    QCOMPARE(c.yaml.watchdog.quickwindow.syncWarnTimeout, 35ms);
    QCOMPARE(c.yaml.watchdog.quickwindow.syncKillTimeout, 10s);
    QCOMPARE(c.yaml.watchdog.quickwindow.renderWarnTimeout, 35ms);
    QCOMPARE(c.yaml.watchdog.quickwindow.renderKillTimeout, 10s);
    QCOMPARE(c.yaml.watchdog.quickwindow.swapWarnTimeout, 35ms);
    QCOMPARE(c.yaml.watchdog.quickwindow.swapKillTimeout, 10s);
    QCOMPARE(c.yaml.watchdog.wayland.checkInterval, 5s);
    QCOMPARE(c.yaml.watchdog.wayland.warnTimeout, 1s);
    QCOMPARE(c.yaml.watchdog.wayland.killTimeout, 10s);
}

void tst_Configuration::simpleConfig()
{
    Configuration c({ u":/data/config1.yaml"_s }, u":/build-config.yaml"_s);
    QVERIFY_THROWS_NO_EXCEPTION(c.parseWithArguments({ u"test"_s, u"--no-cache"_s }));

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), false); // legacy
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), u""_s);
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.yaml.ui.mainQml, u"main.qml"_s);

    QCOMPARE(c.yaml.applications.builtinAppsManifestDir, { u"builtin-dir"_s });
    QCOMPARE(c.yaml.applications.documentDir, u"doc-dir"_s);

    QCOMPARE(c.yaml.applications.installationDir, u"installation-dir"_s);
    QCOMPARE(c.yaml.installer.disable, true);
    QCOMPARE(c.yaml.intents.disable, true);
    QCOMPARE(c.yaml.intents.timeouts.disambiguation.count(), 1);
    QCOMPARE(c.yaml.intents.timeouts.startApplication.count(), 2);
    QCOMPARE(c.yaml.intents.timeouts.replyFromApplication.count(), 3);
    QCOMPARE(c.yaml.intents.timeouts.replyFromSystem.count(), 4);

    QCOMPARE(c.yaml.ui.fullscreen, true);
    QCOMPARE(c.yaml.ui.windowIcon, u"icon.png"_s);
    QCOMPARE(c.yaml.ui.importPaths, QStringList({ pwd.absoluteFilePath(u"ip1"_s), pwd.absoluteFilePath(u"ip2"_s) }));
    QCOMPARE(c.yaml.ui.pluginPaths, QStringList({ u"pp1"_s, u"pp2"_s }));
    QCOMPARE(c.yaml.ui.loadDummyData, true);
    QCOMPARE(c.yaml.flags.noSecurity, true);
    QCOMPARE(c.yaml.flags.developmentMode, true);
    QCOMPARE(c.yaml.flags.allowUnsignedPackages, true);
    QCOMPARE(c.yaml.flags.allowUnknownUiClients, true);
    QCOMPARE(c.yaml.flags.forceSingleProcess, true);
    QCOMPARE(c.yaml.flags.forceMultiProcess, true);
    QCOMPARE(c.yaml.logging.rules, QStringList({ u"lr1"_s, u"lr2"_s }));
    QCOMPARE(c.yaml.logging.messagePattern, u"msgPattern"_s);
    QCOMPARE(c.yaml.logging.useAMConsoleLogger, QVariant(true));
    QCOMPARE(c.yaml.ui.style, u"mystyle"_s);
    QCOMPARE(c.yaml.ui.iconThemeName, u"mytheme"_s);
    QCOMPARE(c.yaml.ui.iconThemeSearchPaths, QStringList({ u"itsp1"_s, u"itsp2"_s }));
    QCOMPARE(c.yaml.logging.dlt.id, u"dltid"_s);
    QCOMPARE(c.yaml.logging.dlt.description, u"dltdesc"_s);
    QCOMPARE(c.yaml.logging.dlt.longMessageBehavior, u"split"_s);
    QCOMPARE(c.yaml.ui.resources, QStringList({ u"r1"_s, u"r2"_s }));

    QCOMPARE(c.yaml.ui.opengl.desktopProfile, u"compatibility"_s);
    QCOMPARE(c.yaml.ui.opengl.esMajorVersion, 5);
    QCOMPARE(c.yaml.ui.opengl.esMinorVersion, 15);

    QList<QPair<QString, QString>> containerSelection {
        { u"*"_s, u"selectionFunction"_s }
    };
    QCOMPARE(c.yaml.containers.selection, containerSelection);
    QCOMPARE(c.yaml.containers.configurations, QVariantMap
             ({
                  { u"c-test"_s, QVariantMap {
                        {  u"c-parameter"_s, u"c-value"_s }
                    } }
              }));
    QCOMPARE(c.yaml.runtimes.configurations, QVariantMap
             ({
                  { u"r-test"_s, QVariantMap {
                        {  u"r-parameter"_s, u"r-value"_s }
                    } }
              }));
    QCOMPARE(c.yaml.runtimes.additionalLaunchers, QStringList(u"a"_s));

    QCOMPARE(c.yaml.dbus.registrations, QVariantMap({ { u"iface1"_s, u"foobus"_s } }));

    QCOMPARE(c.yaml.systemProperties, QVariantMap
             ({
                  { u"public"_s, QVariantMap {
                        {  u"public-prop"_s, u"public-value"_s }
                    } },
                  { u"protected"_s, QVariantMap {
                        {  u"protected-prop"_s, u"protected-value"_s }
                    } },
                  { u"private"_s, QVariantMap {
                        {  u"private-prop"_s, u"private-value"_s }
                    } }
              }));

    QCOMPARE(c.yaml.quicklaunch.idleLoad, qreal(0.5));
    QMap<std::pair<QString, QString>, int> rpc { { { u"*"_s, u"*"_s}, 5 } };
    QCOMPARE(c.yaml.quicklaunch.runtimesPerContainer, rpc);
    QCOMPARE(c.yaml.quicklaunch.failedStartLimit, 42);
    QCOMPARE(c.yaml.quicklaunch.failedStartLimitIntervalSec.count(), 43);

    QCOMPARE(c.yaml.wayland.socketName, u"my-wlsock-42"_s);

    QList<ConfigurationData::Wayland::ExtraSocket> extraSockets {
        { u"path-es1"_s, 0440, 1, 2 },
        { u"path-es2"_s, 0222, 3, 4 }
    };
    QCOMPARE(c.yaml.wayland.extraSockets, extraSockets);

    QCOMPARE(c.yaml.crashAction.printBacktrace, true);
    QCOMPARE(c.yaml.crashAction.printQmlStack, true);
    QCOMPARE(c.yaml.crashAction.waitForGdbAttach.count(), 42);
    QCOMPARE(c.yaml.crashAction.dumpCore, true);
    QCOMPARE(c.yaml.crashAction.stackFramesToIgnore.onCrash, -1);
    QCOMPARE(c.yaml.crashAction.stackFramesToIgnore.onException, -1);

    QCOMPARE(c.yaml.installer.caCertificates, QStringList({ u"cert1"_s, u"cert2"_s }));

    QCOMPARE(c.yaml.plugins.startup, QStringList({ u"s1"_s, u"s2"_s }));
    QCOMPARE(c.yaml.plugins.container, QStringList({ u"c1"_s, u"c2"_s }));

    QCOMPARE(c.yaml.watchdog.disable, true);
    QCOMPARE(c.yaml.watchdog.eventloop.checkInterval, 2min);
    QCOMPARE(c.yaml.watchdog.eventloop.warnTimeout, 3min);
    QCOMPARE(c.yaml.watchdog.eventloop.killTimeout, 4min);
    QCOMPARE(c.yaml.watchdog.quickwindow.checkInterval, 2s);
    QCOMPARE(c.yaml.watchdog.quickwindow.syncWarnTimeout, 3s);
    QCOMPARE(c.yaml.watchdog.quickwindow.syncKillTimeout, 4s);
    QCOMPARE(c.yaml.watchdog.quickwindow.renderWarnTimeout, 5s);
    QCOMPARE(c.yaml.watchdog.quickwindow.renderKillTimeout, 6s);
    QCOMPARE(c.yaml.watchdog.quickwindow.swapWarnTimeout, 7s);
    QCOMPARE(c.yaml.watchdog.quickwindow.swapKillTimeout, 8s);
    QCOMPARE(c.yaml.watchdog.wayland.checkInterval, 2ms);
    QCOMPARE(c.yaml.watchdog.wayland.warnTimeout, 3ms);
    QCOMPARE(c.yaml.watchdog.wayland.killTimeout, 4ms);
}

void tst_Configuration::mergedConfig()
{
    Configuration c({ u":/data/"_s }, u":/build-config.yaml"_s);
    c.parseWithArguments({ u"test"_s, u"--no-cache"_s });

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), false); // legacy
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), u""_s);
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.yaml.ui.mainQml, u"main2.qml"_s);

    QCOMPARE(c.yaml.applications.builtinAppsManifestDir, QStringList({ u"builtin-dir"_s, u"builtin-dir2"_s }));
    QCOMPARE(c.yaml.applications.documentDir, u"doc-dir2"_s);

    QCOMPARE(c.yaml.applications.installationDir, u"installation-dir2"_s);
    QCOMPARE(c.yaml.installer.disable, true);
    QCOMPARE(c.yaml.intents.disable, true);
    QCOMPARE(c.yaml.intents.timeouts.disambiguation.count(), 5);
    QCOMPARE(c.yaml.intents.timeouts.startApplication.count(), 6);
    QCOMPARE(c.yaml.intents.timeouts.replyFromApplication.count(), 7);
    QCOMPARE(c.yaml.intents.timeouts.replyFromSystem.count(), 8);

    QCOMPARE(c.yaml.ui.fullscreen, true);
    QCOMPARE(c.yaml.ui.windowIcon, u"icon2.png"_s);
    QCOMPARE(c.yaml.ui.importPaths, QStringList
             ({ pwd.absoluteFilePath(u"ip1"_s),
                pwd.absoluteFilePath(u"ip2"_s),
                pwd.absoluteFilePath(u"ip3"_s) }));
    QCOMPARE(c.yaml.ui.pluginPaths, QStringList({ u"pp1"_s, u"pp2"_s, u"pp3"_s }));
    QCOMPARE(c.yaml.ui.loadDummyData, true);
    QCOMPARE(c.yaml.flags.noSecurity, true);
    QCOMPARE(c.yaml.flags.developmentMode, true);
    QCOMPARE(c.yaml.flags.allowUnsignedPackages, true);
    QCOMPARE(c.yaml.flags.allowUnknownUiClients, true);
    QCOMPARE(c.yaml.flags.forceSingleProcess, true);
    QCOMPARE(c.yaml.flags.forceMultiProcess, true);
    QCOMPARE(c.yaml.logging.rules, QStringList({ u"lr1"_s, u"lr2"_s, u"lr3"_s }));
    QCOMPARE(c.yaml.logging.messagePattern, u"msgPattern2"_s);
    QCOMPARE(c.yaml.logging.useAMConsoleLogger, true);
    QCOMPARE(c.yaml.ui.style, u"mystyle2"_s);
    QCOMPARE(c.yaml.ui.iconThemeName, u"mytheme2"_s);
    QCOMPARE(c.yaml.ui.iconThemeSearchPaths, QStringList({ u"itsp1"_s, u"itsp2"_s, u"itsp3"_s }));
    QCOMPARE(c.yaml.logging.dlt.id, u"dltid2"_s);
    QCOMPARE(c.yaml.logging.dlt.description, u"dltdesc2"_s);
    QCOMPARE(c.yaml.logging.dlt.longMessageBehavior, u"truncate"_s);
    QCOMPARE(c.yaml.ui.resources, QStringList({ u"r1"_s, u"r2"_s, u"r3"_s }));

    QCOMPARE(c.yaml.ui.opengl.desktopProfile, u"classic"_s);
    QCOMPARE(c.yaml.ui.opengl.esMajorVersion, 2);
    QCOMPARE(c.yaml.ui.opengl.esMinorVersion, 0);

    QList<QPair<QString, QString>> containerSelection {
        { u"*"_s, u"selectionFunction"_s },
        { u"2"_s, u"second"_s }
    };
    QCOMPARE(c.yaml.containers.selection, containerSelection);
    QCOMPARE(c.yaml.containers.configurations, QVariantMap
             ({
                  { u"c-test"_s, QVariantMap {
                        {  u"c-parameter"_s, u"xc-value"_s },
                    } },
                  { u"c-test2"_s, QVariantMap {
                        {  u"c-parameter2"_s, u"c-value2"_s },
                    } }

              }));

    QCOMPARE(c.yaml.runtimes.configurations, QVariantMap
             ({
                  { u"r-test"_s, QVariantMap {
                        {  u"r-parameter"_s, u"xr-value"_s },
                    } },
                  { u"r-test2"_s, QVariantMap {
                        {  u"r-parameter2"_s, u"r-value2"_s },
                    } }

              }));
    QCOMPARE(c.yaml.runtimes.additionalLaunchers, QStringList({ u"a"_s, u"b"_s, u"c"_s }));

    QVERIFY(c.yaml.dbus.policies.isEmpty());
    QCOMPARE(c.yaml.dbus.registrations, QVariantMap
             ({
                 { u"iface1"_s, u"foobus1"_s },
                 { u"iface2"_s, u"foobus2"_s }
             }));

    QCOMPARE(c.yaml.systemProperties, QVariantMap
             ({
                  { u"public"_s, QVariantMap {
                        {  u"public-prop"_s, u"xpublic-value"_s },
                        {  u"public-prop2"_s, u"public-value2"_s }
                    } },
                  { u"protected"_s, QVariantMap {
                        {  u"protected-prop"_s, u"xprotected-value"_s },
                        {  u"protected-prop2"_s, u"protected-value2"_s }
                    } },
                  { u"private"_s, QVariantMap {
                        {  u"private-prop"_s, u"xprivate-value"_s },
                        {  u"private-prop2"_s, u"private-value2"_s }
                    } }
              }));

    QCOMPARE(c.yaml.quicklaunch.idleLoad, qreal(0.2));
    QMap<std::pair<QString, QString>, int> rpc = {
        { { u"*"_s, u"*"_s }, 5 },
        { { u"c-foo"_s, u"r-foo"_s }, 1 },
        { { u"c-foo"_s, u"r-bar"_s }, 2 },
        { { u"c-bar"_s, u"*"_s }, 4 },
    };
    QCOMPARE(c.yaml.quicklaunch.runtimesPerContainer, rpc);
    QCOMPARE(c.yaml.quicklaunch.failedStartLimit, 44);
    QCOMPARE(c.yaml.quicklaunch.failedStartLimitIntervalSec.count(), 45);

    QCOMPARE(c.yaml.wayland.socketName, u"other-wlsock-0"_s);

    QList<ConfigurationData::Wayland::ExtraSocket> extraSockets {
        { u"path-es1"_s, 0440, 1, 2 },
        { u"path-es2"_s, 0222, 3, 4 },
        { u"path-es3"_s, -1, -1, -1 }
    };
    QCOMPARE(c.yaml.wayland.extraSockets, extraSockets);
    QCOMPARE(c.yaml.crashAction.printBacktrace, true);
    QCOMPARE(c.yaml.crashAction.printQmlStack, true);
    QCOMPARE(c.yaml.crashAction.waitForGdbAttach.count(), 42);
    QCOMPARE(c.yaml.crashAction.dumpCore, true);
    QCOMPARE(c.yaml.crashAction.stackFramesToIgnore.onCrash, -1);
    QCOMPARE(c.yaml.crashAction.stackFramesToIgnore.onException, -1);

    QCOMPARE(c.yaml.installer.caCertificates, QStringList({ u"cert1"_s, u"cert2"_s, u"cert3"_s }));

    QCOMPARE(c.yaml.plugins.container, QStringList({ u"c1"_s, u"c2"_s, u"c3"_s, u"c4"_s }));
    QCOMPARE(c.yaml.plugins.startup, QStringList({ u"s1"_s, u"s2"_s, u"s3"_s }));

// the QTextStream op<< for std::chrono::duration cannot deal with double based durations
#define TO_MS(x) std::chrono::duration_cast<std::chrono::milliseconds>(x)

    QCOMPARE(c.yaml.watchdog.disable, true);
    QCOMPARE(c.yaml.watchdog.eventloop.checkInterval, TO_MS(2.5min));
    QCOMPARE(c.yaml.watchdog.eventloop.warnTimeout, TO_MS(3.5min));
    QCOMPARE(c.yaml.watchdog.eventloop.killTimeout, TO_MS(4.5min));
    QCOMPARE(c.yaml.watchdog.quickwindow.checkInterval, TO_MS(2.5s));
    QCOMPARE(c.yaml.watchdog.quickwindow.syncWarnTimeout, TO_MS(3.5s));
    QCOMPARE(c.yaml.watchdog.quickwindow.syncKillTimeout, TO_MS(4.5s));
    QCOMPARE(c.yaml.watchdog.quickwindow.renderWarnTimeout, TO_MS(5.5s));
    QCOMPARE(c.yaml.watchdog.quickwindow.renderKillTimeout, TO_MS(6.5s));
    QCOMPARE(c.yaml.watchdog.quickwindow.swapWarnTimeout, TO_MS(7.5s));
    QCOMPARE(c.yaml.watchdog.quickwindow.swapKillTimeout, TO_MS(8.5s));
    QCOMPARE(c.yaml.watchdog.wayland.checkInterval, TO_MS(2.5h));
    QCOMPARE(c.yaml.watchdog.wayland.warnTimeout, TO_MS(3.5h));
    QCOMPARE(c.yaml.watchdog.wayland.killTimeout, 5ms); // round to precision

#undef TO_MS
}

void tst_Configuration::commandLineConfig()
{
    Configuration c;
    QByteArrayList commandLine { "test", "--no-cache" };

    commandLine << "--builtin-apps-manifest-dir" << "builtin-dir-cl1"
                << "--builtin-apps-manifest-dir" << "builtin-dir-cl2"
                << "--installation-dir" << "installation-dir-cl"
                << "--document-dir" << "document-dir-cl"
                << "--disable-installer"
                << "--disable-intents"
                << "--dbus" << "system"
                << "--no-fullscreen"
                << "-I" << "ip-cl1"
                << "-I" << "ip-cl2"
                << "-v"
                << "--slow-animations"
                << "--load-dummydata"
                << "--no-security"
                << "--development-mode"
                << "--no-ui-watchdog"
                << "--no-dlt-logging"
                << "--force-single-process"
                << "--force-multi-process"
                << "--wayland-socket-name" << "wlsock-1"
                << "--single-app" << "appname"
                << "--logging-rule" << "cl-lr1"
                << "--logging-rule" << "cl-lr2"
                << "--qml-debug"
                << "main-cl.qml";

    QStringList strCommandLine;
    for (const auto &cl : std::as_const(commandLine))
        strCommandLine << QString::fromLatin1(cl);

    c.parseWithArguments(strCommandLine);

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), true); // legacy
    QCOMPARE(c.verbose(), true);
    QCOMPARE(c.slowAnimations(), true);
    QCOMPARE(c.noDltLogging(), true);
    QCOMPARE(c.singleApp(), u"appname"_s);
    QCOMPARE(c.qmlDebugging(), true);
    QCOMPARE(c.waylandSocketName(), u"wlsock-1"_s);
    QCOMPARE(c.dbus(), u"system"_s);

    // values from config file
    QCOMPARE(c.yaml.ui.mainQml, u"main-cl.qml"_s);

    QCOMPARE(c.yaml.applications.builtinAppsManifestDir, QStringList({ u"builtin-dir-cl1"_s, u"builtin-dir-cl2"_s }));
    QCOMPARE(c.yaml.applications.documentDir, u"document-dir-cl"_s);

    QCOMPARE(c.yaml.applications.installationDir, u"installation-dir-cl"_s);
    QCOMPARE(c.yaml.installer.disable, true);
    QCOMPARE(c.yaml.intents.disable, true);
    QCOMPARE(c.yaml.intents.timeouts.disambiguation.count(), 10000);
    QCOMPARE(c.yaml.intents.timeouts.startApplication.count(), 3000);
    QCOMPARE(c.yaml.intents.timeouts.replyFromApplication.count(), 5000);
    QCOMPARE(c.yaml.intents.timeouts.replyFromSystem.count(), 20000);

    QCOMPARE(c.yaml.ui.fullscreen, false);
    QCOMPARE(c.yaml.ui.windowIcon, u""_s);
    QCOMPARE(c.yaml.ui.importPaths, QStringList({ pwd.absoluteFilePath(u"ip-cl1"_s),
                                            pwd.absoluteFilePath(u"ip-cl2"_s) }));
    QCOMPARE(c.yaml.ui.pluginPaths, {});
    QCOMPARE(c.yaml.ui.loadDummyData, true);
    QCOMPARE(c.yaml.flags.noSecurity, true);
    QCOMPARE(c.yaml.flags.developmentMode, true);
    QCOMPARE(c.yaml.flags.forceSingleProcess, true);
    QCOMPARE(c.yaml.flags.forceMultiProcess, true);
    QCOMPARE(c.yaml.logging.rules, QStringList({ u"cl-lr1"_s, u"cl-lr2"_s }));
    QCOMPARE(c.yaml.logging.messagePattern, u""_s);
    QCOMPARE(c.yaml.logging.useAMConsoleLogger, QVariant());
    QCOMPARE(c.yaml.ui.style, u""_s);
    QCOMPARE(c.yaml.ui.iconThemeName, u""_s);
    QCOMPARE(c.yaml.ui.iconThemeSearchPaths, {});
    QCOMPARE(c.yaml.logging.dlt.id, u""_s);
    QCOMPARE(c.yaml.logging.dlt.description, u""_s);
    QCOMPARE(c.yaml.ui.resources, {});

    QCOMPARE(c.yaml.ui.opengl.desktopProfile, u""_s);
    QCOMPARE(c.yaml.ui.opengl.esMajorVersion, -1);
    QCOMPARE(c.yaml.ui.opengl.esMinorVersion, -1);

    QCOMPARE(c.yaml.containers.selection, {});
    QCOMPARE(c.yaml.containers.configurations, QVariantMap{});
    QCOMPARE(c.yaml.runtimes.configurations, QVariantMap{});
    QCOMPARE(c.yaml.runtimes.additionalLaunchers, QStringList{});

    QVERIFY(c.yaml.dbus.policies.isEmpty());
    QVERIFY(c.yaml.dbus.registrations.isEmpty());

    QCOMPARE(c.yaml.systemProperties, QVariantMap {});

    QCOMPARE(c.yaml.quicklaunch.idleLoad, qreal(0));
    QVERIFY(c.yaml.quicklaunch.runtimesPerContainer.isEmpty());

    QCOMPARE(c.yaml.wayland.socketName, u""_s);
    QVERIFY(c.yaml.wayland.extraSockets.isEmpty());

    QCOMPARE(c.yaml.crashAction.printBacktrace, true);
    QCOMPARE(c.yaml.crashAction.printQmlStack, true);
    QCOMPARE(c.yaml.crashAction.waitForGdbAttach.count(), 0);
    QCOMPARE(c.yaml.crashAction.dumpCore, true);
    QCOMPARE(c.yaml.crashAction.stackFramesToIgnore.onCrash, -1);
    QCOMPARE(c.yaml.crashAction.stackFramesToIgnore.onException, -1);

    QCOMPARE(c.yaml.installer.caCertificates, {});

    QCOMPARE(c.yaml.plugins.container, {});
    QCOMPARE(c.yaml.plugins.startup, {});

    QCOMPARE(c.yaml.watchdog.disable, true); // via the legacy flag 'noUiWatchdog'
}


QTEST_GUILESS_MAIN(tst_Configuration)

#include "tst_configuration.moc"
