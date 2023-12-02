// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtCore/QtCore>
#include <QtTest/QtTest>

#include <QtAppManMain/configuration.h>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/global.h>

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
    c.parseWithArguments({ qSL("test"), qSL("--no-cache") });

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), false);
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), qSL(""));
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.mainQmlFile(), qSL(""));

    QCOMPARE(c.builtinAppsManifestDirs(), {});
    QCOMPARE(c.documentDir(), qSL(""));

    QCOMPARE(c.installationDir(), qSL(""));
    QCOMPARE(c.disableInstaller(), false);
    QCOMPARE(c.disableIntents(), false);
    QCOMPARE(c.intentTimeoutForDisambiguation(), 10000);
    QCOMPARE(c.intentTimeoutForStartApplication(), 3000);
    QCOMPARE(c.intentTimeoutForReplyFromApplication(), 5000);
    QCOMPARE(c.intentTimeoutForReplyFromSystem(), 20000);

    QCOMPARE(c.fullscreen(), false);
    QCOMPARE(c.windowIcon(), qSL(""));
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
    QCOMPARE(c.messagePattern(), qSL(""));
    QCOMPARE(c.useAMConsoleLogger(), QVariant());
    QCOMPARE(c.style(), qSL(""));
    QCOMPARE(c.iconThemeName(), qSL(""));
    QCOMPARE(c.iconThemeSearchPaths(), {});
    QCOMPARE(c.dltId(), qSL(""));
    QCOMPARE(c.dltDescription(), qSL(""));
    QCOMPARE(c.resources(), {});

    QCOMPARE(c.openGLConfiguration(), QVariantMap {});

    QCOMPARE(c.installationLocations(), {});

    QCOMPARE(c.containerSelectionConfiguration(), {});
    QCOMPARE(c.containerConfigurations(), QVariantMap {});
    QCOMPARE(c.runtimeAdditionalLaunchers(), QStringList {});
    QCOMPARE(c.runtimeConfigurations(), QVariantMap {});

    QCOMPARE(c.dbusRegistration("iface1"), qSL("auto"));

    QCOMPARE(c.rawSystemProperties(), QVariantMap {});

    QCOMPARE(c.quickLaunchIdleLoad(), qreal(0));
    QVERIFY(c.quickLaunchRuntimesPerContainer().isEmpty());

    QString defaultWaylandSocketName =
