/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include <memory>
#include <qglobal.h>

// include as eary as possible, since the <windows.h> header will re-#define "interface"
#if defined(QT_DBUS_LIB)
#  include <QDBusConnection>
#  if defined(Q_OS_LINUX)
#    include <sys/prctl.h>
#    include <sys/signal.h>
#  endif
#endif

#include <QFile>
#include <QDir>
#include <QStringList>
#include <QVariant>
#include <QFileInfo>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QUrl>
#include <QLibrary>
#include <QFunctionPointer>
#include <QProcess>
#include <QQmlDebuggingEnabler>
#include <private/qabstractanimation_p.h>

#if !defined(AM_HEADLESS)
#  include <QGuiApplication>
#  include <QQuickView>
#  include <QQuickItem>
#endif

#if defined(QT_PSHELLSERVER_LIB)
#  include <PShellServer/PTelnetServer>
#  include <PShellServer/PAbstractShell>
#  include <PShellServer/PDeclarativeShell>
#endif

#if defined(QT_PSSDP_LIB)
#  include <PSsdp/PSsdpService>
#endif

#include "global.h"

#include "application.h"
#include "applicationmanager.h"
#include "applicationdatabase.h"
#include "installationreport.h"
#include "yamlapplicationscanner.h"
#if !defined(AM_DISABLE_INSTALLER)
#  include "applicationinstaller.h"
#  include "sudo.h"
#endif
#if defined(QT_DBUS_LIB)
#  include "applicationmanager_adaptor.h"
#  include "applicationinstaller_adaptor.h"
#  include "notifications_adaptor.h"
#endif
#include "runtimefactory.h"
#include "containerfactory.h"
#include "quicklauncher.h"
#include "nativeruntime.h"
#include "processcontainer.h"
#include "plugincontainer.h"
#include "notificationmanager.h"
#include "qmlinprocessruntime.h"
#include "qmlinprocessapplicationinterface.h"
#include "qml-utilities.h"
#include "dbus-utilities.h"

#if !defined(AM_HEADLESS)
#  include "windowmanager.h"
#  include "fakeapplicationmanagerwindow.h"
#  if defined(QT_DBUS_LIB)
#    include "windowmanager_adaptor.h"
#  endif
#endif

#include "configuration.h"
#include "utilities.h"
#include "qmllogger.h"
#include "startuptimer.h"
#include "systemmonitor.h"
#include "applicationipcmanager.h"
#include "unixsignalhandler.h"

#include "../plugin-interfaces/startupinterface.h"

#ifdef AM_TESTRUNNER
#include "testrunner.h"
#include "qtyaml.h"
#endif

QT_BEGIN_NAMESPACE_AM

static Configuration *configuration = 0;


#if defined(QT_DBUS_LIB)

static QString dbusInterfaceName(QObject *o) throw (Exception)
{
    int idx = o->metaObject()->indexOfClassInfo("D-Bus Interface");
    if (idx < 0) {
        throw Exception("Could not get class-info \"D-Bus Interface\" for D-Bus adapter %1")
            .arg(o->metaObject()->className());
    }
    return QLatin1String(o->metaObject()->classInfo(idx).value());
}

