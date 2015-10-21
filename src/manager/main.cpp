/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

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
#include "nativeruntime.h"
#include "runtimefactory.h"
#include "notificationmanager.h"
#include "qmlinprocessruntime.h"
#include "qmlinprocessapplicationinterface.h"

#if !defined(AM_HEADLESS)
#  include "windowmanager.h"
#  include "fakepelagicorewindow.h"
#endif

#include "configuration.h"
#include "utilities.h"
#include "qmllogger.h"
#include "startuptimer.h"

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

static void registerDBusObject(QDBusAbstractAdaptor *adaptor, const QString &serviceName, const QString &path) throw (Exception)
{
    QString interfaceName = dbusInterfaceName(adaptor);
    QString dbus = configuration->dbusRegistration(interfaceName);
    QDBusConnection conn("");

    if (dbus.isEmpty()) {
        return;
    } else if (dbus == "system") {
        conn = QDBusConnection::systemBus();
    } else if (dbus == "session") {
        dbus = qgetenv("DBUS_SESSION_BUS_ADDRESS");
        conn = QDBusConnection::sessionBus();
    } else {
        conn = QDBusConnection::connectToBus(dbus, "custom");
    }

    if (!conn.isConnected()) {
        throw Exception(Error::System, "could not connect to D-Bus (%1): %2")
                .arg(dbus).arg(conn.lastError().message());
    }

    if (!conn.registerObject(path, adaptor->parent(), QDBusConnection::ExportAdaptors)) {
        throw Exception(Error::System, "could not register object %1 on D-Bus (%2): %3")
                .arg(path).arg(dbus).arg(conn.lastError().message());
    }

    if (!conn.registerService(serviceName)) {
        throw Exception(Error::System, "could not register service %1 on D-Bus (%2): %3")
                .arg(serviceName).arg(dbus).arg(conn.lastError().message());
    }

    if (interfaceName.startsWith("com.pelagicore.")) {
        // Write the bus address of the interface to a file in /tmp. This is needed for the
        // controller tool, which does not even have a session bus, when started via ssh.

        QFile f(QDir::temp().absoluteFilePath(interfaceName + ".dbus"));
        QByteArray dbusUtf8 = dbus.toUtf8();
        if (!f.open(QFile::WriteOnly | QFile::Truncate) || (f.write(dbusUtf8) != dbusUtf8.size()))
            throw Exception(f, "Could not write D-Bus address of interface %1").arg(interfaceName);

        static QStringList filesToDelete;
        if (filesToDelete.isEmpty())
            atexit([]() { foreach (const QString &ftd, filesToDelete) QFile::remove(ftd); });
        filesToDelete << f.fileName();
    }
}

#endif // QT_DBUS_LIB


// copied straight from Qt 5.1.0 qmlscene/main.cpp for now - needs to be revised
static void loadDummyDataFiles(QQmlEngine &engine, const QString& directory)
{
    QDir dir(directory+"/dummydata", "*.qml");
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
    { }

    PAbstractShell *create(QObject *parent)
    {
        return new PDeclarativeShell(m_engine, m_object, parent);
    }

private:
    QQmlEngine *m_engine;
    QObject *m_object;
};

#endif // QT_PSHELLSERVER_LIB

static QList<const Application *> scanForApplications(const QDir &builtinAppsDir, const QDir &installedAppsDir,
                                                      const QList<InstallationLocation> &installationLocations)
{
    QList<const Application *> result;
    YamlApplicationScanner yas;

    auto scan = [&result, &yas, &installationLocations](const QDir &baseDir, bool checkInstallationReport) {
        foreach (const QString &appDirName, baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
            if (appDirName.endsWith('+') || appDirName.endsWith('-'))
                continue;
            if (!isValidDnsName(appDirName))
                continue;
            QDir appDir = baseDir.absoluteFilePath(appDirName);
            if (!appDir.exists())
                continue;
            if (!appDir.exists("info.yaml"))
                continue;
            if (checkInstallationReport && !appDir.exists("installation-report.yaml"))
                continue;

            QScopedPointer<Application> a(yas.scan(appDir.absoluteFilePath("info.yaml")));
            if (!a)
                continue;
            if (a->id() != appDirName)
                continue;
            if (checkInstallationReport) {
                QFile f(appDir.absoluteFilePath("installation-report.yaml"));
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
            }
            result << a.take();
        }
    };

    scan(builtinAppsDir, false);
    scan(installedAppsDir, true);
    return result;
}

