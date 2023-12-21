// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore/QtCore>
#include <QtTest/QtTest>

#include <QtAppManMain/configuration.h>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/global.h>

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_Configuration : public QObject
{
    Q_OBJECT

public:
    tst_Configuration();

private slots:
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
    QCOMPARE(c.noFullscreen(), false);
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), u""_s);
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.mainQmlFile(), u""_s);

    QCOMPARE(c.builtinAppsManifestDirs(), {});
    QCOMPARE(c.documentDir(), u""_s);

    QCOMPARE(c.installationDir(), u""_s);
    QCOMPARE(c.disableInstaller(), false);
    QCOMPARE(c.disableIntents(), false);
    QCOMPARE(c.intentTimeoutForDisambiguation(), 10000);
    QCOMPARE(c.intentTimeoutForStartApplication(), 3000);
    QCOMPARE(c.intentTimeoutForReplyFromApplication(), 5000);
    QCOMPARE(c.intentTimeoutForReplyFromSystem(), 20000);

    QCOMPARE(c.fullscreen(), false);
    QCOMPARE(c.windowIcon(), u""_s);
    QCOMPARE(c.importPaths(), {});
    QCOMPARE(c.pluginPaths(), {});
    QCOMPARE(c.loadDummyData(), false);
    QCOMPARE(c.noSecurity(), false);
    QCOMPARE(c.developmentMode(), false);
    QCOMPARE(c.noUiWatchdog(), false);
    QCOMPARE(c.allowUnsignedPackages(), false);
    QCOMPARE(c.allowUnknownUiClients(), false);
    QCOMPARE(c.forceSingleProcess(), false);
    QCOMPARE(c.forceMultiProcess(), false);
    QCOMPARE(c.loggingRules(), {});
    QCOMPARE(c.messagePattern(), u""_s);
    QCOMPARE(c.useAMConsoleLogger(), QVariant());
    QCOMPARE(c.style(), u""_s);
    QCOMPARE(c.iconThemeName(), u""_s);
    QCOMPARE(c.iconThemeSearchPaths(), {});
    QCOMPARE(c.dltId(), u""_s);
    QCOMPARE(c.dltDescription(), u""_s);
    QCOMPARE(c.resources(), {});

    QCOMPARE(c.openGLConfiguration(), QVariantMap {});

    QCOMPARE(c.installationLocations(), {});

    QCOMPARE(c.containerSelectionConfiguration(), {});
    QCOMPARE(c.containerConfigurations(), QVariantMap {});
    QCOMPARE(c.runtimeAdditionalLaunchers(), QStringList {});
    QCOMPARE(c.runtimeConfigurations(), QVariantMap {});

    QCOMPARE(c.dbusRegistration("iface1"), u"auto"_s);

    QCOMPARE(c.rawSystemProperties(), QVariantMap {});

    QCOMPARE(c.quickLaunchIdleLoad(), qreal(0));
    QVERIFY(c.quickLaunchRuntimesPerContainer().isEmpty());

    QString defaultWaylandSocketName =
#if defined(Q_OS_LINUX)
            u"qtam-wayland-"_s;
#else
            QString();
#endif
    // we cannot rely on the actual display number, when running on Wayland Desktop
    QVERIFY(c.waylandSocketName().startsWith(defaultWaylandSocketName));
    QCOMPARE(c.waylandExtraSockets(), {});

    QCOMPARE(c.managerCrashAction(), QVariantMap {});

    QCOMPARE(c.caCertificates(), {});

    QCOMPARE(c.pluginFilePaths("container"), {});
    QCOMPARE(c.pluginFilePaths("startup"), {});
}