static void registerDBusObject(QDBusAbstractAdaptor *adaptor, const char *serviceName, const char *path) throw (Exception)
{
    QString interfaceName = dbusInterfaceName(adaptor);
    QString dbusName = configuration->dbusRegistration(interfaceName);
    QString dbusAddress;
    QDBusConnection conn((QString()));

    if (dbusName.isEmpty()) {
        return;
    } else if (dbusName == qL1S("system")) {
        dbusAddress = qgetenv("DBUS_SYSTEM_BUS_ADDRESS");
#if defined(Q_OS_LINUX)
        if (dbusAddress.isEmpty())
            dbusAddress = qL1S("unix:path=/var/run/dbus/system_bus_socket");
#endif
        conn = QDBusConnection::systemBus();
    } else if (dbusName == qL1S("session")) {
        dbusAddress = qgetenv("DBUS_SESSION_BUS_ADDRESS");
        conn = QDBusConnection::sessionBus();
    } else {
        dbusAddress = dbusName;
        conn = QDBusConnection::connectToBus(dbusAddress, qSL("custom"));
    }

    if (!conn.isConnected()) {
        throw Exception("could not connect to D-Bus (%1): %2")
                .arg(dbusAddress.isEmpty() ? dbusName : dbusAddress).arg(conn.lastError().message());
    }

    if (adaptor->parent()) {
        // we need this information later on to tell apps where services are listening
        adaptor->parent()->setProperty("_am_dbus_name", dbusName);
        adaptor->parent()->setProperty("_am_dbus_address", dbusAddress);
    }

    if (!conn.registerObject(qL1S(path), adaptor->parent(), QDBusConnection::ExportAdaptors)) {
        throw Exception("could not register object %1 on D-Bus (%2): %3")
                .arg(path).arg(dbusName).arg(conn.lastError().message());
    }

    if (!conn.registerService(qL1S(serviceName))) {
        throw Exception("could not register service %1 on D-Bus (%2): %3")
                .arg(serviceName).arg(dbusName).arg(conn.lastError().message());
    }

    qCDebug(LogSystem).nospace().noquote() << " * " << serviceName << path << " [on bus: " << dbusName << "]";

    if (interfaceName.startsWith(qL1S("io.qt."))) {
        // Write the bus address of the interface to a file in /tmp. This is needed for the
        // controller tool, which does not even have a session bus, when started via ssh.

        QFile f(QDir::temp().absoluteFilePath(interfaceName + qSL(".dbus")));
        QByteArray dbusUtf8 = dbusAddress.isEmpty() ? dbusName.toUtf8() : dbusAddress.toUtf8();
        if (!f.open(QFile::WriteOnly | QFile::Truncate) || (f.write(dbusUtf8) != dbusUtf8.size()))
            throw Exception(f, "Could not write D-Bus address of interface %1").arg(interfaceName);

        static QStringList filesToDelete;
        if (filesToDelete.isEmpty())
            atexit([]() { foreach (const QString &ftd, filesToDelete) QFile::remove(ftd); });
        filesToDelete << f.fileName();
    }

    static bool once = false;
    if (!once) {
        once = true;
        registerDBusTypes();
    }
}

static void dbusInitialization()
{
    try {
        qCDebug(LogSystem) << "Registering D-Bus services:";

        auto am = ApplicationManager::instance();
        auto ama = new ApplicationManagerAdaptor(am);

        // connect this signal manually, since it needs a type conversion
        // (the automatic signal relay fails in this case)
        QObject::connect(am, &ApplicationManager::applicationRunStateChanged,
                         ama, &ApplicationManagerAdaptor::applicationRunStateChanged);
        registerDBusObject(ama, "io.qt.ApplicationManager", "/ApplicationManager");
        if (!am->setDBusPolicy(configuration->dbusPolicy(dbusInterfaceName(am))))
            throw Exception(Error::DBus, "could not set DBus policy for ApplicationManager");

#  if !defined(AM_DISABLE_INSTALLER)
        auto ai = ApplicationInstaller::instance();
        registerDBusObject(new ApplicationInstallerAdaptor(ai), "io.qt.ApplicationManager", "/ApplicationInstaller");
        if (!ai->setDBusPolicy(configuration->dbusPolicy(dbusInterfaceName(ai))))
            throw Exception(Error::DBus, "could not set DBus policy for ApplicationInstaller");
#  endif

#  if !defined(AM_HEADLESS)
        try {
            auto nm = NotificationManager::instance();
            registerDBusObject(new NotificationsAdaptor(nm), "org.freedesktop.Notifications", "/org/freedesktop/Notifications");
        } catch (const Exception &e) {
            //TODO: what should we do here? on the desktop this will obviously always fail
            qCCritical(LogSystem) << "WARNING:" << e.what();
        }
        auto wm = WindowManager::instance();
        registerDBusObject(new WindowManagerAdaptor(wm), "io.qt.ApplicationManager", "/WindowManager");
        if (!wm->setDBusPolicy(configuration->dbusPolicy(dbusInterfaceName(wm))))
            throw Exception(Error::DBus, "could not set DBus policy for WindowManager");
#endif
    } catch (const std::exception &e) {
        qCCritical(LogSystem) << "ERROR:" << e.what();
        qApp->exit(2);
    }
}

#endif // QT_DBUS_LIB


// copied straight from Qt 5.1.0 qmlscene/main.cpp for now - needs to be revised
static void loadDummyDataFiles(QQmlEngine &engine, const QString& directory)
{
    QDir dir(directory + qSL("/dummydata"), qSL("*.qml"));
    QStringList list = dir.entryList();
    for (int i = 0; i < list.size(); ++i) {
        QString qml = list.at(i);
        QQmlComponent comp(&engine, dir.filePath(qml));
        QObject *dummyData = comp.create();

        if (comp.isError()) {
            QList<QQmlError> errors = comp.errors();
            foreach (const QQmlError &error, errors)
                qWarning() << error;
        }

        if (dummyData) {
            qWarning() << "Loaded dummy data:" << dir.filePath(qml);
            qml.truncate(qml.length()-4);
            engine.rootContext()->setContextProperty(qml, dummyData);
            dummyData->setParent(&engine);
        }
    }
}

#if defined(QT_PSHELLSERVER_LIB)

