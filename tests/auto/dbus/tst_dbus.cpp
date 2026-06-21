// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtTest>
#include <QtDBus>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include <sys/prctl.h>
#include <signal.h>

#include "global.h"
#include "main.h"
#include "applicationmanager.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "packagemanager.h"
#include "windowmanager.h"
#include "notificationmanager.h"
#include "intentclient.h"
#include "intentclientrequest.h"
#include "configuration.h"
#include "exception.h"
#include "utilities.h"
#include "dbus-utilities.h"

#include "../error-checking.h"

using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM


class tst_DBus : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void applicationManager();
    void packageManager();
    void packageInstallation();
    void windowManager();
    void notificationManager();

private:
    bool startPrivateDBusDaemon();

    int m_argc = 0;
    char **m_argv = nullptr;
    QProcess *m_dbusDaemon = nullptr;
    Main *m_main = nullptr;
    Configuration *m_config = nullptr;
    bool m_mainSetupDone = false;
};

// Starts a private dbus-daemon and points DBUS_SESSION_BUS_ADDRESS at it, so the test runs
// against an isolated session bus instead of the developer's / CI's real one.
bool tst_DBus::startPrivateDBusDaemon()
{
    qunsetenv("DBUS_SESSION_BUS_ADDRESS");

    m_dbusDaemon = new QProcess;
    m_dbusDaemon->setProgram(u"dbus-daemon"_s);
    m_dbusDaemon->setArguments({ u"--nofork"_s, u"--print-address"_s, u"--session"_s });
    // make sure the daemon dies with the test process
    m_dbusDaemon->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGKILL); });
    m_dbusDaemon->start(QIODevice::ReadOnly);

    const int timeout = 10000 * int(timeoutFactor());
    if (!m_dbusDaemon->waitForStarted(timeout) || !m_dbusDaemon->waitForReadyRead(timeout))
        return false;

    qputenv("DBUS_SESSION_BUS_ADDRESS", m_dbusDaemon->readAllStandardOutput().trimmed());
    return true;
}

void tst_DBus::initTestCase()
{
    QVERIFY2(startPrivateDBusDaemon(), "could not start a private dbus-daemon");
    QVERIFY(QDBusConnection::sessionBus().isConnected());

    m_argc = 2;
    m_argv = new char * [size_t(m_argc) + 1];
    m_argv[0] = qstrdup("tst_dbus");
    m_argv[1] = qstrdup("--no-cache");
    m_argv[m_argc] = nullptr;

    m_main = new Main(m_argc, m_argv);  // QCoreApplication saves a reference to argc!

    m_config = new Configuration({ QFINDTESTDATA("am-config.yaml") }, QString());
    m_config->parseWithArguments(QCoreApplication::arguments());
    if (amVerboseTest())
        m_config->setForceVerbose(true);

    try {
        m_main->setup(m_config);
        m_mainSetupDone = true;
        m_main->loadQml();
        m_main->showWindow();
        if (qEnvironmentVariableIsSet("AM_BACKGROUND_TEST") && !qApp->topLevelWindows().isEmpty()) {
            QWindow *w = qApp->topLevelWindows().first();
            w->setFlag(Qt::WindowStaysOnBottomHint);
            w->setFlag(Qt::WindowDoesNotAcceptFocus);
        }
        // wait until the System-UI window is fully shown: only then is its compositor view
        // registered (on the platform-surface-created event), which WindowManager needs e.g. for
        // makeScreenshot of the compositor window
        QVERIFY(!qApp->topLevelWindows().isEmpty());
        QVERIFY(QTest::qWaitForWindowExposed(qApp->topLevelWindows().first()));
    } catch (const Exception &e) {
        QVERIFY2(false, e.what());
    }
}

void tst_DBus::cleanupTestCase()
{
    if (m_main) {
        if (m_mainSetupDone) {
            m_main->shutDown({});
            m_main->exec();
        }
        delete m_main;
    }
    if (m_config)
        delete m_config;
    if (m_argc && m_argv) {
        for (int i = 0; i < m_argc; ++i)
            delete [] m_argv[i];
        delete [] m_argv;
    }
    if (m_dbusDaemon) {
        m_dbusDaemon->kill();
        m_dbusDaemon->waitForFinished();
        delete m_dbusDaemon;
    }
}