void tst_Configuration::simpleConfig()
{
    Configuration c({ u":/data/config1.yaml"_s }, u":/build-config.yaml"_s);
    c.parseWithArguments({ u"test"_s, u"--no-cache"_s });

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), false);
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), u""_s);
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.mainQmlFile(), u"main.qml"_s);

    QCOMPARE(c.builtinAppsManifestDirs(), { u"builtin-dir"_s });
    QCOMPARE(c.documentDir(), u"doc-dir"_s);

    QCOMPARE(c.installationDir(), u"installation-dir"_s);
    QCOMPARE(c.disableInstaller(), true);
    QCOMPARE(c.disableIntents(), true);
    QCOMPARE(c.intentTimeoutForDisambiguation(), 1);
    QCOMPARE(c.intentTimeoutForStartApplication(), 2);
    QCOMPARE(c.intentTimeoutForReplyFromApplication(), 3);
    QCOMPARE(c.intentTimeoutForReplyFromSystem(), 4);

    QCOMPARE(c.fullscreen(), true);
    QCOMPARE(c.windowIcon(), u"icon.png"_s);
    QCOMPARE(c.importPaths(), QStringList({ pwd.absoluteFilePath(u"ip1"_s), pwd.absoluteFilePath(u"ip2"_s) }));
    QCOMPARE(c.pluginPaths(), QStringList({ u"pp1"_s, u"pp2"_s }));
    QCOMPARE(c.loadDummyData(), true);
    QCOMPARE(c.noSecurity(), true);
    QCOMPARE(c.developmentMode(), true);
    QCOMPARE(c.noUiWatchdog(), true);
    QCOMPARE(c.allowUnsignedPackages(), true);
    QCOMPARE(c.allowUnknownUiClients(), true);
    QCOMPARE(c.forceSingleProcess(), true);
    QCOMPARE(c.forceMultiProcess(), true);
    QCOMPARE(c.loggingRules(), QStringList({ u"lr1"_s, u"lr2"_s }));
    QCOMPARE(c.messagePattern(), u"msgPattern"_s);
    QCOMPARE(c.useAMConsoleLogger(), QVariant(true));
    QCOMPARE(c.style(), u"mystyle"_s);
    QCOMPARE(c.iconThemeName(), u"mytheme"_s);
    QCOMPARE(c.iconThemeSearchPaths(), QStringList({ u"itsp1"_s, u"itsp2"_s }));
    QCOMPARE(c.dltId(), u"dltid"_s);
    QCOMPARE(c.dltDescription(), u"dltdesc"_s);
    QCOMPARE(c.dltLongMessageBehavior(), u"split"_s);
    QCOMPARE(c.resources(), QStringList({ u"r1"_s, u"r2"_s }));

    QCOMPARE(c.openGLConfiguration(), QVariantMap
             ({
                  { u"desktopProfile"_s, u"compatibility"_s },
                  { u"esMajorVersion"_s, 5 },
                  { u"esMinorVersion"_s, 15 }
              }));

    QCOMPARE(c.installationLocations(), {});

    QList<QPair<QString, QString>> containerSelectionConfiguration {
        { u"*"_s, u"selectionFunction"_s }
    };
    QCOMPARE(c.containerSelectionConfiguration(), containerSelectionConfiguration);
    QCOMPARE(c.containerConfigurations(), QVariantMap
             ({
                  { u"c-test"_s, QVariantMap {
                        {  u"c-parameter"_s, u"c-value"_s }
                    } }
              }));
    QCOMPARE(c.runtimeConfigurations(), QVariantMap
             ({
                  { u"r-test"_s, QVariantMap {
                        {  u"r-parameter"_s, u"r-value"_s }
                    } }
              }));
    QCOMPARE(c.runtimeAdditionalLaunchers(), QStringList(u"a"_s));

    QCOMPARE(c.dbusRegistration("iface1"), u"foobus"_s);

    QCOMPARE(c.rawSystemProperties(), QVariantMap
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

    QCOMPARE(c.quickLaunchIdleLoad(), qreal(0.5));
    QHash<std::pair<QString, QString>, int> rpc { { { u"*"_s, u"*"_s}, 5 } };
    QCOMPARE(c.quickLaunchRuntimesPerContainer(), rpc);
    QCOMPARE(c.quickLaunchFailedStartLimit(), 42);
    QCOMPARE(c.quickLaunchFailedStartLimitIntervalSec(), 43);

    QCOMPARE(c.waylandSocketName(), u"my-wlsock-42"_s);

    QCOMPARE(c.waylandExtraSockets(), QVariantList
             ({
                  QVariantMap {
                      { u"path"_s, u"path-es1"_s },
                      { u"permissions"_s, 0440 },
                      { u"userId"_s, 1 },
                      { u"groupId"_s, 2 }
                  },
                  QVariantMap {
                      { u"path"_s, u"path-es2"_s },
                      { u"permissions"_s, 0222 },
                      { u"userId"_s, 3 },
                      { u"groupId"_s, 4 }
                  }
              }));

    QCOMPARE(c.managerCrashAction(), QVariantMap
             ({
                  { u"printBacktrace"_s, true },
                  { u"printQmlStack"_s, true },
                  { u"waitForGdbAttach"_s, true },
                  { u"dumpCore"_s, true }
              }));

    QCOMPARE(c.caCertificates(), QStringList({ u"cert1"_s, u"cert2"_s }));

    QCOMPARE(c.pluginFilePaths("startup"), QStringList({ u"s1"_s, u"s2"_s }));
    QCOMPARE(c.pluginFilePaths("container"), QStringList({ u"c1"_s, u"c2"_s }));
}