struct AMShellFactory : public PAbstractShellFactory
{
public:
    AMShellFactory(QQmlEngine *engine, QObject *object)
        : m_engine(engine)
        , m_object(object)
    {
        Q_ASSERT(engine);
        Q_ASSERT(object);
    }

    PAbstractShell *create(QObject *parent)
    {
        return new PDeclarativeShell(m_engine, m_object, parent);
    }

private:
    QQmlEngine *m_engine;
    QObject *m_object;
};

#endif // QT_PSHELLSERVER_LIB

static QVector<const Application *> scanForApplication(const QString &singleAppInfoYaml, const QStringList &builtinAppsDirs)
{
    QVector<const Application *> result;
    YamlApplicationScanner yas;

    QDir appDir = QFileInfo(singleAppInfoYaml).dir();

    QScopedPointer<Application> a(yas.scan(singleAppInfoYaml));
    Q_ASSERT(a);

    if (!RuntimeFactory::instance()->manager(a->runtimeName())) {
        qCDebug(LogSystem) << "Ignoring application" << a->id() << ", because it uses an unknown runtime:" << a->runtimeName();
        return result;
    }

    QStringList aliasPaths = appDir.entryList(QStringList(qSL("info-*.yaml")));
    std::vector<std::unique_ptr<Application>> aliases;

    for (int i = 0; i < aliasPaths.size(); ++i) {
        std::unique_ptr<Application> alias(yas.scanAlias(appDir.absoluteFilePath(aliasPaths.at(i)), a.data()));

        Q_ASSERT(alias);
        Q_ASSERT(alias->isAlias());
        Q_ASSERT(alias->nonAliased() == a.data());

        aliases.push_back(std::move(alias));
    }

    if (appDir.cdUp()) {
        for (const QString &dir : builtinAppsDirs) {
            if (appDir == QDir(dir)) {
                a->setBuiltIn(true);
                break;
            }
        }
    }

    result << a.take();
    for (auto &&alias : aliases)
        result << alias.release();

    return result;
}