// Spies on a D-Bus signal addressed by name (QSignalSpy cannot target a string signal on a
// QDBusInterface directly). Captures the arguments of every emission.
class DBusSignalSpy : public QObject
{
    Q_OBJECT
public:
    DBusSignalSpy(QDBusInterface &iface, const QString &signalName)
    {
        QDBusConnection::sessionBus().connect(iface.service(), iface.path(), iface.interface(),
                                              signalName, this, SLOT(onSignal(QDBusMessage)));
    }
    Q_INVOKABLE void onSignal(const QDBusMessage &msg) { m_emissions << msg.arguments(); }

    bool wait(int count = 1)
    {
        QElapsedTimer t;
        t.start();
        while ((m_emissions.count() < count) && (t.elapsed() < 5000 * int(timeoutFactor())))
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
        return m_emissions.count() >= count;
    }
    QList<QVariantList> m_emissions;
};

template <typename R, typename ...Args>
static R dbusCall(QDBusInterface &iface, const QString &call, Args ...args)
{
    QDBusPendingReply<R> reply = iface.asyncCall(call, QVariant::fromValue(args)...);
    QDBusPendingCallWatcher watcher(reply);
    QSignalSpy finishedSpy(&watcher, &QDBusPendingCallWatcher::finished);
    QVERIFY2(finishedSpy.wait(5000 * int(timeoutFactor())), "timed out waiting for the D-Bus reply");
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));

    if constexpr (std::is_same_v<R, void>)
        return;
    else
        return reply.value();
}

// Issues a call and returns whether it completed with a D-Bus error (for negative testing).
// Uses the async watcher (a blocking call would deadlock in the same process).
template <typename ...Args>
static bool dbusCallErrors(QDBusInterface &iface, const QString &call, Args ...args)
{
    QDBusPendingReply<> reply = iface.asyncCall(call, QVariant::fromValue(args)...);
    QDBusPendingCallWatcher watcher(reply);
    QSignalSpy finishedSpy(&watcher, &QDBusPendingCallWatcher::finished);
    if (!finishedSpy.wait(5000 * int(timeoutFactor())))
        return false; // timed out -> treat as "did not error" so the caller's QVERIFY fails
    return reply.isError();
}