void tst_Configuration::mergedConfig()
{
    Configuration c({ u":/data/"_s }, u":/build-config.yaml"_s);
    c.parseWithArguments({ u"test"_s, u"--no-cache"_s });

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), false);
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), u""_s);
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.mainQmlFile(), u"main2.qml"_s);

    QCOMPARE(c.builtinAppsManifestDirs(), QStringList({ u"builtin-dir"_s, u"builtin-dir2"_s }));
    QCOMPARE(c.documentDir(), u"doc-dir2"_s);

    QCOMPARE(c.installationDir(), u"installation-dir2"_s);
    QCOMPARE(c.disableInstaller(), true);
    QCOMPARE(c.disableIntents(), true);
    QCOMPARE(c.intentTimeoutForDisambiguation(), 5);
    QCOMPARE(c.intentTimeoutForStartApplication(), 6);
    QCOMPARE(c.intentTimeoutForReplyFromApplication(), 7);
    QCOMPARE(c.intentTimeoutForReplyFromSystem(), 8);

    QCOMPARE(c.fullscreen(), true);
    QCOMPARE(c.windowIcon(), u"icon2.png"_s);
    QCOMPARE(c.importPaths(), QStringList
             ({ pwd.absoluteFilePath(u"ip1"_s),
                pwd.absoluteFilePath(u"ip2"_s),
                pwd.absoluteFilePath(u"ip3"_s) }));
    QCOMPARE(c.pluginPaths(), QStringList({ u"pp1"_s, u"pp2"_s, u"pp3"_s }));
    QCOMPARE(c.loadDummyData(), true);
    QCOMPARE(c.noSecurity(), true);
    QCOMPARE(c.developmentMode(), true);
    QCOMPARE(c.noUiWatchdog(), true);
    QCOMPARE(c.allowUnsignedPackages(), true);
    QCOMPARE(c.allowUnknownUiClients(), true);
    QCOMPARE(c.forceSingleProcess(), true);
    QCOMPARE(c.forceMultiProcess(), true);
    QCOMPARE(c.loggingRules(), QStringList({ u"lr1"_s, u"lr2"_s, u"lr3"_s }));
    QCOMPARE(c.messagePattern(), u"msgPattern2"_s);
    QCOMPARE(c.useAMConsoleLogger(), QVariant());
    QCOMPARE(c.style(), u"mystyle2"_s);
    QCOMPARE(c.iconThemeName(), u"mytheme2"_s);
    QCOMPARE(c.iconThemeSearchPaths(), QStringList({ u"itsp1"_s, u"itsp2"_s, u"itsp3"_s }));
    QCOMPARE(c.dltId(), u"dltid2"_s);
    QCOMPARE(c.dltDescription(), u"dltdesc2"_s);
    QCOMPARE(c.dltLongMessageBehavior(), u"truncate"_s);
    QCOMPARE(c.resources(), QStringList({ u"r1"_s, u"r2"_s, u"r3"_s }));

    QCOMPARE(c.openGLConfiguration(), QVariantMap
             ({
                  { u"desktopProfile"_s, u"classic"_s },
                  { u"esMajorVersion"_s, 1 },
                  { u"esMinorVersion"_s, 0 },
              }));

    QCOMPARE(c.installationLocations(), {});

    QList<QPair<QString, QString>> containerSelectionConfiguration {
        { u"*"_s, u"selectionFunction"_s },
        { u"2"_s, u"second"_s }
    };
    QCOMPARE(c.containerSelectionConfiguration(), containerSelectionConfiguration);
    QCOMPARE(c.containerConfigurations(), QVariantMap
             ({
                  { u"c-test"_s, QVariantMap {
                        {  u"c-parameter"_s, u"xc-value"_s },
                    } },
                  { u"c-test2"_s, QVariantMap {
                        {  u"c-parameter2"_s, u"c-value2"_s },
                    } }

              }));

    QCOMPARE(c.runtimeConfigurations(), QVariantMap
             ({
                  { u"r-test"_s, QVariantMap {
                        {  u"r-parameter"_s, u"xr-value"_s },
                    } },
                  { u"r-test2"_s, QVariantMap {
                        {  u"r-parameter2"_s, u"r-value2"_s },
                    } }

              }));
    QCOMPARE(c.runtimeAdditionalLaunchers(), QStringList({ u"a"_s, u"b"_s, u"c"_s }));

    QCOMPARE(c.dbusRegistration("iface1"), u"foobus1"_s);
    QCOMPARE(c.dbusRegistration("iface2"), u"foobus2"_s);

    QCOMPARE(c.rawSystemProperties(), QVariantMap
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

    QCOMPARE(c.quickLaunchIdleLoad(), qreal(0.2));
    QHash<std::pair<QString, QString>, int> rpc = {
        { { u"*"_s, u"*"_s }, 5 },
        { { u"c-foo"_s, u"r-foo"_s }, 1 },
        { { u"c-foo"_s, u"r-bar"_s }, 2 },
        { { u"c-bar"_s, u"*"_s }, 4 },
    };
    QCOMPARE(c.quickLaunchRuntimesPerContainer(), rpc);
    QCOMPARE(c.quickLaunchFailedStartLimit(), 44);
    QCOMPARE(c.quickLaunchFailedStartLimitIntervalSec(), 45);

    QCOMPARE(c.waylandSocketName(), u"other-wlsock-0"_s);

    QCOMPARE(c.waylandExtraSockets(), QVariantList
             ({
                  QVariantMap {
                      { u"path"_s, u"path-es1"_s },
                      { u"permissions"_s, 0440 },
                      { u"userId"_s, 1 },
                      { u"groupId"_s, 2 }
                  },
                  QVariantMap {
                      { u"path"_s, u"path-es2"_s },
                      { u"permissions"_s, 0222 },
                      { u"userId"_s, 3 },
                      { u"groupId"_s, 4 }
                  },
                  QVariantMap {
                      { u"path"_s, u"path-es3"_s },
                  }
              }));

    QCOMPARE(c.managerCrashAction(), QVariantMap
             ({
                  { u"printBacktrace"_s, true },
                  { u"printQmlStack"_s, true },
                  { u"waitForGdbAttach"_s, true },
                  { u"dumpCore"_s, true }
              }));

    QCOMPARE(c.caCertificates(), QStringList({ u"cert1"_s, u"cert2"_s, u"cert3"_s }));

    QCOMPARE(c.pluginFilePaths("container"), QStringList({ u"c1"_s, u"c2"_s, u"c3"_s, u"c4"_s }));
    QCOMPARE(c.pluginFilePaths("startup"), QStringList({ u"s1"_s, u"s2"_s, u"s3"_s }));
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
    QCOMPARE(c.noFullscreen(), true);
    QCOMPARE(c.verbose(), true);
    QCOMPARE(c.slowAnimations(), true);
    QCOMPARE(c.noDltLogging(), true);
    QCOMPARE(c.singleApp(), u"appname"_s);
    QCOMPARE(c.qmlDebugging(), true);

    // values from config file
    QCOMPARE(c.mainQmlFile(), u"main-cl.qml"_s);

    QCOMPARE(c.builtinAppsManifestDirs(), QStringList({ u"builtin-dir-cl1"_s, u"builtin-dir-cl2"_s }));
    QCOMPARE(c.documentDir(), u"document-dir-cl"_s);

    QCOMPARE(c.installationDir(), u"installation-dir-cl"_s);
    QCOMPARE(c.disableInstaller(), true);
    QCOMPARE(c.disableIntents(), true);
    QCOMPARE(c.intentTimeoutForDisambiguation(), 10000);
    QCOMPARE(c.intentTimeoutForStartApplication(), 3000);
    QCOMPARE(c.intentTimeoutForReplyFromApplication(), 5000);
    QCOMPARE(c.intentTimeoutForReplyFromSystem(), 20000);

    QCOMPARE(c.fullscreen(), false);
    QCOMPARE(c.windowIcon(), u""_s);
    QCOMPARE(c.importPaths(), QStringList({ pwd.absoluteFilePath(u"ip-cl1"_s),
                                            pwd.absoluteFilePath(u"ip-cl2"_s) }));
    QCOMPARE(c.pluginPaths(), {});
    QCOMPARE(c.loadDummyData(), true);
    QCOMPARE(c.noSecurity(), true);
    QCOMPARE(c.developmentMode(), true);
    QCOMPARE(c.noUiWatchdog(), true);
    QCOMPARE(c.forceSingleProcess(), true);
    QCOMPARE(c.forceMultiProcess(), true);
    QCOMPARE(c.loggingRules(), QStringList({ u"cl-lr1"_s, u"cl-lr2"_s }));
    QCOMPARE(c.messagePattern(), u""_s);
    QCOMPARE(c.useAMConsoleLogger(), QVariant());
    QCOMPARE(c.style(), u""_s);
    QCOMPARE(c.iconThemeName(), u""_s);
    QCOMPARE(c.iconThemeSearchPaths(), {});
    QCOMPARE(c.dltId(), u""_s);
    QCOMPARE(c.dltDescription(), u""_s);
    QCOMPARE(c.resources(), {});

    QCOMPARE(c.openGLConfiguration(), QVariantMap {});

    QCOMPARE(c.installationLocations(), {});

    QCOMPARE(c.containerSelectionConfiguration(), {});
    QCOMPARE(c.containerConfigurations(), QVariantMap{});
    QCOMPARE(c.runtimeConfigurations(), QVariantMap{});
    QCOMPARE(c.runtimeAdditionalLaunchers(), QStringList{});

    QCOMPARE(c.dbusRegistration("iface1"), u"system"_s);

    QCOMPARE(c.rawSystemProperties(), QVariantMap {});

    QCOMPARE(c.quickLaunchIdleLoad(), qreal(0));
    QVERIFY(c.quickLaunchRuntimesPerContainer().isEmpty());

    QCOMPARE(c.waylandSocketName(), u"wlsock-1"_s);
    QCOMPARE(c.waylandExtraSockets(), {});

    QCOMPARE(c.managerCrashAction(), QVariantMap {});

    QCOMPARE(c.caCertificates(), {});

    QCOMPARE(c.pluginFilePaths("container"), {});
    QCOMPARE(c.pluginFilePaths("startup"), {});
}


QTEST_MAIN(tst_Configuration)

#include "tst_configuration.moc"