#if !defined(AM_DISABLE_INSTALLER)
static QVector<const Application *> scanForApplications(const QStringList &builtinAppsDirs, const QString &installedAppsDir,
                                                        const QVector<InstallationLocation> &installationLocations)
{
#else
static QVector<const Application *> scanForApplications(const QStringList &builtinAppsDirs)
{
    int installationLocations; // dummy variable to get rid of #ifdef within lambda below
#endif

    QVector<const Application *> result;
    YamlApplicationScanner yas;

    auto scan = [&result, &yas, &installationLocations](const QDir &baseDir, bool scanningBuiltinApps) {
        auto flags = scanningBuiltinApps ? QDir::Dirs | QDir::NoDotAndDotDot
                                         : QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;

        foreach (const QString &appDirName, baseDir.entryList(flags)) {
            if (appDirName.endsWith('+') || appDirName.endsWith('-'))
                continue;
            if (!isValidDnsName(appDirName)) {
                qCDebug(LogSystem) << "Ignoring application directory" << appDirName << ", as it's not following the rdns convention";
                continue;
            }
            QDir appDir = baseDir.absoluteFilePath(appDirName);
            if (!appDir.exists())
                continue;
            if (!appDir.exists(qSL("info.yaml"))) {
                qCDebug(LogSystem) << "Couldn't find a info.yaml in:" << appDir;
                continue;
            }
            if (!scanningBuiltinApps && !appDir.exists(qSL("installation-report.yaml")))
                continue;

            QScopedPointer<Application> a(yas.scan(appDir.absoluteFilePath(qSL("info.yaml"))));
            Q_ASSERT(a);

            AbstractRuntimeManager *runtimeManager = RuntimeFactory::instance()->manager(a->runtimeName());
            if (!runtimeManager) {
                qCDebug(LogSystem) << "Ignoring application" << a->id() << ", because it uses an unknown runtime:" << a->runtimeName();
                continue;
            }
            if (runtimeManager->supportsQuickLaunch()) {
                if (a->supportsApplicationInterface())
                    qCDebug(LogSystem) << "Ignoring supportsApplicationInterface for application" << a->id() <<
                                          "as the runtime launcher supports it by default";
                a->setSupportsApplicationInterface(true);
            }
            if (a->id() != appDirName) {
                throw Exception(Error::Parse, "an info.yaml for built-in applications must be in directory "
                                              "that has the same name as the application's id: found %1 in %2")
                    .arg(a->id(), appDirName);
            }
            if (scanningBuiltinApps) {
                a->setBuiltIn(true);
                QStringList aliasPaths = appDir.entryList(QStringList(qSL("info-*.yaml")));
                std::vector<std::unique_ptr<Application>> aliases;

                for (int i = 0; i < aliasPaths.size(); ++i) {
                    std::unique_ptr<Application> alias(yas.scanAlias(appDir.absoluteFilePath(aliasPaths.at(i)), a.data()));

                    Q_ASSERT(alias);
                    Q_ASSERT(alias->isAlias());
                    Q_ASSERT(alias->nonAliased() == a.data());

                    aliases.push_back(std::move(alias));
                }
                result << a.take();
                for (auto &&alias : aliases)
                    result << alias.release();
            } else { // 3rd-party apps
                QFile f(appDir.absoluteFilePath(qSL("installation-report.yaml")));
                if (!f.open(QFile::ReadOnly))
                    continue;

                QScopedPointer<InstallationReport> report(new InstallationReport(a->id()));
                if (!report->deserialize(&f))
                    continue;

#if !defined(AM_DISABLE_INSTALLER)
                // fix the basedir of the application
                //TODO: we need to come up with a different way of handling baseDir
                foreach (const InstallationLocation &il, installationLocations) {
                    if (il.id() == report->installationLocationId()) {
                        a->setBaseDir(il.installationPath() + a->id());
                        break;
                    }
                }
#endif
                a->setInstallationReport(report.take());
                result << a.take();
            }
        }
    };

    foreach (const QString &dir, builtinAppsDirs)
        scan(dir, true);
#if !defined(AM_DISABLE_INSTALLER)
    scan(installedAppsDir, false);
#endif
    return result;
}

struct SystemProperties
{
    enum {
        ThirdParty,
        BuiltIn,
        SystemUi
    };

    static QVector<QVariantMap> partition(const QVariantMap &rawMap)
    {
        QVector<QVariantMap> props(SystemUi + 1);

        props[ThirdParty] = rawMap.value(qSL("public")).toMap();

        props[BuiltIn] = props.at(ThirdParty);
        const QVariantMap pro = rawMap.value(qSL("protected")).toMap();
        for (auto it = pro.cbegin(); it != pro.cend(); ++it)
            props[BuiltIn].insert(it.key(), it.value());

        props[SystemUi] = props.at(BuiltIn);
        const QVariantMap pri = rawMap.value(qSL("private")).toMap();
        for (auto it = pri.cbegin(); it != pri.cend(); ++it)
            props[SystemUi].insert(it.key(), it.value());

        return props;
    }
};

QT_END_NAMESPACE_AM

QT_USE_NAMESPACE_AM

int main(int argc, char *argv[])
{
    StartupTimer startupTimer;

    QCoreApplication::setApplicationName(qSL("ApplicationManager"));
    QCoreApplication::setOrganizationName(qSL("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(qSL("pelagicore.com"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));
    for (int i = 1; i < argc; ++i) {
        if (strcmp("--no-dlt-logging", argv[i]) == 0) {
            dltLoggingEnabled = false;
            break;
        }
    }
    installMessageHandlers();

    QString error;

    startupTimer.checkpoint("after basic initialization");

#if !defined(AM_DISABLE_INSTALLER)
    ensureCorrectLocale();

    if (Q_UNLIKELY(!forkSudoServer(DropPrivilegesPermanently, &error))) {
        qCCritical(LogSystem) << "ERROR:" << qPrintable(error);
        return 2;
    }
#endif

    startupTimer.checkpoint("after sudo server fork");

#if defined(AM_HEADLESS)
    QCoreApplication a(argc, argv);
#else
#  if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    // this is needed for both WebEngine and Wayland Multi-screen rendering
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#  endif
#  if !defined(QT_NO_SESSIONMANAGER)
    QGuiApplication::setFallbackSessionManagementEnabled(false);
#endif
    QGuiApplication a(argc, argv);

    UnixSignalHandler::instance()->install(UnixSignalHandler::ForwardedToEventLoopHandler, SIGINT,
                                           [](int /*sig*/) {
        UnixSignalHandler::instance()->resetToDefault(SIGINT);

        qCritical(" > RECEIVED CTRL+C ... EXITING\n");

        auto windows = qApp->allWindows();
        for (QWindow *w : windows)
            w->metaObject()->invokeMethod(w, "close");
    });
#endif

    startupTimer.checkpoint("after application constructor");

    Configuration config_;
    configuration = &config_;

    startupTimer.checkpoint("after command line parse");

    setCrashActionConfiguration(configuration->managerCrashAction());

#if !defined(QT_NO_QML_DEBUGGER)
    QQmlDebuggingEnabler *debuggingEnabler = nullptr;
    if (configuration->qmlDebugging())
        debuggingEnabler = new QQmlDebuggingEnabler(true);
#else
    if (configuration->qmlDebugging())
        qCWarning(LogSystem) << "The --qml-debug option is ignored, because Qt was built without support for QML Debugging!";
#endif

    try {
        QStringList loggingRules = configuration->loggingRules();
        bool verbose = configuration->verbose();

        if (loggingRules.isEmpty() && !verbose)
            loggingRules.append(qSL("*.debug=false"));

        if (verbose) {
            // we are prepending here to allow the user to override these in the config file.
            loggingRules.prepend(qSL("qt.qpa.*.debug=false"));
            loggingRules.prepend(qSL("qt.quick.*.debug=false"));
            loggingRules.prepend(qSL("qt.scenegraph.*.debug=false"));
            loggingRules.prepend(qSL("*.debug=true"));
        }

        QLoggingCategory::setFilterRules(loggingRules.join(qL1C('\n')));

        // setting this for child processes //TODO: use a more generic IPC approach
        qputenv("AM_LOGGING_RULES", loggingRules.join(qL1C('\n')).toUtf8());

        registerUnregisteredDLTContexts();

        startupTimer.checkpoint("after logging setup");

#ifdef AM_TESTRUNNER
        TestRunner::initialize(argv[0], configuration->positionalArguments());
#endif

        auto startupPlugins = loadPlugins<StartupInterface>("startup", configuration->pluginFilePaths("startup"));

        const QVector<QVariantMap> sysProps = SystemProperties::partition(configuration->rawSystemProperties());
        foreach (StartupInterface *iface, startupPlugins)
            iface->initialize(sysProps.at(SystemProperties::SystemUi));

        startupTimer.checkpoint("after startup-plugin load");

#if defined(QT_DBUS_LIB)
        if (Q_UNLIKELY(configuration->dbusStartSessionBus())) {
            class DBusDaemonProcess : public QProcess
            {
            public:
                DBusDaemonProcess(QObject *parent = 0)
                    : QProcess(parent)
                {
                    setProgram(qSL("dbus-daemon"));
                    setArguments({ qSL("--nofork"), qSL("--print-address"), qSL("--session")});
                }
                ~DBusDaemonProcess() override
                {
                    kill();
                    waitForFinished();
                }

            protected:
                void setupChildProcess() override
                {
#  if defined(Q_OS_LINUX)
                    // at least on Linux we can make sure that those dbus-daemons are always killed
                    prctl(PR_SET_PDEATHSIG, SIGKILL);
#  endif
                    QProcess::setupChildProcess();
                }
            };

            auto dbusDaemon = new DBusDaemonProcess(&a);
            dbusDaemon->start(QIODevice::ReadOnly);
            if (!dbusDaemon->waitForStarted() || !dbusDaemon->waitForReadyRead())
                throw Exception("could not start a dbus-launch process: %1").arg(dbusDaemon->errorString());

            QByteArray busAddress = dbusDaemon->readAllStandardOutput().trimmed();
            qputenv("DBUS_SESSION_BUS_ADDRESS", busAddress);
            qCInfo(LogSystem) << "NOTICE: running on private D-Bus session bus to avoid conflicts:";
            qCInfo(LogSystem, "        DBUS_SESSION_BUS_ADDRESS=%s", busAddress.constData());

            startupTimer.checkpoint("after starting session D-Bus");
        }
#endif
        if (Q_UNLIKELY(!QFile::exists(configuration->mainQmlFile())))
            throw Exception("no/invalid main QML file specified: %1").arg(configuration->mainQmlFile());

#if !defined(AM_DISABLE_INSTALLER)
        if (!checkCorrectLocale()) {
            // we should really throw here, but so many embedded systems are badly set up
            qCCritical(LogSystem) << "WARNING: the appman installer needs a UTF-8 locale to work correctly:\n"
                                     "         even automatically switching to C.UTF-8 or en_US.UTF-8 failed.";
        }

        if (Q_UNLIKELY(hardwareId().isEmpty()))
            throw Exception("the installer is enabled, but the device-id is empty");

        QVector<InstallationLocation> installationLocations = InstallationLocation::parseInstallationLocations(configuration->installationLocations());

        if (Q_UNLIKELY(!QDir::root().mkpath(configuration->installedAppsManifestDir())))
            throw Exception("could not create manifest directory %1").arg(configuration->installedAppsManifestDir());

        if (Q_UNLIKELY(!QDir::root().mkpath(configuration->appImageMountDir())))
            throw Exception("could not create the image-mount directory %1").arg(configuration->appImageMountDir());

        startupTimer.checkpoint("after installer setup checks");
#endif

        bool forceSingleProcess = configuration->forceSingleProcess();
        bool forceMultiProcess = configuration->forceMultiProcess();

        if (forceMultiProcess && forceSingleProcess)
            throw Exception("You cannot enforce multi- and single-process mode at the same time.");

#if !defined(AM_MULTI_PROCESS)
        if (forceMultiProcess)
            throw Exception("This application manager build is not multi-process capable.");
        forceSingleProcess = true;
#endif

        qApp->setProperty("singleProcessMode", forceSingleProcess);

        if (forceSingleProcess) {
            RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager());
            RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager(qSL("qml")));
        } else {
            RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager());
#if defined(AM_NATIVE_RUNTIME_AVAILABLE)
            RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager());
            RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager(qSL("qml")));
            //RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager(qSL("html")));
#endif
#if defined(AM_HOST_CONTAINER_AVAILABLE)
            ContainerFactory::instance()->registerContainer(new ProcessContainerManager());
#endif
            auto containerPlugins = loadPlugins<ContainerManagerInterface>("container", configuration->pluginFilePaths("container"));
            foreach (ContainerManagerInterface *iface, containerPlugins)
                ContainerFactory::instance()->registerContainer(new PluginContainerManager(iface));
        }
        foreach (StartupInterface *iface, startupPlugins)
            iface->afterRuntimeRegistration();

        ContainerFactory::instance()->setConfiguration(configuration->containerConfigurations());
        RuntimeFactory::instance()->setConfiguration(configuration->runtimeConfigurations());

        RuntimeFactory::instance()->setSystemProperties(sysProps.at(SystemProperties::ThirdParty),
                                                        sysProps.at(SystemProperties::BuiltIn));

        startupTimer.checkpoint("after runtime registration");

        QScopedPointer<ApplicationDatabase> adb(configuration->singleApp().isEmpty()
                                                ? new ApplicationDatabase(configuration->database())
                                                : new ApplicationDatabase());

        if (Q_UNLIKELY(!adb->isValid() && !configuration->recreateDatabase()))
            throw Exception("database file %1 is not a valid application database: %2").arg(adb->name(), adb->errorString());

        if (!adb->isValid() || configuration->recreateDatabase()) {
            QVector<const Application *> apps;

            if (!configuration->singleApp().isEmpty()) {
                apps = scanForApplication(configuration->singleApp(), configuration->builtinAppsManifestDirs());
            } else {
                apps = scanForApplications(configuration->builtinAppsManifestDirs()
#if !defined(AM_DISABLE_INSTALLER)
                                           , configuration->installedAppsManifestDir(), installationLocations
#endif
                                           );
            }

            qCDebug(LogSystem) << "Registering applications:";
            foreach (const Application *app, apps)
                qCDebug(LogSystem).nospace().noquote() << " * " << app->id() << " [at: " << app->baseDir().path() << "]";

            adb->write(apps);
        }

        startupTimer.checkpoint("after application database loading");

        ApplicationManager *am = ApplicationManager::createInstance(adb.take(), forceSingleProcess, &error);
        if (Q_UNLIKELY(!am))
            throw Exception(Error::System, error);
        if (configuration->noSecurity())
            am->setSecurityChecksEnabled(false);

        am->setSystemProperties(sysProps.at(SystemProperties::SystemUi));
        am->setContainerSelectionConfiguration(configuration->containerSelectionConfiguration());

        startupTimer.checkpoint("after ApplicationManager instantiation");

        NotificationManager *nm = NotificationManager::createInstance();

        startupTimer.checkpoint("after NotificationManager instantiation");

        SystemMonitor *sysmon = SystemMonitor::createInstance();

        startupTimer.checkpoint("after SystemMonitor instantiation");

        QuickLauncher *ql = QuickLauncher::instance();
        ql->initialize(configuration->quickLaunchRuntimesPerContainer(), configuration->quickLaunchIdleLoad());

        startupTimer.checkpoint("after quick-launcher setup");

