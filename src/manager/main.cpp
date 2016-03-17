/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <memory>
#include <qglobal.h>

// include as eary as possible, since the <windows.h> header will re-#define "interface"
#if defined(QT_DBUS_LIB)
#  include <QDBusConnection>
#endif

#include <QFile>
#include <QDir>
#include <QStringList>
#include <QVariant>
#include <QFileInfo>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QPluginLoader>
#include <QUrl>
#include <QLibrary>
#include <QFunctionPointer>
#include <QProcess>
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
#include "notificationmanager.h"
#include "qmlinprocessruntime.h"
#include "qmlinprocessapplicationinterface.h"

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

#if defined(MALIIT_INTEGRATION)
#include "minputcontextconnection.h"
#include "abstractplatform.h"
#include "mimserver.h"
#include "connectionfactory.h"
#include "unknownplatform.h"
#endif


static Configuration *configuration = 0;


#if defined(QT_DBUS_LIB)

static QString dbusInterfaceName(QObject *o) throw (Exception)
{
    int idx = o->metaObject()->indexOfClassInfo("D-Bus Interface");
    if (idx < 0) {
        throw Exception(Error::System, "Could not get class-info \"D-Bus Interface\" for D-Bus adapter %1")
            .arg(o->metaObject()->className());
    }
    return QLatin1String(o->metaObject()->classInfo(idx).value());
}

static void registerDBusObject(QDBusAbstractAdaptor *adaptor, const char *serviceName, const char *path) throw (Exception)
{
    QString interfaceName = dbusInterfaceName(adaptor);
    QString dbus = configuration->dbusRegistration(interfaceName);
    QDBusConnection conn((QString()));

    if (dbus.isEmpty()) {
        return;
    } else if (dbus == qL1S("system")) {
        conn = QDBusConnection::systemBus();
    } else if (dbus == qL1S("session")) {
        dbus = qgetenv("DBUS_SESSION_BUS_ADDRESS");
        conn = QDBusConnection::sessionBus();
    } else {
        conn = QDBusConnection::connectToBus(dbus, qSL("custom"));
    }

    if (!conn.isConnected()) {
        throw Exception(Error::System, "could not connect to D-Bus (%1): %2")
                .arg(dbus).arg(conn.lastError().message());
    }

    if (!conn.registerObject(qL1S(path), adaptor->parent(), QDBusConnection::ExportAdaptors)) {
        throw Exception(Error::System, "could not register object %1 on D-Bus (%2): %3")
                .arg(path).arg(dbus).arg(conn.lastError().message());
    }

    if (!conn.registerService(qL1S(serviceName))) {
        throw Exception(Error::System, "could not register service %1 on D-Bus (%2): %3")
                .arg(serviceName).arg(dbus).arg(conn.lastError().message());
    }

    if (interfaceName.startsWith(qL1S("io.qt."))) {
        // Write the bus address of the interface to a file in /tmp. This is needed for the
        // controller tool, which does not even have a session bus, when started via ssh.

        QFile f(QDir::temp().absoluteFilePath(interfaceName + qSL(".dbus")));
        QByteArray dbusUtf8 = dbus.toUtf8();
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
        qDBusRegisterMetaType<QUrl>();
    }
}

QT_BEGIN_NAMESPACE

QDBusArgument &operator<<(QDBusArgument &argument, const QUrl &url)
{
    argument.beginStructure();
    argument << QString::fromLatin1(url.toEncoded());
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QUrl &url)
{
    QString s;
    argument.beginStructure();
    argument >> s;
    argument.endStructure();
    url = QUrl::fromEncoded(s.toLatin1());
    return argument;
}

QT_END_NAMESPACE

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