#if defined(Q_OS_LINUX)
            qSL("qtam-wayland-");
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
    Configuration c({ qSL(":/data/config1.yaml") }, qSL(":/build-config.yaml"));
    c.parseWithArguments({ qSL("test"), qSL("--no-cache") });

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), false);
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), qSL(""));
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.mainQmlFile(), qSL("main.qml"));

    QCOMPARE(c.builtinAppsManifestDirs(), { qSL("builtin-dir") });
    QCOMPARE(c.documentDir(), qSL("doc-dir"));

    QCOMPARE(c.installationDir(), qSL("installation-dir"));
    QCOMPARE(c.disableInstaller(), true);
    QCOMPARE(c.disableIntents(), true);
    QCOMPARE(c.intentTimeoutForDisambiguation(), 1);
    QCOMPARE(c.intentTimeoutForStartApplication(), 2);
    QCOMPARE(c.intentTimeoutForReplyFromApplication(), 3);
    QCOMPARE(c.intentTimeoutForReplyFromSystem(), 4);

    QCOMPARE(c.fullscreen(), true);
    QCOMPARE(c.windowIcon(), qSL("icon.png"));
    QCOMPARE(c.importPaths(), QStringList({ pwd.absoluteFilePath(qSL("ip1")), pwd.absoluteFilePath(qSL("ip2")) }));
    QCOMPARE(c.pluginPaths(), QStringList({ qSL("pp1"), qSL("pp2") }));
    QCOMPARE(c.loadDummyData(), true);
    QCOMPARE(c.noSecurity(), true);
    QCOMPARE(c.developmentMode(), true);
    QCOMPARE(c.noUiWatchdog(), true);
    QCOMPARE(c.allowUnsignedPackages(), true);
    QCOMPARE(c.allowUnknownUiClients(), true);
    QCOMPARE(c.forceSingleProcess(), true);
    QCOMPARE(c.forceMultiProcess(), true);
    QCOMPARE(c.loggingRules(), QStringList({ qSL("lr1"), qSL("lr2") }));
    QCOMPARE(c.messagePattern(), qSL("msgPattern"));
    QCOMPARE(c.useAMConsoleLogger(), QVariant(true));
    QCOMPARE(c.style(), qSL("mystyle"));
    QCOMPARE(c.iconThemeName(), qSL("mytheme"));
    QCOMPARE(c.iconThemeSearchPaths(), QStringList({ qSL("itsp1"), qSL("itsp2") }));
    QCOMPARE(c.dltId(), qSL("dltid"));
    QCOMPARE(c.dltDescription(), qSL("dltdesc"));
    QCOMPARE(c.dltLongMessageBehavior(), qSL("split"));
    QCOMPARE(c.resources(), QStringList({ qSL("r1"), qSL("r2") }));

    QCOMPARE(c.openGLConfiguration(), QVariantMap
             ({
                  { qSL("desktopProfile"), qSL("compatibility") },
                  { qSL("esMajorVersion"), 5 },
                  { qSL("esMinorVersion"), 15 }
              }));

    QCOMPARE(c.installationLocations(), {});

    QList<QPair<QString, QString>> containerSelectionConfiguration {
        { qSL("*"), qSL("selectionFunction") }
    };
    QCOMPARE(c.containerSelectionConfiguration(), containerSelectionConfiguration);
    QCOMPARE(c.containerConfigurations(), QVariantMap
             ({
                  { qSL("c-test"), QVariantMap {
                        {  qSL("c-parameter"), qSL("c-value") }
                    } }
              }));
    QCOMPARE(c.runtimeConfigurations(), QVariantMap
             ({
                  { qSL("r-test"), QVariantMap {
                        {  qSL("r-parameter"), qSL("r-value") }
                    } }
              }));
    QCOMPARE(c.runtimeAdditionalLaunchers(), QStringList(qSL("a")));

    QCOMPARE(c.dbusRegistration("iface1"), qSL("foobus"));

    QCOMPARE(c.rawSystemProperties(), QVariantMap
             ({
                  { qSL("public"), QVariantMap {
                        {  qSL("public-prop"), qSL("public-value") }
                    } },
                  { qSL("protected"), QVariantMap {
                        {  qSL("protected-prop"), qSL("protected-value") }
                    } },
                  { qSL("private"), QVariantMap {
                        {  qSL("private-prop"), qSL("private-value") }
                    } }
              }));

    QCOMPARE(c.quickLaunchIdleLoad(), qreal(0.5));
    QHash<std::pair<QString, QString>, int> rpc { { { qSL("*"), qSL("*")}, 5 } };
    QCOMPARE(c.quickLaunchRuntimesPerContainer(), rpc);
    QCOMPARE(c.quickLaunchFailedStartLimit(), 42);
    QCOMPARE(c.quickLaunchFailedStartLimitIntervalSec(), 43);

    QCOMPARE(c.waylandSocketName(), qSL("my-wlsock-42"));

    QCOMPARE(c.waylandExtraSockets(), QVariantList
             ({
                  QVariantMap {
                      { qSL("path"), qSL("path-es1") },
                      { qSL("permissions"), 0440 },
                      { qSL("userId"), 1 },
                      { qSL("groupId"), 2 }
                  },
                  QVariantMap {
                      { qSL("path"), qSL("path-es2") },
                      { qSL("permissions"), 0222 },
                      { qSL("userId"), 3 },
                      { qSL("groupId"), 4 }
                  }
              }));

    QCOMPARE(c.managerCrashAction(), QVariantMap
             ({
                  { qSL("printBacktrace"), true },
                  { qSL("printQmlStack"), true },
                  { qSL("waitForGdbAttach"), true },
                  { qSL("dumpCore"), true }
              }));

    QCOMPARE(c.caCertificates(), QStringList({ qSL("cert1"), qSL("cert2") }));

    QCOMPARE(c.pluginFilePaths("startup"), QStringList({ qSL("s1"), qSL("s2") }));
    QCOMPARE(c.pluginFilePaths("container"), QStringList({ qSL("c1"), qSL("c2") }));
}