#if !defined(AM_DISABLE_INSTALLER)
        ApplicationInstaller *ai = ApplicationInstaller::createInstance(installationLocations,
                                                                        configuration->installedAppsManifestDir(),
                                                                        configuration->appImageMountDir(),
                                                                        &error);
        if (Q_UNLIKELY(!ai))
            throw Exception(Error::System, error);
        if (configuration->noSecurity()) {
            ai->setDevelopmentMode(true);
            ai->setAllowInstallationOfUnsignedPackages(true);
        } else {
            QList<QByteArray> caCertificateList;

            const auto caFiles = configuration->caCertificates();
            for (const auto &caFile : caFiles) {
                QFile f(caFile);
                if (Q_UNLIKELY(!f.open(QFile::ReadOnly)))
                    throw Exception(f, "could not open CA-certificate file");
                QByteArray cert = f.readAll();
                if (Q_UNLIKELY(cert.isEmpty()))
                    throw Exception(f, "CA-certificate file is empty");
                caCertificateList << cert;
            }
            ai->setCACertificates(caCertificateList);
        }

        uint minUserId, maxUserId, commonGroupId;
        if (configuration->applicationUserIdSeparation(&minUserId, &maxUserId, &commonGroupId)) {
#  if defined(Q_OS_LINUX)
            if (!ai->enableApplicationUserIdSeparation(minUserId, maxUserId, commonGroupId))
                throw Exception("could not enable application user-id separation in the installer.");
#  else
            qCCritical(LogSystem) << "WARNING: application user-id separation requested, but not possible on this platform.";
#  endif // Q_OS_LINUX
        }

        //TODO: this could be delayed, but needs to have a lock on the app-db in this case
        ai->cleanupBrokenInstallations();

        startupTimer.checkpoint("after ApplicationInstaller instantiation");