static QVector<const Application *> scanForApplications(const QDir &builtinAppsDir, const QDir &installedAppsDir,
                                                      const QVector<InstallationLocation> &installationLocations)
{
    QVector<const Application *> result;
    YamlApplicationScanner yas;

    auto scan = [&result, &yas, &installationLocations](const QDir &baseDir, bool scanningBuiltinApps) {
        foreach (const QString &appDirName, baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
            if (appDirName.endsWith('+') || appDirName.endsWith('-'))
                continue;
            if (!isValidDnsName(appDirName)) {
                qCDebug(LogSystem) << "Ignoring Application Directory" << appDirName << ", as it's not following the rdns convention";
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

            if (a->id() != appDirName) {
                throw Exception(Error::Parse, "an info.yaml for built-in applications must be in directory "
                                              "that has the same name as the application's id: found %1 in %2")
                    .arg(a->id(), appDirName);
            }
            if (scanningBuiltinApps) {
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

    scan(builtinAppsDir, true);
    scan(installedAppsDir, false);
    return result;
}

int main(int argc, char *argv[])
{
    StartupTimer startupTimer;

    QCoreApplication::setApplicationName(qSL("ApplicationManager"));
    QCoreApplication::setOrganizationName(qSL("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(qSL("pelagicore.com"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    qInstallMessageHandler(colorLogToStderr);
    QString error;

    startupTimer.checkpoint("after basic initialization");

#if !defined(AM_DISABLE_INSTALLER)
    {
        if (!forkSudoServer(DropPrivilegesPermanently, &error)) {
            qCCritical(LogSystem) << "WARNING:" << qPrintable(error);

            // do not quit, but rather contine with reduced functionality
            SudoClient::initialize(-1);
        }
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

    QGuiApplication a(argc, argv);
#endif

    startupTimer.checkpoint("after application constructor");

    Configuration config_;
    configuration = &config_;

    startupTimer.checkpoint("after command line parse");

    try {
        if (!QFile::exists(configuration->mainQmlFile()))
            throw Exception(Error::System, "no/invalid main QML file specified: %1").arg(configuration->mainQmlFile());

        QStringList loggingRules = configuration->loggingRules();
        if (loggingRules.isEmpty())
            loggingRules << qSL("*.warning=true");
        if (configuration->verbose()) {
            loggingRules << qSL("*.debug=true");
            loggingRules << qSL("qt.scenegraph.*.debug=false");
            loggingRules << qSL("qt.quick.*.debug=false");
            loggingRules << qSL("qt.qpa.*.debug=false");
        }

        QLoggingCategory::setFilterRules(loggingRules.join(qL1C('\n')));

        // setting this for child processes //TODO: use a more generic IPC approach
        qputenv("AM_LOGGING_RULES", loggingRules.join(qL1C('\n')).toUtf8());

        startupTimer.checkpoint("after logging setup");

#if !defined(AM_DISABLE_INSTALLER)
        if (hardwareId().isEmpty())
            throw Exception(Error::System, "the installer is enabled, but the device-id is empty");

        QVector<InstallationLocation> installationLocations = InstallationLocation::parseInstallationLocations(configuration->installationLocations());

        if (!QDir::root().mkpath(configuration->installedAppsManifestDir()))
            throw Exception(Error::System, "could not create manifest directory %1").arg(configuration->installedAppsManifestDir());

        if (!QDir::root().mkpath(configuration->appImageMountDir()))
            throw Exception(Error::System, "could not create the image-mount directory %1").arg(configuration->appImageMountDir());

        startupTimer.checkpoint("after installer setup checks");
#endif

        bool forceSingleProcess = true;
#if !defined(AM_SINGLE_PROCESS_MODE)
        forceSingleProcess = configuration->forceSingleProcess();
#endif

        if (forceSingleProcess) {
            RuntimeFactory::instance()->registerRuntime<QmlInProcessRuntimeManager>();
            RuntimeFactory::instance()->registerRuntime<QmlInProcessRuntimeManager>(qSL("qml"));
        } else {
            RuntimeFactory::instance()->registerRuntime<QmlInProcessRuntimeManager>();
#if defined(AM_NATIVE_RUNTIME_AVAILABLE)
            RuntimeFactory::instance()->registerRuntime<NativeRuntimeManager>();
            RuntimeFactory::instance()->registerRuntime<NativeRuntimeManager>(qSL("qml"));
            //RuntimeFactory::instance()->registerRuntime<NativeRuntimeManager>(qSL("html"));
#endif
#if defined(AM_HOST_CONTAINER_AVAILABLE)
            ContainerFactory::instance()->registerContainer<ProcessContainerManager>();
#endif
        }
        ContainerFactory::instance()->setConfiguration(configuration->containerConfigurations());
        RuntimeFactory::instance()->setConfiguration(configuration->runtimeConfigurations());
        RuntimeFactory::instance()->setAdditionalConfiguration(configuration->additionalUiConfiguration());

        startupTimer.checkpoint("after runtime registration");

        QuickLauncher *ql = QuickLauncher::instance();
        ql->initialize(configuration->quickLaunchRuntimesPerContainer(), configuration->quickLaunchIdleLoad());

        startupTimer.checkpoint("after quick-launcher setup");

        QScopedPointer<ApplicationDatabase> adb(configuration->singleApp().isEmpty()
                                                ? new ApplicationDatabase(configuration->database())
                                                : new ApplicationDatabase());

        if (!adb->isValid() && !configuration->recreateDatabase())
            throw Exception(Error::System, "database file %1 is not a valid application database: %2").arg(adb->name(), adb->errorString());

        if (!adb->isValid() || configuration->recreateDatabase()) {
            QVector<const Application *> apps;
            YamlApplicationScanner yas;

            if (!configuration->singleApp().isEmpty()) {
                if (const Application *app = yas.scan(configuration->singleApp()))
                    apps.append(app);
            } else {
                apps = scanForApplications(configuration->builtinAppsManifestDir(),
                                           configuration->installedAppsManifestDir(),
                                           installationLocations);
            }

            qCDebug(LogSystem) << "Found Applications: [";
            foreach (const Application *app, apps)
                qCDebug(LogSystem) << " * APP:" << app->id() << "(" << app->baseDir().absolutePath() << ")";
            qCDebug(LogSystem) << "]";

            adb->write(apps);
        }

        startupTimer.checkpoint("after application database loading");

        ApplicationManager *am = ApplicationManager::createInstance(adb.take(), &error);
        if (!am)
            throw Exception(Error::System, error);
        if (configuration->noSecurity())
            am->setSecurityChecksEnabled(false);
        am->setAdditionalConfiguration(configuration->additionalUiConfiguration());

        startupTimer.checkpoint("after ApplicationManager instantiation");

        NotificationManager *nm = NotificationManager::createInstance();

        startupTimer.checkpoint("after NotificationManager instantiation");

        SystemMonitor *sysmon = SystemMonitor::createInstance();

        startupTimer.checkpoint("after SystemMonitor instantiation");

#if !defined(AM_DISABLE_INSTALLER)
        ApplicationInstaller *ai = ApplicationInstaller::createInstance(installationLocations,
                                                                        configuration->installedAppsManifestDir(),
                                                                        configuration->appImageMountDir(),
                                                                        &error);
        if (!ai)
            throw Exception(Error::System, error);
        if (configuration->noSecurity()) {
            ai->setDevelopmentMode(true);
            ai->setAllowInstallationOfUnsignedPackages(true);
        }

        uint minUserId, maxUserId, commonGroupId;
        if (configuration->applicationUserIdSeparation(&minUserId, &maxUserId, &commonGroupId)) {
#  if defined(Q_OS_LINUX)
            if (!ai->enableApplicationUserIdSeparation(minUserId, maxUserId, commonGroupId))
                throw Exception(Error::System, "could not enable application user-id separation in the installer.");
#  else
            qCCritical(LogSystem) << "WARNING: application user-id separation requested, but not possible on this platform.";
#  endif // Q_OS_LINUX
        }

        ai->cleanupBrokenInstallations();

        startupTimer.checkpoint("after ApplicationInstaller instantiation");

        qmlRegisterSingletonType<ApplicationInstaller>("io.qt.ApplicationInstaller", 1, 0, "ApplicationInstaller",
                                                       &ApplicationInstaller::instanceForQml);
#endif // AM_DISABLE_INSTALLER

        qmlRegisterSingletonType<ApplicationManager>("io.qt.ApplicationManager", 1, 0, "ApplicationManager",
                                                     &ApplicationManager::instanceForQml);
        qmlRegisterSingletonType<SystemMonitor>("io.qt.ApplicationManager", 1, 0, "SystemMonitor",
                                                     &SystemMonitor::instanceForQml);
        qmlRegisterSingletonType<NotificationManager>("io.qt.ApplicationManager", 1, 0, "NotificationManager",
                                                     &NotificationManager::instanceForQml);
        qmlRegisterType<QmlInProcessNotification>("io.qt.ApplicationManager", 1, 0, "Notification");
        qmlRegisterType<QmlInProcessApplicationInterfaceExtension>("io.qt.ApplicationManager", 1, 0, "ApplicationInterfaceExtension");

#if !defined(AM_HEADLESS)
        qmlRegisterSingletonType<WindowManager>("io.qt.ApplicationManager", 1, 0, "WindowManager",
                                                &WindowManager::instanceForQml);
        qmlRegisterType<FakeApplicationManagerWindow>("io.qt.ApplicationManager", 1, 0, "ApplicationManagerWindow");
#endif

        startupTimer.checkpoint("after QML registrations");

        QQmlApplicationEngine *engine = new QQmlApplicationEngine(&a);
        new QmlLogger(engine);
        engine->setOutputWarningsToStandardError(false);
        engine->setImportPathList(engine->importPathList() + configuration->importPaths());

        startupTimer.checkpoint("after QML engine instantiation");

#if !defined(AM_HEADLESS)

        // For development only: set an icon, so you know which window is the AM
        bool setIcon =
#  if defined(Q_OS_LINUX)
                (a.platformName() == "xcb");
#  else
                true;
#  endif
        if (setIcon) {
            QString icon = configuration->windowIcon();
            if (!icon.isEmpty())
                QGuiApplication::setWindowIcon(QIcon(icon));
        }

        QUnifiedTimer::instance()->setSlowModeEnabled(configuration->slowAnimations());

        WindowManager *wm = WindowManager::createInstance(engine, forceSingleProcess, configuration->waylandSocketName());
        wm->enableWatchdog(!configuration->noUiWatchdog());

        QObject::connect(am, &ApplicationManager::inProcessRuntimeCreated,
                         wm, &WindowManager::setupInProcessRuntime);
        QObject::connect(am, &ApplicationManager::applicationWasReactivated,
                         wm, &WindowManager::raiseApplicationWindow);
#endif

        if (configuration->loadDummyData()) {
            loadDummyDataFiles(*engine, QFileInfo(configuration->mainQmlFile()).path());
            startupTimer.checkpoint("after loading dummy-data");
        }

#if defined(QT_DBUS_LIB)
        registerDBusObject(new ApplicationManagerAdaptor(am), "io.qt.ApplicationManager", "/ApplicationManager");
        if (!am->setDBusPolicy(configuration->dbusPolicy(dbusInterfaceName(am))))
            throw Exception(Error::DBus, "could not set DBus policy for ApplicationManager");

#  if !defined(AM_DISABLE_INSTALLER)
        registerDBusObject(new ApplicationInstallerAdaptor(ai), "io.qt.ApplicationManager", "/ApplicationInstaller");
        if (!ai->setDBusPolicy(configuration->dbusPolicy(dbusInterfaceName(ai))))
            throw Exception(Error::DBus, "could not set DBus policy for ApplicationInstaller");
#  endif

#  if !defined(AM_HEADLESS)
        try {
            registerDBusObject(new NotificationsAdaptor(nm), "org.freedesktop.Notifications", "/org/freedesktop/Notifications");
        } catch (const Exception &e) {
            //TODO: what should we do here? on the desktop this will obviously always fail
            qCCritical(LogSystem) << "WARNING:" << e.what();
        }
        registerDBusObject(new WindowManagerAdaptor(wm), "io.qt.ApplicationManager", "/WindowManager");
        if (!wm->setDBusPolicy(configuration->dbusPolicy(dbusInterfaceName(wm))))
            throw Exception(Error::DBus, "could not set DBus policy for WindowManager");
#endif
        startupTimer.checkpoint("after D-Bus registrations");
#endif // QT_DBUS_LIB

        engine->load(configuration->mainQmlFile());
        if (engine->rootObjects().isEmpty())
            throw Exception(Error::System, "Qml scene does not have a root object");
        QObject *rootObject = engine->rootObjects().at(0);

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
        }

        Q_ASSERT(window);
        QMetaObject::Connection conn = QObject::connect(window, &QQuickWindow::frameSwapped, qApp, [&startupTimer, &conn]() {
            static bool once = true;
            if (once) {
                startupTimer.checkpoint("after first frame drawn");
                QObject::disconnect(conn);
                once = false;
            }
        });

        wm->registerOutputWindow(window);

        // --no-fullscreen on the command line trumps the fullscreen setting in the config file
        if (configuration->fullscreen() && !configuration->noFullscreen())
            window->showFullScreen();
        else
            window->show();

        startupTimer.checkpoint("after window show");
#endif

#if defined(MALIIT_INTEGRATION)
        // Input Context Connection
        QSharedPointer<MInputContextConnection> icConnection(Maliit::DBus::createInputContextConnectionWithDynamicAddress());

        QSharedPointer<Maliit::AbstractPlatform> platform(new Maliit::UnknownPlatform);

        // The actual server
        MImServer::configureSettings(MImServer::PersistentSettings);
        MImServer imServer(icConnection, platform);
#endif

#if defined(QT_PSHELLSERVER_LIB)
        // have a JavaScript shell reachable via telnet protocol
        PTelnetServer telnetServer;
        AMShellFactory shellFactory(engine, rootObject);
        telnetServer.setShellFactory(&shellFactory);

        if (!telnetServer.listen(QHostAddress(configuration->telnetAddress()), configuration->telnetPort())) {
            throw Exception(Error::System, "could not start Telnet server");
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

        startupTimer.createReport();
        int res = a.exec();

        // Normally we would delete view here (which would in turn delete am and wm). This will
        // however dead-lock the process in Qt 5.2.1 with the main thread and the scene-graph
        // thread both waiting for each other.

        delete nm;
#if !defined(AM_HEADLESS)
        delete wm;
#endif
        delete am;
        delete ql;
        delete sysmon;

        delete engine;


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