void tst_DBus::applicationManager()
{
    QDBusInterface am(u"io.qt.ApplicationManager"_s, u"/ApplicationManager"_s,
                      u"io.qt.ApplicationManager"_s, QDBusConnection::sessionBus());
    QVERIFY2(am.isValid(), qPrintable(am.lastError().message()));

    // property reads over D-Bus must reflect the actual System-UI state
    auto *amInstance = ApplicationManager::instance();
    QCOMPARE(am.property("count").toInt(), 3);
    QCOMPARE(am.property("singleProcess").toBool(), amInstance->isSingleProcess());
    QCOMPARE(am.property("securityChecksEnabled").toBool(), amInstance->securityChecksEnabled());
    QCOMPARE(am.property("dummy").toBool(), amInstance->isDummy());
    QCOMPARE(am.property("windowManagerCompositorReady").toBool(),
             amInstance->isWindowManagerCompositorReady());

    // systemProperties (an a{sv} map) carry the values configured in am-config.yaml
    const QVariantMap sysProps = am.property("systemProperties").toMap();
    QCOMPARE(sysProps.value(u"aString"_s).toString(), u"hello"_s);
    QCOMPARE(sysProps.value(u"anInt"_s).toInt(), 42);
    QCOMPARE(sysProps.value(u"aBool"_s).toBool(), true);

    // method without arguments
    QCOMPARE(dbusCall<QStringList>(am, u"applicationIds"_s),
             QStringList({ u"test.dbus.app1"_s, u"test.dbus.app2"_s, u"test.dbus.notifier"_s }));

    // method with an argument
    QCOMPARE(dbusCall<QStringList>(am, u"capabilities"_s, u"test.dbus.app2"_s),
             QStringList({ u"cap-one"_s, u"cap-two"_s }));
    QCOMPARE(dbusCall<QStringList>(am, u"capabilities"_s, u"test.dbus.app1"_s),
             QStringList());

    // get() returns a map of application data
    auto appData = dbusCall<QVariantMap>(am, u"get"_s, u"test.dbus.app1"_s);
    QCOMPARE(appData.value(u"applicationId"_s).toString(), u"test.dbus.app1"_s);

    // identifyApplication: a pid that is not an application's process resolves to no id
    QCOMPARE(dbusCall<QString>(am, u"identifyApplication"_s, qint64(1)), QString());

    // applicationRunState reflects the lifecycle; the app is not running yet
    QCOMPARE(dbusCall<uint>(am, u"applicationRunState"_s, u"test.dbus.app1"_s),
             uint(Am::NotRunning));

    // a side-effecting call plus the resulting signal, spied by name
    DBusSignalSpy runStateSpy(am, u"applicationRunStateChanged"_s);
    dbusCall<bool>(am, u"startApplication"_s, u"test.dbus.app1"_s);
    QVERIFY(runStateSpy.wait());
    QCOMPARE(runStateSpy.m_emissions.first().at(0).toString(), u"test.dbus.app1"_s);

    // applicationRunState now sees the app starting up or running
    QTRY_VERIFY(dbusCall<uint>(am, u"applicationRunState"_s, u"test.dbus.app1"_s) == uint(Am::Running));

    // identifyApplication: now there should be a pid in multi-process mode
    if (!ApplicationManager::instance()->isSingleProcess()) {
        const Application *app = ApplicationManager::instance()->fromId(u"test.dbus.app1"_s);
        QVERIFY(app);
        QVERIFY(app->currentRuntime());
        QVERIFY(app->currentRuntime()->container());
        QVERIFY(app->currentRuntime()->container()->process());
        const qint64 pid = app->currentRuntime()->container()->process()->processId();
        QVERIFY(pid > 0);

        const auto appId = dbusCall<QString>(am, u"identifyApplication"_s, pid);
        const auto allAppIds = dbusCall<QStringList>(am, u"identifyAllApplications"_s, pid);
        QCOMPARE(appId, u"test.dbus.app1"_s);
        QCOMPARE(allAppIds, QStringList({ u"test.dbus.app1"_s }));
    }

    // stop the app again
    dbusCall<void>(am, u"stopApplication"_s, u"test.dbus.app1"_s);
    QTRY_VERIFY(dbusCall<uint>(am, u"applicationRunState"_s, u"test.dbus.app1"_s) == uint(Am::NotRunning));

    // doesn't do anything at this, but we need to exercise all the overloads
    dbusCall<void>(am, u"stopApplication"_s, u"test.dbus.app1"_s, true);
    dbusCall<void>(am, u"stopAllApplications"_s);
    dbusCall<void>(am, u"stopAllApplications"_s, true);

    // the intent-request methods are only available on the development-mode bus, so on the regular
    // session bus they must fail with a D-Bus error
    QVERIFY(dbusCallErrors(am, u"sendIntentRequestAs"_s,
                           u"test.dbus.app1"_s, u"some.intent"_s, u"test.dbus.app2"_s, u"{}"_s));
    QVERIFY(dbusCallErrors(am, u"broadcastIntentRequestAs"_s,
                           u"test.dbus.app1"_s, u"some.intent"_s, u"{}"_s));
}