int main(int argc, char *argv[])
{
    StartupTimer startupTimer;

    QCoreApplication::setApplicationName("ApplicationManager");
    QCoreApplication::setOrganizationName(QLatin1String("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(QLatin1String("pelagicore.com"));
    QCoreApplication::setApplicationVersion(AM_GIT_VERSION);

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

#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0) && QT_VERSION < QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

#if defined(AM_HEADLESS)
    QCoreApplication a(argc, argv);
#else
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
            loggingRules << QLatin1String("*.warning=true");
        if (configuration->verbose()) {
            loggingRules << QLatin1String("*.debug=true");
            loggingRules << QLatin1String("qt.scenegraph.*.debug=false");
            loggingRules << QLatin1String("qt.quick.*.debug=false");
            loggingRules << QLatin1String("qt.qpa.*.debug=false");
        }

        QLoggingCategory::setFilterRules(loggingRules.join(QLatin1Char('\n')));

        // setting this for child processes //TODO: use a more generic IPC approach
        qputenv("AM_LOGGING_RULES", loggingRules.join(QLatin1Char('\n')).toUtf8());

        startupTimer.checkpoint("after logging setup");

#if !defined(AM_DISABLE_INSTALLER)
        if (hardwareId().isEmpty())
            throw Exception(Error::System, "the installer is enabled, but the device-id is empty");

        QList<InstallationLocation> installationLocations = InstallationLocation::parseInstallationLocations(configuration->installationLocations());

        if (!QDir::root().mkpath(configuration->installedAppsManifestDir()))
            throw Exception(Error::System, "could not create manifest directory %1").arg(configuration->installedAppsManifestDir());

        if (!QDir::root().mkpath(configuration->appImageMountDir()))
            throw Exception(Error::System, "could not create the image-mount directory %1").arg(configuration->appImageMountDir());

        startupTimer.checkpoint("after installer setup checks");
#endif

        AbstractRuntime::setConfiguration(configuration->runtimeConfigurations());

        QDir pluginsDir = QDir(a.applicationDirPath() + QLatin1String("/../lib/appman/"));

        qCDebug(LogSystem) << "Scanning for plugins in" << pluginsDir.absolutePath();

        foreach (const QString &plugin, pluginsDir.entryList(QStringList() << "*.so" << "*.dylib" << "*.dll"))  {
            QString pluginPath = pluginsDir.absoluteFilePath(plugin);

            // hack for the time being - this should be solved by the plugin's root object
            // (which we do not have yet)
            if (plugin.contains("runtime")) // && !plugin.contains("inprocess"))
                continue;

            QPluginLoader pluginLoader(pluginPath);
            bool ok = pluginLoader.load();

            qCDebug(LogSystem) << (ok ? " OK " : "FAIL") << pluginPath << (ok ? QString() : pluginLoader.errorString());
        }
        startupTimer.checkpoint("after plugin load");

#if defined(AM_SINGLEPROCESS_MODE)
        if (true) {
#else
        if (configuration->forceSingleProcess()) {
#endif
            RuntimeFactory::instance()->registerRuntime<QmlInProcessRuntime>();
            RuntimeFactory::instance()->registerRuntime<QmlInProcessRuntime>("qml");
        } else {
            RuntimeFactory::instance()->registerRuntime<QmlInProcessRuntime>();
#if defined(AM_NATIVE_RUNTIME_AVAILABLE)
            RuntimeFactory::instance()->registerRuntime<NativeRuntime>();
            RuntimeFactory::instance()->registerRuntime<NativeRuntime>("qml");
            //RuntimeFactory::instance()->registerRuntime<NativeRuntime>("html");
#endif
        }
        startupTimer.checkpoint("after runtime registration");

        QScopedPointer<ApplicationDatabase> adb(configuration->singleApp().isEmpty()
                                                ? new ApplicationDatabase(configuration->database())
                                                : new ApplicationDatabase());

        if (!adb->isValid() && !configuration->recreateDatabase())
            throw Exception(Error::System, "database file %1 is not a valid application database: %2").arg(adb->name(), adb->errorString());

        if (!adb->isValid() || configuration->recreateDatabase()) {
            QList<const Application *> apps;
            YamlApplicationScanner yas;

            if (!configuration->singleApp().isEmpty()) {
                if (const Application *app = yas.scan(configuration->singleApp()))
                    apps.append(app);
            } else {
                apps = scanForApplications(configuration->builtinAppsManifestDir(),
                                           configuration->installedAppsManifestDir(),
                                           installationLocations);
            }

            foreach (const Application *app, apps)
                qCDebug(LogSystem) << " * APP:" << app->id() << "(" << app->baseDir().absolutePath() << ")";

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

        qmlRegisterSingletonType<ApplicationInstaller>("com.pelagicore.ApplicationInstaller", 0, 1, "ApplicationInstaller",
                                                       &ApplicationInstaller::instanceForQml);
#endif // AM_DISABLE_INSTALLER

        qmlRegisterSingletonType<ApplicationManager>("com.pelagicore.ApplicationManager", 0, 1, "ApplicationManager",
                                                     &ApplicationManager::instanceForQml);
        qmlRegisterType<QmlInProcessNotification>("com.pelagicore.Notification", 0, 1, "Notification");

#if !defined(AM_HEADLESS)
        qmlRegisterSingletonType<WindowManager>("com.pelagicore.ApplicationManager", 0, 1, "WindowManager",
                                                &WindowManager::instanceForQml);
        qmlRegisterType<FakePelagicoreWindow>("com.pelagicore.ApplicationManager", 0, 1, "PelagicoreWindow");
#endif

        startupTimer.checkpoint("after QML registrations");

        QQmlApplicationEngine *engine = new QQmlApplicationEngine(&a);
        new QmlLogger(engine);
        engine->setOutputWarningsToStandardError(false);
        engine->setImportPathList(engine->importPathList() + configuration->importPaths());

        startupTimer.checkpoint("after QML engine instantiation");

#if !defined(AM_HEADLESS)
        QUnifiedTimer::instance()->setSlowModeEnabled(configuration->slowAnimations());
        QQuickView *view = new QQuickView(engine, 0);

        WindowManager *wm = WindowManager::createInstance(view);
        wm->enableWatchdog(!configuration->noUiWatchdog());

        QObject::connect(am, &ApplicationManager::inProcessRuntimeCreated,
                         wm, &WindowManager::setupInProcessRuntime);
        QObject::connect(am, &ApplicationManager::applicationWasReactivated,
                         wm, &WindowManager::raiseApplicationWindow);

        startupTimer.checkpoint("after WindowManager/QuickView instantiation");
        QMetaObject::Connection conn = QObject::connect(view, &QQuickView::frameSwapped, qApp, [&startupTimer, &conn]() {
            static bool once = true;
            if (once) {
                startupTimer.checkpoint("after first frame drawn");
                QObject::disconnect(conn);
                once = false;
            }
        });
#endif
        NotificationManager *nm = NotificationManager::createInstance();

        if (configuration->loadDummyData())
            loadDummyDataFiles(*engine, QFileInfo(configuration->mainQmlFile()).path());

        startupTimer.checkpoint("after NotificationManager instantiation");

#if defined(QT_DBUS_LIB)
        registerDBusObject(new ApplicationManagerAdaptor(am), "com.pelagicore.ApplicationManager", "/Manager");
        if (!am->setDBusPolicy(configuration->dbusPolicy(dbusInterfaceName(am))))
            throw Exception(Error::DBus, "could not set DBus policy for ApplicationManager");

#  if !defined(AM_DISABLE_INSTALLER)
        registerDBusObject(new ApplicationInstallerAdaptor(ai), "com.pelagicore.ApplicationManager", "/Installer");
        if (!ai->setDBusPolicy(configuration->dbusPolicy(dbusInterfaceName(ai))))
            throw Exception(Error::DBus, "could not set DBus policy for ApplicationInstaller");
#  endif

        try {
            registerDBusObject(new NotificationsAdaptor(nm), "org.freedesktop.Notifications", "/org/freedesktop/Notifications");
        } catch (const Exception &e) {
            //TODO: what should we do here? on the desktop this will obviously always fail
            qCCritical(LogSystem) << "WARNING:" << e.what();
        }
        startupTimer.checkpoint("after D-Bus registrations");
#endif // QT_DBUS_LIB

#if defined(QT_PSHELLSERVER_LIB)
        // have a JavaScript shell reachable via telnet protocol
        PTelnetServer telnetServer;
        AMShellFactory shellFactory(view->engine(), view->rootObject());
        telnetServer.setShellFactory(&shellFactory);
        if (!telnetServer.listen(QHostAddress::Any, 0)) {
            throw Exception(Error::System, "could not start Telnet server");
        } else {
            qCDebug(LogSystem) << "Telnet server listening on \n " << telnetServer.serverAddress().toString()
                               << "port" << telnetServer.serverPort();
        }

        // register all objects that should be reachable from the telnet shell
        view->rootContext()->setContextProperty("_ApplicationManager", am);
        view->rootContext()->setContextProperty("_WindowManager", wm);
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
        view->rootContext()->setContextProperty("ssdp", &ssdp);
#endif // QT_PSSDP_LIB

        engine->load(configuration->mainQmlFile());
        if (engine->rootObjects().isEmpty())
            throw Exception(Error::System, "Qml scene does not have a root object");

        startupTimer.checkpoint("after loading main QML file");

#if !defined(AM_HEADLESS)
        view->setContent(configuration->mainQmlFile(), 0, engine->rootObjects().first());
        if (configuration->fullscreen())
            view->showFullScreen();
        else
            view->show();

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