void tst_Configuration::mergedConfig()
{
    Configuration c({ qSL(":/data/") }, qSL(":/build-config.yaml"));
    c.parseWithArguments({ qSL("test"), qSL("--no-cache") });

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), false);
    QCOMPARE(c.verbose(), false);
    QCOMPARE(c.slowAnimations(), false);
    QCOMPARE(c.noDltLogging(), false);
    QCOMPARE(c.singleApp(), qSL(""));
    QCOMPARE(c.qmlDebugging(), false);

    // values from config file
    QCOMPARE(c.mainQmlFile(), qSL("main2.qml"));

    QCOMPARE(c.builtinAppsManifestDirs(), QStringList({ qSL("builtin-dir"), qSL("builtin-dir2") }));
    QCOMPARE(c.documentDir(), qSL("doc-dir2"));

    QCOMPARE(c.installationDir(), qSL("installation-dir2"));
    QCOMPARE(c.disableInstaller(), true);
    QCOMPARE(c.disableIntents(), true);
    QCOMPARE(c.intentTimeoutForDisambiguation(), 5);
    QCOMPARE(c.intentTimeoutForStartApplication(), 6);
    QCOMPARE(c.intentTimeoutForReplyFromApplication(), 7);
    QCOMPARE(c.intentTimeoutForReplyFromSystem(), 8);

    QCOMPARE(c.fullscreen(), true);
    QCOMPARE(c.windowIcon(), qSL("icon2.png"));
    QCOMPARE(c.importPaths(), QStringList
             ({ pwd.absoluteFilePath(qSL("ip1")),
                pwd.absoluteFilePath(qSL("ip2")),
                pwd.absoluteFilePath(qSL("ip3")) }));
    QCOMPARE(c.pluginPaths(), QStringList({ qSL("pp1"), qSL("pp2"), qSL("pp3") }));
    QCOMPARE(c.loadDummyData(), true);
    QCOMPARE(c.noSecurity(), true);
    QCOMPARE(c.developmentMode(), true);
    QCOMPARE(c.noUiWatchdog(), true);
    QCOMPARE(c.allowUnsignedPackages(), true);
    QCOMPARE(c.allowUnknownUiClients(), true);
    QCOMPARE(c.forceSingleProcess(), true);
    QCOMPARE(c.forceMultiProcess(), true);
    QCOMPARE(c.loggingRules(), QStringList({ qSL("lr1"), qSL("lr2"), qSL("lr3") }));
    QCOMPARE(c.messagePattern(), qSL("msgPattern2"));
    QCOMPARE(c.useAMConsoleLogger(), QVariant());
    QCOMPARE(c.style(), qSL("mystyle2"));
    QCOMPARE(c.iconThemeName(), qSL("mytheme2"));
    QCOMPARE(c.iconThemeSearchPaths(), QStringList({ qSL("itsp1"), qSL("itsp2"), qSL("itsp3") }));
    QCOMPARE(c.dltId(), qSL("dltid2"));
    QCOMPARE(c.dltDescription(), qSL("dltdesc2"));
    QCOMPARE(c.dltLongMessageBehavior(), qSL("truncate"));
    QCOMPARE(c.resources(), QStringList({ qSL("r1"), qSL("r2"), qSL("r3") }));

    QCOMPARE(c.openGLConfiguration(), QVariantMap
             ({
                  { qSL("desktopProfile"), qSL("classic") },
                  { qSL("esMajorVersion"), 1 },
                  { qSL("esMinorVersion"), 0 },
              }));

    QCOMPARE(c.installationLocations(), {});

    QList<QPair<QString, QString>> containerSelectionConfiguration {
        { qSL("*"), qSL("selectionFunction") },
        { qSL("2"), qSL("second") }
    };
    QCOMPARE(c.containerSelectionConfiguration(), containerSelectionConfiguration);
    QCOMPARE(c.containerConfigurations(), QVariantMap
             ({
                  { qSL("c-test"), QVariantMap {
                        {  qSL("c-parameter"), qSL("xc-value") },
                    } },
                  { qSL("c-test2"), QVariantMap {
                        {  qSL("c-parameter2"), qSL("c-value2") },
                    } }

              }));

    QCOMPARE(c.runtimeConfigurations(), QVariantMap
             ({
                  { qSL("r-test"), QVariantMap {
                        {  qSL("r-parameter"), qSL("xr-value") },
                    } },
                  { qSL("r-test2"), QVariantMap {
                        {  qSL("r-parameter2"), qSL("r-value2") },
                    } }

              }));
    QCOMPARE(c.runtimeAdditionalLaunchers(), QStringList({ qSL("a"), qSL("b"), qSL("c") }));

    QCOMPARE(c.dbusRegistration("iface1"), qSL("foobus1"));
    QCOMPARE(c.dbusRegistration("iface2"), qSL("foobus2"));

    QCOMPARE(c.rawSystemProperties(), QVariantMap
             ({
                  { qSL("public"), QVariantMap {
                        {  qSL("public-prop"), qSL("xpublic-value") },
                        {  qSL("public-prop2"), qSL("public-value2") }
                    } },
                  { qSL("protected"), QVariantMap {
                        {  qSL("protected-prop"), qSL("xprotected-value") },
                        {  qSL("protected-prop2"), qSL("protected-value2") }
                    } },
                  { qSL("private"), QVariantMap {
                        {  qSL("private-prop"), qSL("xprivate-value") },
                        {  qSL("private-prop2"), qSL("private-value2") }
                    } }
              }));

    QCOMPARE(c.quickLaunchIdleLoad(), qreal(0.2));
    QHash<std::pair<QString, QString>, int> rpc = {
        { { qSL("*"), qSL("*") }, 5 },
        { { qSL("c-foo"), qSL("r-foo") }, 1 },
        { { qSL("c-foo"), qSL("r-bar") }, 2 },
        { { qSL("c-bar"), qSL("*") }, 4 },
    };
    QCOMPARE(c.quickLaunchRuntimesPerContainer(), rpc);
    QCOMPARE(c.quickLaunchFailedStartLimit(), 44);
    QCOMPARE(c.quickLaunchFailedStartLimitIntervalSec(), 45);

    QCOMPARE(c.waylandSocketName(), qSL("other-wlsock-0"));

    QCOMPARE(c.waylandExtraSockets(), QVariantList
             ({
                  QVariantMap {
                      { qSL("path"), qSL("path-es1") },
                      { qSL("permissions"), 0440 },
                      { qSL("userId"), 1 },
                      { qSL("groupId"), 2 }
                  },
                  QVariantMap {
                      { qSL("path"), qSL("path-es2") },
                      { qSL("permissions"), 0222 },
                      { qSL("userId"), 3 },
                      { qSL("groupId"), 4 }
                  },
                  QVariantMap {
                      { qSL("path"), qSL("path-es3") },
                  }
              }));

    QCOMPARE(c.managerCrashAction(), QVariantMap
             ({
                  { qSL("printBacktrace"), true },
                  { qSL("printQmlStack"), true },
                  { qSL("waitForGdbAttach"), true },
                  { qSL("dumpCore"), true }
              }));

    QCOMPARE(c.caCertificates(), QStringList({ qSL("cert1"), qSL("cert2"), qSL("cert3") }));

    QCOMPARE(c.pluginFilePaths("container"), QStringList({ qSL("c1"), qSL("c2"), qSL("c3"), qSL("c4") }));
    QCOMPARE(c.pluginFilePaths("startup"), QStringList({ qSL("s1"), qSL("s2"), qSL("s3") }));
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
    for (const auto &c : std::as_const(commandLine))
        strCommandLine << QString::fromLatin1(c);

    c.parseWithArguments(strCommandLine);

    QVERIFY(c.noCache());

    // command line only
    QCOMPARE(c.noFullscreen(), true);
    QCOMPARE(c.verbose(), true);
    QCOMPARE(c.slowAnimations(), true);
    QCOMPARE(c.noDltLogging(), true);
    QCOMPARE(c.singleApp(), qSL("appname"));
    QCOMPARE(c.qmlDebugging(), true);

    // values from config file
    QCOMPARE(c.mainQmlFile(), qSL("main-cl.qml"));

    QCOMPARE(c.builtinAppsManifestDirs(), QStringList({ qSL("builtin-dir-cl1"), qSL("builtin-dir-cl2") }));
    QCOMPARE(c.documentDir(), qSL("document-dir-cl"));

    QCOMPARE(c.installationDir(), qSL("installation-dir-cl"));
    QCOMPARE(c.disableInstaller(), true);
    QCOMPARE(c.disableIntents(), true);
    QCOMPARE(c.intentTimeoutForDisambiguation(), 10000);
    QCOMPARE(c.intentTimeoutForStartApplication(), 3000);
    QCOMPARE(c.intentTimeoutForReplyFromApplication(), 5000);
    QCOMPARE(c.intentTimeoutForReplyFromSystem(), 20000);

    QCOMPARE(c.fullscreen(), false);
    QCOMPARE(c.windowIcon(), qSL(""));
    QCOMPARE(c.importPaths(), QStringList({ pwd.absoluteFilePath(qSL("ip-cl1")),
                                            pwd.absoluteFilePath(qSL("ip-cl2")) }));
    QCOMPARE(c.pluginPaths(), {});
    QCOMPARE(c.loadDummyData(), true);
    QCOMPARE(c.noSecurity(), true);
    QCOMPARE(c.developmentMode(), true);
    QCOMPARE(c.noUiWatchdog(), true);
    QCOMPARE(c.forceSingleProcess(), true);
    QCOMPARE(c.forceMultiProcess(), true);
    QCOMPARE(c.loggingRules(), QStringList({ qSL("cl-lr1"), qSL("cl-lr2") }));
    QCOMPARE(c.messagePattern(), qSL(""));
    QCOMPARE(c.useAMConsoleLogger(), QVariant());
    QCOMPARE(c.style(), qSL(""));
    QCOMPARE(c.iconThemeName(), qSL(""));
    QCOMPARE(c.iconThemeSearchPaths(), {});
    QCOMPARE(c.dltId(), qSL(""));
    QCOMPARE(c.dltDescription(), qSL(""));
    QCOMPARE(c.resources(), {});

    QCOMPARE(c.openGLConfiguration(), QVariantMap {});

    QCOMPARE(c.installationLocations(), {});

    QCOMPARE(c.containerSelectionConfiguration(), {});
    QCOMPARE(c.containerConfigurations(), QVariantMap{});
    QCOMPARE(c.runtimeConfigurations(), QVariantMap{});
    QCOMPARE(c.runtimeAdditionalLaunchers(), QStringList{});

    QCOMPARE(c.dbusRegistration("iface1"), qSL("system"));

    QCOMPARE(c.rawSystemProperties(), QVariantMap {});

    QCOMPARE(c.quickLaunchIdleLoad(), qreal(0));
    QVERIFY(c.quickLaunchRuntimesPerContainer().isEmpty());

    QCOMPARE(c.waylandSocketName(), qSL("wlsock-1"));
    QCOMPARE(c.waylandExtraSockets(), {});

    QCOMPARE(c.managerCrashAction(), QVariantMap {});

    QCOMPARE(c.caCertificates(), {});

    QCOMPARE(c.pluginFilePaths("container"), {});
    QCOMPARE(c.pluginFilePaths("startup"), {});
}


QTEST_MAIN(tst_Configuration)

#include "tst_configuration.moc"