void tst_DBus::packageManager()
{
    QDBusInterface pm(u"io.qt.ApplicationManager"_s, u"/PackageManager"_s,
                      u"io.qt.PackageManager"_s, QDBusConnection::sessionBus());
    QVERIFY2(pm.isValid(), qPrintable(pm.lastError().message()));

    // there is one package per built-in application
    QCOMPARE(pm.property("count").toInt(), 3);

    QCOMPARE(dbusCall<QStringList>(pm, u"packageIds"_s),
             QStringList({ u"test.dbus.app1"_s, u"test.dbus.app2"_s, u"test.dbus.notifier"_s }));

    auto pkgData = dbusCall<QVariantMap>(pm, u"get"_s, u"test.dbus.app1"_s);
    QCOMPARE(pkgData.value(u"packageId"_s).toString(), u"test.dbus.app1"_s);

    // all properties, compared against the actual PackageManager state
    auto *pmInstance = PackageManager::instance();
    QCOMPARE(pm.property("architecture").toString(), pmInstance->architecture());
    QCOMPARE(pm.property("hardwareId").toString(), pmInstance->hardwareId());
    QCOMPARE(pm.property("installationEnabled").toBool(), pmInstance->installationEnabled());
    QCOMPARE(pm.property("allowInstallationOfUnsignedPackages").toBool(),
             pmInstance->allowInstallationOfUnsignedPackages());
    QCOMPARE(pm.property("ready").toBool(), pmInstance->isReady());
    // developmentMode is a C++ enum exposed as a string over D-Bus
    QVERIFY(!pm.property("developmentMode").toString().isEmpty());
    // developerCertificate is a variant; just make sure it is readable without a D-Bus error
    QCOMPARE(convertFromDBusVariant(pm.property("developerCertificate")),
             pmInstance->developerCertificate().toVariant());
    // the location maps carry at least a 'path' entry, matching the singleton
    QCOMPARE(pm.property("installationLocation").toMap().value(u"path"_s),
             pmInstance->installationLocation().value(u"path"_s));
    QCOMPARE(pm.property("documentLocation").toMap().value(u"path"_s),
             pmInstance->documentLocation().value(u"path"_s));

    // compareVersions: one ordering and one equality
    QCOMPARE(dbusCall<int>(pm, u"compareVersions"_s, u"1.0"_s, u"2.0"_s), -1);
    QCOMPARE(dbusCall<int>(pm, u"compareVersions"_s, u"3.0"_s, u"3.0"_s), 0);

    // validateDnsName:
    QCOMPARE(dbusCall<bool>(pm, u"validateDnsName"_s, u"com.example.app"_s, 3), true);
    QCOMPARE(dbusCall<bool>(pm, u"validateDnsName"_s, u"notadnsname"_s, 3), false);
    QCOMPARE(dbusCall<bool>(pm, u"validateDnsName"_s, u"shortname"_s), true);
}