#endif // AM_DISABLE_INSTALLER

        qmlRegisterType<QmlInProcessNotification>("QtApplicationManager", 1, 0, "Notification");
        qmlRegisterType<QmlInProcessApplicationInterfaceExtension>("QtApplicationManager", 1, 0, "ApplicationInterfaceExtension");

#if !defined(AM_HEADLESS)
        qmlRegisterType<FakeApplicationManagerWindow>("QtApplicationManager", 1, 0, "ApplicationManagerWindow");
#endif
        startupTimer.checkpoint("after QML registrations");

        ApplicationIPCManager *aipcm = ApplicationIPCManager::createInstance();

        QQmlApplicationEngine *engine = new QQmlApplicationEngine(&a);
        new QmlLogger(engine);
        engine->setOutputWarningsToStandardError(false);
        engine->setImportPathList(engine->importPathList() + configuration->importPaths());
        engine->rootContext()->setContextProperty("StartupTimer", &startupTimer);

        startupTimer.checkpoint("after QML engine instantiation");

#ifdef AM_TESTRUNNER
        QFile f(qSL(":/build-config.yaml"));
        QVector<QVariant> docs;
        if (f.open(QFile::ReadOnly))
            docs = QtYaml::variantDocumentsFromYaml(f.readAll());
        f.close();

        engine->rootContext()->setContextProperty("buildConfig", docs.toList());