void tst_DBus::packageInstallation()
{
    if (!QDir(QString::fromLatin1(AM_TESTDATA_DIR "/packages")).exists())
        QSKIP("No test packages available in the data/ directory");

    QDBusInterface pm(u"io.qt.ApplicationManager"_s, u"/PackageManager"_s,
                      u"io.qt.PackageManager"_s, QDBusConnection::sessionBus());
    QVERIFY2(pm.isValid(), qPrintable(pm.lastError().message()));

    const QString pkgId = u"test-pkg"_s;
    const QString pkgUrl = u"file://"_s AM_TESTDATA_DIR u"packages/test-extra-dev-signed.ampkg"_s;

    DBusSignalSpy blockingSpy(pm, u"taskBlockingUntilInstallationAcknowledge"_s);
    DBusSignalSpy finishedSpy(pm, u"taskFinished"_s);
    DBusSignalSpy failedSpy(pm, u"taskFailed"_s);

    // 1) start the installation
    const QString taskId = dbusCall<QString>(pm, u"startPackageInstallation"_s, pkgUrl);
    QVERIFY(!taskId.isEmpty());

    // 2) the task is active
    QVERIFY(dbusCall<QStringList>(pm, u"activeTaskIds"_s).contains(taskId));

    // wait until the task parks on the acknowledge wait-condition: now it is safe to query it
    // (the package header has been parsed) and it stays put until we acknowledge
    QVERIFY(blockingSpy.wait());
    QCOMPARE(blockingSpy.m_emissions.first().at(0).toString(), taskId);

    // 3) + 4) the task knows its state and target package id
    QVERIFY(!dbusCall<QString>(pm, u"taskState"_s, taskId).isEmpty());
    QCOMPARE(dbusCall<QString>(pm, u"taskPackageId"_s, taskId), pkgId);

    // 5) acknowledge the installation
    dbusCall<void>(pm, u"acknowledgePackageInstallation"_s, taskId);

    // 6) wait for it to finish successfully
    QVERIFY(finishedSpy.wait());
    QCOMPARE(finishedSpy.m_emissions.first().at(0).toString(), taskId);
    QCOMPARE(failedSpy.m_emissions.count(), 0);
    QTRY_VERIFY(dbusCall<QStringList>(pm, u"packageIds"_s).contains(pkgId));

    // 7) the installed package reports a non-zero size
    QVERIFY(dbusCall<qlonglong>(pm, u"installedPackageSize"_s, pkgId) > 0);

    // 8) the unsigned extra meta-data baked into the package
    const QVariantMap extra = dbusCall<QVariantMap>(pm, u"installedPackageExtraMetaData"_s, pkgId);
    QCOMPARE(extra.value(u"foo"_s).toString(), u"bar"_s);
    QCOMPARE(extra.value(u"foo2"_s).toString(), u"bar2"_s);
    QCOMPARE(extra.value(u"key"_s).toString(), u"value"_s);

    // 9) the signed extra meta-data
    const QVariantMap extraSigned = dbusCall<QVariantMap>(pm, u"installedPackageExtraSignedMetaData"_s, pkgId);
    QCOMPARE(extraSigned.value(u"sfoo"_s).toString(), u"sbar"_s);
    QCOMPARE(extraSigned.value(u"signed-key"_s).toString(), u"signed-value"_s);

    // 10) + 11) remove the package (no force) and wait for that task to finish
    finishedSpy.m_emissions.clear();
    const QString removeId = dbusCall<QString>(pm, u"removePackage"_s, pkgId, false);
    QVERIFY(!removeId.isEmpty());
    if (!finishedSpy.wait() || (finishedSpy.m_emissions.first().at(0).toString() != removeId)) {
        // 12) on error, force-remove and wait again
        finishedSpy.m_emissions.clear();
        const QString forceId = dbusCall<QString>(pm, u"removePackage"_s, pkgId, false, true);
        QVERIFY(!forceId.isEmpty());
        QVERIFY(finishedSpy.wait());
    }
    QTRY_VERIFY(!dbusCall<QStringList>(pm, u"packageIds"_s).contains(pkgId));

    // 13) cancelling the (long-finished) installation task must fail
    QVERIFY(dbusCallErrors(pm, u"cancelTask"_s, taskId));
}

void tst_DBus::windowManager()
{
    QDBusInterface wm(u"io.qt.ApplicationManager"_s, u"/WindowManager"_s,
                      u"io.qt.WindowManager"_s, QDBusConnection::sessionBus());
    QVERIFY2(wm.isValid(), qPrintable(wm.lastError().message()));

    auto *wmInstance = WindowManager::instance();

    // all read-only properties, compared against the actual System-UI state
    QCOMPARE(wm.property("count").toInt(), 0);
    QCOMPARE(wm.property("runningOnDesktop").toBool(), wmInstance->isRunningOnDesktop());
    QCOMPARE(wm.property("allowUnknownUiClients").toBool(), wmInstance->allowUnknownUiClients());

    // the read-write 'slowAnimations' property: set it over D-Bus, verify value + signal
    QVERIFY(!wm.property("slowAnimations").toBool());
    DBusSignalSpy slowSpy(wm, u"slowAnimationsChanged"_s);
    QVERIFY(wm.setProperty("slowAnimations", true));
    QVERIFY(slowSpy.wait());
    QCOMPARE(slowSpy.m_emissions.first().at(0).toBool(), true);
    QVERIFY(wm.property("slowAnimations").toBool());
    wm.setProperty("slowAnimations", false);

    // an empty selector captures the compositor (System-UI) window itself - no apps required
    QTemporaryDir screenshotDir;
    QVERIFY(screenshotDir.isValid());
    const QString sysuiShot = screenshotDir.filePath(u"sysui.png"_s);
    QCOMPARE(dbusCall<bool>(wm, u"makeScreenshot"_s, sysuiShot, QString()), true);
    QTRY_VERIFY(QFile::exists(sysuiShot));

    // starting an application brings up a window: count rises and countChanged fires
    DBusSignalSpy countSpy(wm, u"countChanged"_s);
    QVERIFY(ApplicationManager::instance()->startApplication(u"test.dbus.app1"_s));
    QVERIFY(countSpy.wait());
    QTRY_COMPARE(wm.property("count").toInt(), 1);

    // stopping the application removes the window again
    countSpy.m_emissions.clear();
    ApplicationManager::instance()->stopApplication(u"test.dbus.app1"_s);
    QVERIFY(countSpy.wait());
    QTRY_COMPARE(wm.property("count").toInt(), 0);
}

void tst_DBus::notificationManager()
{
    QDBusInterface nm(u"org.freedesktop.Notifications"_s, u"/org/freedesktop/Notifications"_s,
                      u"org.freedesktop.Notifications"_s, QDBusConnection::sessionBus());
    QVERIFY2(nm.isValid(), qPrintable(nm.lastError().message()));

    // the freedesktop spec surface
    QVERIFY(!dbusCall<QStringList>(nm, u"GetCapabilities"_s).isEmpty());

    // posting a notification returns a non-zero id. Use an external app_name: a *known* app id
    // would trigger the adaptor's caller-PID check (the call comes from the test, not the app).
    const QVariantMap hints = {
        { u"urgency"_s, uint(2) },
        { u"category"_s, u"test-category"_s },
    };
    uint id = dbusCall<uint>(nm, u"Notify"_s,
                             u"external.notifier"_s,                       // app_name
                             0u,                                           // replaces_id
                             u"my-icon"_s,                                 // app_icon
                             u"the summary"_s,                             // summary
                             u"the body"_s,                                // body
                             QStringList({ u"ok"_s, u"OK"_s,
                                           u"cancel"_s, u"Cancel"_s }),    // actions (key/label pairs)
                             hints,                                        // hints
                             3000);                                        // timeout
    QVERIFY(id != 0);

    // the data sent over D-Bus must have landed verbatim in the System-UI's NotificationManager.
    // The model is updated via a queued connection, so wait for the notification to appear.
    auto *nmInstance = NotificationManager::instance();
    QTRY_COMPARE(nmInstance->indexOfNotification(id) >= 0, true);

    const QVariantMap data = nmInstance->notification(id);
    QCOMPARE(data.value(u"id"_s).toUInt(), id);
    // an external client's app-id is namespaced with a ':ext:' prefix
    QCOMPARE(data.value(u"applicationId"_s).toString(), u":ext:external.notifier"_s);
    QCOMPARE(data.value(u"summary"_s).toString(), u"the summary"_s);
    QCOMPARE(data.value(u"body"_s).toString(), u"the body"_s);
    QCOMPARE(data.value(u"icon"_s).toString(), u"my-icon"_s);
    QCOMPARE(data.value(u"category"_s).toString(), u"test-category"_s);
    QCOMPARE(data.value(u"priority"_s).toUInt(), uint(2));
    QCOMPARE(data.value(u"timeout"_s).toInt(), 3000);
    // actions arrive as a list of { key: label } maps
    const QVariantList actions = data.value(u"actions"_s).toList();
    QCOMPARE(actions.size(), 2);
    QCOMPARE(actions.at(0).toMap().value(u"ok"_s).toString(), u"OK"_s);
    QCOMPARE(actions.at(1).toMap().value(u"cancel"_s).toString(), u"Cancel"_s);

    // closing it emits NotificationClosed with that id and removes it from the manager
    DBusSignalSpy closedSpy(nm, u"NotificationClosed"_s);
    dbusCall<void>(nm, u"CloseNotification"_s, id);
    QVERIFY(closedSpy.wait());
    QCOMPARE(closedSpy.m_emissions.first().at(0).toUInt(), id);
    QTRY_COMPARE(nmInstance->indexOfNotification(id), -1);

    // triggering an action (as the System UI would) forwards it to the client via the
    // ActionInvoked D-Bus signal
    uint id2 = dbusCall<uint>(nm, u"Notify"_s, u"external.notifier"_s, 0u, QString(),
                              u"s2"_s, u"b2"_s,
                              QStringList({ u"ok"_s, u"OK"_s }), QVariantMap(), 3000);
    QVERIFY(id2 != 0);
    QTRY_VERIFY(nmInstance->indexOfNotification(id2) >= 0);

    DBusSignalSpy actionSpy(nm, u"ActionInvoked"_s);
    DBusSignalSpy actionClosedSpy(nm, u"NotificationClosed"_s);
    nmInstance->triggerNotificationAction(id2, u"ok"_s);
    QVERIFY(actionSpy.wait());
    QCOMPARE(actionSpy.m_emissions.first().at(0).toUInt(), id2);
    QCOMPARE(actionSpy.m_emissions.first().at(1).toString(), u"ok"_s);

    // dismissOnAction defaults to true, so the action also closes the notification: the client
    // must receive a NotificationClosed for it, and it is gone from the manager
    QVERIFY(actionClosedSpy.wait());
    QCOMPARE(actionClosedSpy.m_emissions.first().at(0).toUInt(), id2);
    QTRY_COMPARE(nmInstance->indexOfNotification(id2), -1);

    // GetServerInformation returns four out-arguments (name, vendor, version, spec_version)
    QDBusPendingReply<QString, QString, QString, QString> info = nm.asyncCall(u"GetServerInformation"_s);
    QDBusPendingCallWatcher infoWatcher(info);
    QSignalSpy infoSpy(&infoWatcher, &QDBusPendingCallWatcher::finished);
    QVERIFY(infoSpy.wait(5000 * int(timeoutFactor())));
    QVERIFY2(!info.isError(), qPrintable(info.error().message()));
    QCOMPARE(info.argumentAt<0>(), qApp->applicationName());     // name
    QCOMPARE(info.argumentAt<1>(), qApp->organizationName());    // vendor
    QCOMPARE(info.argumentAt<2>(), u"1.0"_s);                    // version
    QCOMPARE(info.argumentAt<3>(), u"1.2"_s);                    // spec_version

    // Security feature: the Notifications adaptor only accepts a notification for a *known* app id
    // if the D-Bus caller's PID actually matches that app's process. Drive the calls from inside a
    // real app's process (via the NotifyHelper QML module) and check both the matching and the
    // non-matching case. (only meaningful in multi-process mode, where apps have their own pid)
    if (ApplicationManager::instance()->isSingleProcess())
        return;

    auto *amInstance = ApplicationManager::instance();
    QVERIFY(amInstance->startApplication(u"test.dbus.notifier"_s));
    QTRY_COMPARE(amInstance->applicationRunState(u"test.dbus.notifier"_s), Am::Running);

    // asks the notifier app to post a notification under 'asAppName' and returns the id it got back
    auto notifyFromApp = [](const QString &asAppName) -> uint {
        auto *req = IntentClient::instance()->sendIntentRequest(u"notify-request"_s,
            u"test.dbus.notifier"_s, { { u"appName"_s, asAppName }, { u"summary"_s, u"s"_s } });
        if (!req)
            return 0;
        QSignalSpy replySpy(req, &IntentClientRequest::replyReceived);
        if (!replySpy.wait(5000 * int(timeoutFactor())) || !req->succeeded())
            return 0;
        return req->result().value(u"id"_s).toUInt();
    };

    // matching: the app posts under its own id -> the caller-pid check passes -> accepted
    uint okId = notifyFromApp(u"test.dbus.notifier"_s);
    QVERIFY(okId != 0);
    QTRY_VERIFY(nmInstance->indexOfNotification(okId) >= 0);
    // a known, matching app is not namespaced with the ':ext:' prefix
    QCOMPARE(nmInstance->notification(okId).value(u"applicationId"_s).toString(),
             u"test.dbus.notifier"_s);

    // non-matching: the app tries to post under a *different* app's id -> rejected -> id 0
    QCOMPARE(notifyFromApp(u"test.dbus.app1"_s), uint(0));

    amInstance->stopApplication(u"test.dbus.notifier"_s);
}

QT_AM_VERBOSE_TEST_MAIN(tst_DBus)

#include "tst_dbus.moc"