#endif

#if !defined(AM_HEADLESS)

        // For development only: set an icon, so you know which window is the AM
        bool setIcon =
#  if defined(Q_OS_LINUX)
                (a.platformName() == qL1S("xcb"));
#  else
                true;
#  endif
        if (Q_UNLIKELY(setIcon)) {
            QString icon = configuration->windowIcon();
            if (!icon.isEmpty())
                QGuiApplication::setWindowIcon(QIcon(icon));
        }

        QUnifiedTimer::instance()->setSlowModeEnabled(configuration->slowAnimations());

        WindowManager *wm = WindowManager::createInstance(engine, configuration->waylandSocketName());
        wm->enableWatchdog(!configuration->noUiWatchdog());

        QObject::connect(am, &ApplicationManager::inProcessRuntimeCreated,
                         wm, &WindowManager::setupInProcessRuntime);
        QObject::connect(am, &ApplicationManager::applicationWasActivated,
                         wm, &WindowManager::raiseApplicationWindow);
#endif

        if (Q_UNLIKELY(configuration->loadDummyData())) {
            loadDummyDataFiles(*engine, QFileInfo(configuration->mainQmlFile()).path());
            startupTimer.checkpoint("after loading dummy-data");
        }

#if defined(QT_DBUS_LIB)
        // can we delay the D-Bus initialization? it is asynchronous anyway...
        int dbusDelay = configuration->dbusRegistrationDelay();
        if (dbusDelay < 0) {
            dbusInitialization();
            startupTimer.checkpoint("after D-Bus registrations");
        } else {
            QTimer::singleShot(dbusDelay, qApp, dbusInitialization);
        }
#endif
        foreach (StartupInterface *iface, startupPlugins)
            iface->beforeQmlEngineLoad(engine);

        engine->load(configuration->mainQmlFile());
        if (Q_UNLIKELY(engine->rootObjects().isEmpty()))
            throw Exception("Qml scene does not have a root object");
        QObject *rootObject = engine->rootObjects().at(0);

        foreach (StartupInterface *iface, startupPlugins)
            iface->afterQmlEngineLoad(engine);

        startupTimer.checkpoint("after loading main QML file");

#if !defined(AM_HEADLESS)
        QQuickWindow *window = nullptr;

        if (!rootObject->isWindowType()) {
            QQuickView *view = new QQuickView(engine, 0);
            startupTimer.checkpoint("after WindowManager/QuickView instantiation");
            view->setContent(configuration->mainQmlFile(), 0, rootObject);
            window = view;
        } else {
            window = qobject_cast<QQuickWindow *>(rootObject);
            if (!engine->incubationController())
                engine->setIncubationController(window->incubationController());
        }

        Q_ASSERT(window);
        QMetaObject::Connection conn = QObject::connect(window, &QQuickWindow::frameSwapped, qApp, [&startupTimer, &conn]() { // clazy:exclude=lambda-in-connect
            // startupTimer and conn are local vars captured by reference: this is a bad thing in
            // general, but in this case it is perfectly fine, since both HAVE to be alive, when
            // the frameSwapped signal is emitted.
            static bool once = true;
            if (once) {
                QObject::disconnect(conn);
                once = false;
                startupTimer.checkpoint("after first frame drawn");
                startupTimer.createReport(qSL("System UI"));
            }
        });

        wm->registerCompositorView(window);

        foreach (StartupInterface *iface, startupPlugins)
            iface->beforeWindowShow(window);

        // --no-fullscreen on the command line trumps the fullscreen setting in the config file
        if (Q_LIKELY(configuration->fullscreen() && !configuration->noFullscreen()))
            window->showFullScreen();
        else
            window->show();

        foreach (StartupInterface *iface, startupPlugins)
            iface->afterWindowShow(window);

        startupTimer.checkpoint("after window show");
#endif

        // delay debug-wrapper setup
        QTimer::singleShot(1500, qApp, []() {
            ApplicationManager::instance()->setDebugWrapperConfiguration(configuration->debugWrappers());
        });

#if defined(QT_PSHELLSERVER_LIB)
        // have a JavaScript shell reachable via telnet protocol
        PTelnetServer telnetServer;
        AMShellFactory shellFactory(engine, rootObject);
        telnetServer.setShellFactory(&shellFactory);

        if (!telnetServer.listen(QHostAddress(configuration->telnetAddress()), configuration->telnetPort())) {
            throw Exception("could not start Telnet server");
        } else {
            qCDebug(LogSystem) << "Telnet server listening on \n " << telnetServer.serverAddress().toString()
                               << "port" << telnetServer.serverPort();
        }

        // register all objects that should be reachable from the telnet shell
        engine->rootContext()->setContextProperty("_ApplicationManager", am);
        engine->rootContext()->setContextProperty("_WindowManager", wm);
#endif // QT_PSHELLSERVER_LIB

#if defined(QT_PSSDP_LIB)
        // announce ourselves via SSDP (the discovery protocol of UPnP)

        QUuid uuid = QUuid::createUuid(); // should really be QUuid::Time version...
        PSsdpService ssdp;

        bool ssdpOk = ssdp.initialize();
        if (!ssdpOk) {
            throw Exception(LogSystem, "could not initialze SSDP service");
        } else {
            ssdp.setDevice(uuid, QLatin1String("application-manager"), 1, QLatin1String("pelagicore.com"));

#  if defined(QT_PSHELLSERVER_LIB)
            QMap<QString, QString> extraTelnet;
            extraTelnet.insert("LOCATION", QString::fromLatin1("${HOST}:%1").arg(telnetServer.serverPort()));
            ssdp.addService(QLatin1String("jsshell"), 1, QLatin1String("pelagicore.com"), extraTelnet);
#  endif // QT_PSHELLSERVER_LIB

            ssdp.setActive(true);
        }
        engine->rootContext()->setContextProperty("ssdp", &ssdp);
#endif // QT_PSSDP_LIB

        QObject::connect(qApp, &QCoreApplication::aboutToQuit,
                         ApplicationManager::instance(), &ApplicationManager::killAll);
#ifdef AM_TESTRUNNER
        Q_UNUSED(nm);
        Q_UNUSED(sysmon);
        Q_UNUSED(aipcm);
#if !defined(QT_NO_QML_DEBUGGER)
        Q_UNUSED(debuggingEnabler);
#endif

        int res =  TestRunner::exec(engine);
        delete engine;
#else
        int res = a.exec();

        // the eventloop stopped, so any pending "retakes" would not be executed
        retakeSingletonOwnershipFromQmlEngine(engine, am, true);
        delete engine;

        delete nm;
#if !defined(AM_HEADLESS)
        delete wm;
#endif
        delete am;
        delete ql;
        delete sysmon;
        delete aipcm;
#if !defined(QT_NO_QML_DEBUGGER)
        delete debuggingEnabler;
#endif
#endif

#if defined(QT_PSSDP_LIB)
        if (ssdpOk)
            ssdp.setActive(false);
#endif // QT_PSSDP_LIB

        return res;

    } catch (const std::exception &e) {
        qCCritical(LogSystem) << "ERROR:" << e.what();
        return 2;
    }
}
