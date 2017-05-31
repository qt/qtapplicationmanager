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
#include <QNetworkInterface>
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
#include "logging.h"
#include "main.h"
#include "defaultconfiguration.h"
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
#include "package.h"

#if !defined(AM_HEADLESS)
#  include "windowmanager.h"
#  include "fakeapplicationmanagerwindow.h"
#  if defined(QT_DBUS_LIB)
#    include "windowmanager_adaptor.h"
#  endif
#endif

#include "configuration.h"
#include "utilities.h"
#include "exception.h"
#include "crashhandler.h"
#include "qmllogger.h"
#include "startuptimer.h"
#include "systemmonitor.h"
#include "processmonitor.h"
#include "applicationipcmanager.h"
#include "unixsignalhandler.h"

#include "../plugin-interfaces/startupinterface.h"

#if defined(AM_TESTRUNNER)
#  include "testrunner.h"
#  include "qtyaml.h"
#endif


QT_USE_NAMESPACE_AM

Q_DECL_EXPORT int main(int argc, char *argv[])
{
#if defined(Q_OS_UNIX) && defined(AM_MULTI_PROCESS)
    // set a reasonable default for OSes/distros that do not set this by default
    setenv("XDG_RUNTIME_DIR", "/tmp", 0);
#endif

    StartupTimer::instance()->checkpoint("entered main");

    QCoreApplication::setApplicationName(qSL("ApplicationManager"));
    QCoreApplication::setOrganizationName(qSL("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(qSL("pelagicore.com"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));
    for (int i = 1; i < argc; ++i) {
        if (strcmp("--no-dlt-logging", argv[i]) == 0) {
            Logging::setDltEnabled(false);
            break;
        }
    }
    Logging::initialize();
    StartupTimer::instance()->checkpoint("after basic initialization");

#if !defined(AM_DISABLE_INSTALLER)
    Package::ensureCorrectLocale();

    QString error;
    if (Q_UNLIKELY(!forkSudoServer(DropPrivilegesPermanently, &error))) {
        qCCritical(LogSystem) << "ERROR:" << qPrintable(error);
        return 2;
    }
    StartupTimer::instance()->checkpoint("after sudo server fork");
#endif

    DefaultConfiguration cfg;
    cfg.parse();
    StartupTimer::instance()->checkpoint("after command line parse");

    try {
#if !defined(AM_HEADLESS)
        // this is needed for both WebEngine and Wayland Multi-screen rendering
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#  if !defined(QT_NO_SESSIONMANAGER)
        QGuiApplication::setFallbackSessionManagementEnabled(false);
#  endif
#endif
        Main a(argc, argv);

        a.setup(&cfg);
        return a.exec();

    } catch (const std::exception &e) {
        qCCritical(LogSystem) << "ERROR:" << e.what();
        return 2;
    }
}

QT_BEGIN_NAMESPACE_AM

Main::Main(int &argc, char **argv)
    : MainBase(argc, argv)
{
    UnixSignalHandler::instance()->install(UnixSignalHandler::ForwardedToEventLoopHandler, SIGINT,
                                           [](int /*sig*/) {
        UnixSignalHandler::instance()->resetToDefault(SIGINT);
        fputs("\n*** received SIGINT / Ctrl+C ... exiting ***\n\n", stderr);
        static_cast<Main *>(qApp)->shutDown();
    });
    StartupTimer::instance()->checkpoint("after application constructor");
}

Main::~Main()
{
#if defined(AM_TESTRUNNER)
    Q_UNUSED(m_notificationManager);
    Q_UNUSED(m_systemMonitor);
    Q_UNUSED(m_applicationIPCManager);
    Q_UNUSED(m_debuggingEnabler);
    delete m_engine;
#else
    delete m_engine;

    delete m_notificationManager;
#  if !defined(AM_HEADLESS)
    delete m_windowManager;
    delete m_view;
#  endif
    delete m_applicationManager;
    delete m_quickLauncher;
    delete m_systemMonitor;
    delete m_applicationIPCManager;
    delete m_debuggingEnabler;
#endif // defined(AM_TESTRUNNER)
}

/*! \internal
    The caller has to make sure that cfg will be available even after this function returns:
    we will access the cfg object from delayed init functions via lambdas!
*/
void Main::setup(const DefaultConfiguration *cfg) Q_DECL_NOEXCEPT_EXPR(false)
{
    // basics that are needed in multiple setup functions below
    m_noSecurity = cfg->noSecurity();
    m_builtinAppsManifestDirs = cfg->builtinAppsManifestDirs();
    m_installedAppsManifestDir = cfg->installedAppsManifestDir();

    CrashHandler::setCrashActionConfiguration(cfg->managerCrashAction());
    setupLoggingRules(cfg->verbose(), cfg->loggingRules());
    setupQmlDebugging(cfg->qmlDebugging());
    Logging::registerUnregisteredDltContexts();

#if defined(AM_TESTRUNNER)
    TestRunner::initialize(cfg->testRunnerArguments());
#endif

    loadStartupPlugins(cfg->pluginFilePaths("startup"));
    parseSystemProperties(cfg->rawSystemProperties());

    setupDBus(cfg->dbusStartSessionBus());
    QTimer::singleShot(cfg->dbusRegistrationDelay(), this, [this, cfg] {
        registerDBusInterfaces(std::bind(&DefaultConfiguration::dbusRegistration, cfg, std::placeholders::_1),
                std::bind(&DefaultConfiguration::dbusPolicy, cfg, std::placeholders::_1));
    });

    setMainQmlFile(cfg->mainQmlFile());
    setupSingleOrMultiProcess(cfg->forceSingleProcess(), cfg->forceMultiProcess());
    setupRuntimesAndContainers(cfg->runtimeConfigurations(), cfg->containerConfigurations(),
                               cfg->pluginFilePaths("container"));
    loadApplicationDatabase(cfg->database(), cfg->recreateDatabase(), cfg->singleApp());
    setupSingletons(cfg->containerSelectionConfiguration(), cfg->quickLaunchRuntimesPerContainer(),
                    cfg->quickLaunchIdleLoad());

    setupInstaller(cfg->installationLocations(), cfg->appImageMountDir(), cfg->caCertificates(),
                   std::bind(&DefaultConfiguration::applicationUserIdSeparation, cfg,
                             std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    setupQmlEngine(cfg->importPaths(), cfg->style());
    setupWindowTitle(QString(), cfg->windowIcon());
    setupWindowManager(cfg->waylandSocketName(), cfg->slowAnimations(), cfg->noUiWatchdog());
    loadQml(cfg->loadDummyData());
    // --no-fullscreen on the command line trumps the fullscreen setting in the config file
    showWindow(cfg->fullscreen() && !cfg->noFullscreen());
    setupDebugWrappers(cfg->debugWrappers());
    setupShellServer(cfg->telnetAddress(), cfg->telnetPort());
    setupSSDPService();
}

int Main::exec() Q_DECL_NOEXCEPT_EXPR(false)
{
    int res;
#if defined(AM_TESTRUNNER)
    res = TestRunner::exec(m_engine);
#else
    res = MainBase::exec();

    // the eventloop stopped, so any pending "retakes" would not be executed
    QObject *singletons[] = {
        m_applicationManager,
        m_applicationInstaller,
        m_applicationIPCManager,
        m_notificationManager,
        m_windowManager,
        m_systemMonitor
    };
    for (const auto &singleton : singletons)
        retakeSingletonOwnershipFromQmlEngine(m_engine, singleton, true);
#endif // defined(AM_TESTRUNNER)

#if defined(QT_PSSDP_LIB)
    if (m_ssdpOk)
        m_ssdp.setActive(false);
#endif // QT_PSSDP_LIB

    return res;
}

bool Main::isSingleProcessMode() const
{
    return m_isSingleProcessMode;
}

void Main::shutDown()
{
    enum {
        ApplicationManagerDown = 0x01,
        QuickLauncherDown = 0x02,
        WindowManagerDown = 0x04
    };

    static int down = 0;

    static auto checkShutDownFinished = [](int nextDown) {
        down |= nextDown;
        if (down == (ApplicationManagerDown | QuickLauncherDown | WindowManagerDown)) {
            down = 0;
            QCoreApplication::quit();
        }
    };

    if (m_applicationManager) {
        connect(m_applicationManager, &ApplicationManager::shutDownFinished,
                this, []() { checkShutDownFinished(ApplicationManagerDown); });
        m_applicationManager->shutDown();
    }
    if (m_quickLauncher) {
        connect(m_quickLauncher, &QuickLauncher::shutDownFinished,
                this, []() { checkShutDownFinished(QuickLauncherDown); });
        m_quickLauncher->shutDown();
    }
    if (m_windowManager) {
        connect(m_windowManager, &WindowManager::shutDownFinished,
                this, []() { checkShutDownFinished(WindowManagerDown); });
        m_windowManager->shutDown();
    }
}

void Main::setupQmlDebugging(bool qmlDebugging)
{
    if (qmlDebugging) {
#if !defined(QT_NO_QML_DEBUGGER)
        m_debuggingEnabler = new QQmlDebuggingEnabler(true);
        if (!QLoggingCategory::defaultCategory()->isDebugEnabled()) {
            qCCritical(LogQmlRuntime) << "The default 'debug' logging category was disabled. "
                                         "Re-enabling it for the QML Debugger interface to work correctly.";
            QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);
        }
#else
        qCWarning(LogSystem) << "The --qml-debug option is ignored, because Qt was built without support for QML Debugging!";
#endif
    }
}

void Main::setupLoggingRules(bool verbose, const QStringList &loggingRules)
{
    const QString rules = verbose ? qSL("*=true\nqt.*.debug=false")
                                  : loggingRules.isEmpty() ? qSL("*.debug=false")
                                                           : loggingRules.join(qL1C('\n'));

    QLoggingCategory::setFilterRules(rules);

    // setting this for child processes //TODO: use a more generic IPC approach
    qputenv("AM_LOGGING_RULES", rules.toUtf8());
    StartupTimer::instance()->checkpoint("after logging setup");
}

void Main::loadStartupPlugins(const QStringList &startupPluginPaths) Q_DECL_NOEXCEPT_EXPR(false)
{
    m_startupPlugins = loadPlugins<StartupInterface>("startup", startupPluginPaths);
    StartupTimer::instance()->checkpoint("after startup-plugin load");
}

void Main::parseSystemProperties(const QVariantMap &rawSystemProperties)
{
    m_systemProperties.resize(SP_SystemUi + 1);
    QVariantMap rawMap = rawSystemProperties;

    m_systemProperties[SP_ThirdParty] = rawMap.value(qSL("public")).toMap();

    m_systemProperties[SP_BuiltIn] = m_systemProperties.at(SP_ThirdParty);
    const QVariantMap pro = rawMap.value(qSL("protected")).toMap();
    for (auto it = pro.cbegin(); it != pro.cend(); ++it)
        m_systemProperties[SP_BuiltIn].insert(it.key(), it.value());

    m_systemProperties[SP_SystemUi] = m_systemProperties.at(SP_BuiltIn);
    const QVariantMap pri = rawMap.value(qSL("private")).toMap();
    for (auto it = pri.cbegin(); it != pri.cend(); ++it)
        m_systemProperties[SP_SystemUi].insert(it.key(), it.value());

    for (auto iface : qAsConst(m_startupPlugins))
        iface->initialize(m_systemProperties.at(SP_SystemUi));
}

void Main::setupDBus(bool startSessionBus) Q_DECL_NOEXCEPT_EXPR(false)
{
#if defined(QT_DBUS_LIB)
    if (Q_LIKELY(!startSessionBus))
        return;

    class DBusDaemonProcess : public QProcess // clazy:exclude=missing-qobject-macro
    {
    public:
        DBusDaemonProcess(QObject *parent = nullptr)
            : QProcess(parent)
        {
            QString program = qSL("dbus-daemon");
            QStringList arguments = { qSL("--nofork"), qSL("--print-address"), qSL("--session") };

#if defined(Q_OS_OSX)
            // there's no native dbus support on macOS, so we try to use brew's dbus support
            program = qSL("/usr/local/bin/dbus-daemon");
            // brew's dbus-daemon needs an address, because it will otherwise assume that it was
            // started via launchd and expects its address in $DBUS_LAUNCHD_SESSION_BUS_SOCKET
            QString address = qSL("--address=unix:path=") + QDir::tempPath() + qSL("am-")
                    + QString::number(QCoreApplication::applicationPid()) + qSL("-session.bus");

            arguments << address;
#endif
            setProgram(program);
            setArguments(arguments);
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

    static const int timeout = 10000 * timeoutFactor();

    auto dbusDaemon = new DBusDaemonProcess(this);
    dbusDaemon->start(QIODevice::ReadOnly);
    if (!dbusDaemon->waitForStarted(timeout) || !dbusDaemon->waitForReadyRead(timeout)) {
        throw Exception("could not start a dbus-daemon process (%1): %2")
                .arg(dbusDaemon->program(), dbusDaemon->errorString());
    }
    QByteArray busAddress = dbusDaemon->readAllStandardOutput().trimmed();
    qputenv("DBUS_SESSION_BUS_ADDRESS", busAddress);
    qCInfo(LogSystem, "NOTICE: running on private D-Bus session bus to avoid conflicts:");
    qCInfo(LogSystem, "        DBUS_SESSION_BUS_ADDRESS=%s", busAddress.constData());

    StartupTimer::instance()->checkpoint("after starting session D-Bus");
#endif // QT_DBUS_LIB
}

void Main::setMainQmlFile(const QString &mainQml) Q_DECL_NOEXCEPT_EXPR(false)
{
    m_mainQml = mainQml;
    if (Q_UNLIKELY(!QFile::exists(m_mainQml)))
        throw Exception("no/invalid main QML file specified: %1").arg(m_mainQml);
}

void Main::setupInstaller(const QVariantList &installationLocations, const QString &appImageMountDir,
                          const QStringList &caCertificatePaths, const std::function<bool(uint *, uint *, uint *)> &userIdSeparation) Q_DECL_NOEXCEPT_EXPR(false)
{
#if !defined(AM_DISABLE_INSTALLER)
    if (!Package::checkCorrectLocale()) {
        // we should really throw here, but so many embedded systems are badly set up
        qCCritical(LogSystem) << "WARNING: the appman installer needs a UTF-8 locale to work correctly:\n"
                                 "         even automatically switching to C.UTF-8 or en_US.UTF-8 failed.";
    }

    if (Q_UNLIKELY(hardwareId().isEmpty()))
        throw Exception("the installer is enabled, but the device-id is empty");

    m_installationLocations = InstallationLocation::parseInstallationLocations(installationLocations,
                                                                               hardwareId());

    if (Q_UNLIKELY(!QDir::root().mkpath(m_installedAppsManifestDir)))
        throw Exception("could not create manifest directory %1").arg(m_installedAppsManifestDir);

    if (Q_UNLIKELY(!QDir::root().mkpath(appImageMountDir)))
        throw Exception("could not create the image-mount directory %1").arg(appImageMountDir);

    StartupTimer::instance()->checkpoint("after installer setup checks");

    QString error;
    m_applicationInstaller = ApplicationInstaller::createInstance(m_installationLocations,
                                                                  m_installedAppsManifestDir,
                                                                  appImageMountDir,
                                                                  &error);
    if (Q_UNLIKELY(!m_applicationInstaller))
        throw Exception(Error::System, error);
    if (m_noSecurity) {
        m_applicationInstaller->setDevelopmentMode(true);
        m_applicationInstaller->setAllowInstallationOfUnsignedPackages(true);
    } else {
        QList<QByteArray> caCertificateList;

        for (const auto &caFile : caCertificatePaths) {
            QFile f(caFile);
            if (Q_UNLIKELY(!f.open(QFile::ReadOnly)))
                throw Exception(f, "could not open CA-certificate file");
            QByteArray cert = f.readAll();
            if (Q_UNLIKELY(cert.isEmpty()))
                throw Exception(f, "CA-certificate file is empty");
            caCertificateList << cert;
        }
        m_applicationInstaller->setCACertificates(caCertificateList);
    }

    uint minUserId, maxUserId, commonGroupId;
    if (userIdSeparation && userIdSeparation(&minUserId, &maxUserId, &commonGroupId)) {
#  if defined(Q_OS_LINUX)
        if (!m_applicationInstaller->enableApplicationUserIdSeparation(minUserId, maxUserId, commonGroupId))
            throw Exception("could not enable application user-id separation in the installer.");
#  else
        qCCritical(LogSystem) << "WARNING: application user-id separation requested, but not possible on this platform.";
#  endif // Q_OS_LINUX
    }

    //TODO: this could be delayed, but needs to have a lock on the app-db in this case
    m_applicationInstaller->cleanupBrokenInstallations();

    StartupTimer::instance()->checkpoint("after ApplicationInstaller instantiation");
#endif // AM_DISABLE_INSTALLER
}

void Main::setupSingleOrMultiProcess(bool forceSingleProcess, bool forceMultiProcess) Q_DECL_NOEXCEPT_EXPR(false)
{
    m_isSingleProcessMode = forceSingleProcess;
    if (forceMultiProcess && m_isSingleProcessMode)
        throw Exception("You cannot enforce multi- and single-process mode at the same time.");

#if !defined(AM_MULTI_PROCESS)
    if (forceMultiProcess)
        throw Exception("This application manager build is not multi-process capable.");
    m_isSingleProcessMode = true;
#endif
}

void Main::setupRuntimesAndContainers(const QVariantMap &runtimeConfigurations, const QVariantMap &containerConfigurations, const QStringList &containerPluginPaths)
{
    if (m_isSingleProcessMode) {
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
        auto containerPlugins = loadPlugins<ContainerManagerInterface>("container", containerPluginPaths);
        for (auto iface : qAsConst(containerPlugins))
            ContainerFactory::instance()->registerContainer(new PluginContainerManager(iface));
    }
    for (auto iface : qAsConst(m_startupPlugins))
        iface->afterRuntimeRegistration();

    ContainerFactory::instance()->setConfiguration(containerConfigurations);
    RuntimeFactory::instance()->setConfiguration(runtimeConfigurations);

    RuntimeFactory::instance()->setSystemProperties(m_systemProperties.at(SP_ThirdParty),
                                                    m_systemProperties.at(SP_BuiltIn));

    StartupTimer::instance()->checkpoint("after runtime registration");
}

void Main::loadApplicationDatabase(const QString &databasePath, bool recreateDatabase,
                                   const QString &singleApp) Q_DECL_NOEXCEPT_EXPR(false)
{
    m_applicationDatabase.reset(singleApp.isEmpty() ? new ApplicationDatabase(databasePath)
                                                    : new ApplicationDatabase());

    if (Q_UNLIKELY(!m_applicationDatabase->isValid() && !recreateDatabase)) {
        throw Exception("database file %1 is not a valid application database: %2")
            .arg(m_applicationDatabase->name(), m_applicationDatabase->errorString());
    }

    if (!m_applicationDatabase->isValid() || recreateDatabase) {
        QVector<const Application *> apps;

        if (!singleApp.isEmpty()) {
            apps = scanForApplication(singleApp, m_builtinAppsManifestDirs);
        } else {
            apps = scanForApplications(m_builtinAppsManifestDirs,
                                       m_installedAppsManifestDir,
                                       m_installationLocations);
        }

        if (LogSystem().isDebugEnabled()) {
            qCDebug(LogSystem) << "Registering applications:";
            for (const Application *app : qAsConst(apps))
                qCDebug(LogSystem).nospace().noquote() << " * " << app->id() << " [at: " << QDir(app->codeDir()).path() << "]";
        }

        m_applicationDatabase->write(apps);
        qDeleteAll(apps);
    }

    StartupTimer::instance()->checkpoint("after application database loading");
}

void Main::setupSingletons(const QList<QPair<QString, QString>> &containerSelectionConfiguration,
                           qreal quickLaunchRuntimesPerContainer, int quickLaunchIdleLoad) Q_DECL_NOEXCEPT_EXPR(false)
{
    QString error;
    m_applicationManager = ApplicationManager::createInstance(m_applicationDatabase.take(),
                                                              m_isSingleProcessMode, &error);
    if (Q_UNLIKELY(!m_applicationManager))
        throw Exception(Error::System, error);
    if (m_noSecurity)
        m_applicationManager->setSecurityChecksEnabled(false);

    m_applicationManager->setSystemProperties(m_systemProperties.at(SP_SystemUi));
    m_applicationManager->setContainerSelectionConfiguration(containerSelectionConfiguration);

    StartupTimer::instance()->checkpoint("after ApplicationManager instantiation");

    m_notificationManager = NotificationManager::createInstance();
    StartupTimer::instance()->checkpoint("after NotificationManager instantiation");

    m_applicationIPCManager = ApplicationIPCManager::createInstance();
    StartupTimer::instance()->checkpoint("after ApplicationIPCManager instantiation");

    m_systemMonitor = SystemMonitor::createInstance();
    StartupTimer::instance()->checkpoint("after SystemMonitor instantiation");

    m_quickLauncher = QuickLauncher::instance();
    m_quickLauncher->initialize(quickLaunchRuntimesPerContainer, quickLaunchIdleLoad);
    StartupTimer::instance()->checkpoint("after quick-launcher setup");
}

void Main::setupQmlEngine(const QStringList &importPaths, const QString &quickControlsStyle)
{
    if (!quickControlsStyle.isEmpty())
        qputenv("QT_QUICK_CONTROLS_STYLE", quickControlsStyle.toLocal8Bit());

    qmlRegisterType<QmlInProcessNotification>("QtApplicationManager", 1, 0, "Notification");
    qmlRegisterType<QmlInProcessApplicationInterfaceExtension>("QtApplicationManager", 1, 0, "ApplicationInterfaceExtension");

#if !defined(AM_HEADLESS)
    qmlRegisterType<FakeApplicationManagerWindow>("QtApplicationManager", 1, 0, "ApplicationManagerWindow");
#endif
    qmlRegisterType<ProcessMonitor>("QtApplicationManager", 1, 0, "ProcessMonitor");

    StartupTimer::instance()->checkpoint("after QML registrations");

    m_engine = new QQmlApplicationEngine(this);
    connect(m_engine, &QQmlEngine::quit, this, &Main::shutDown);
    new QmlLogger(m_engine);
    m_engine->setOutputWarningsToStandardError(false);
    m_engine->setImportPathList(m_engine->importPathList() + importPaths);
    m_engine->rootContext()->setContextProperty("StartupTimer", StartupTimer::instance());

    StartupTimer::instance()->checkpoint("after QML engine instantiation");

#if defined(AM_TESTRUNNER)
    QFile f(qSL(":/build-config.yaml"));
    QVector<QVariant> docs;
    if (f.open(QFile::ReadOnly))
        docs = QtYaml::variantDocumentsFromYaml(f.readAll());
    f.close();
    m_engine->rootContext()->setContextProperty("buildConfig", docs.toList());
#endif
}

void Main::setupWindowTitle(const QString &title, const QString &iconPath)
{
#if !defined(AM_HEADLESS)
    // For development only: set an icon, so you know which window is the AM
    bool setTitle =
#  if defined(Q_OS_LINUX)
            (platformName() == qL1S("xcb"));
#  else
            true;
#  endif
    if (Q_UNLIKELY(setTitle)) {
        if (!iconPath.isEmpty())
            QGuiApplication::setWindowIcon(QIcon(iconPath));
        if (!title.isEmpty())
            QGuiApplication::setApplicationDisplayName(title);
    }
#endif // AM_HEADLESS
}

void Main::setupWindowManager(const QString &waylandSocketName, bool slowAnimations, bool uiWatchdog)
{
#if !defined(AM_HEADLESS)
    QUnifiedTimer::instance()->setSlowModeEnabled(slowAnimations);

    m_windowManager = WindowManager::createInstance(m_engine, waylandSocketName);
    m_windowManager->setSlowAnimations(slowAnimations);
    m_windowManager->enableWatchdog(!uiWatchdog);

    QObject::connect(m_applicationManager, &ApplicationManager::inProcessRuntimeCreated,
                     m_windowManager, &WindowManager::setupInProcessRuntime);
    QObject::connect(m_applicationManager, &ApplicationManager::applicationWasActivated,
                     m_windowManager, &WindowManager::raiseApplicationWindow);
#endif
}


void Main::loadQml(bool loadDummyData) Q_DECL_NOEXCEPT_EXPR(false)
{
    for (auto iface : qAsConst(m_startupPlugins))
        iface->beforeQmlEngineLoad(m_engine);

    if (Q_UNLIKELY(loadDummyData)) {
        loadDummyDataFiles();
        StartupTimer::instance()->checkpoint("after loading dummy-data");
    }

    m_engine->load(m_mainQml);
    if (Q_UNLIKELY(m_engine->rootObjects().isEmpty()))
        throw Exception("Qml scene does not have a root object");

    for (auto iface : qAsConst(m_startupPlugins))
        iface->afterQmlEngineLoad(m_engine);

    StartupTimer::instance()->checkpoint("after loading main QML file");
}

void Main::showWindow(bool showFullscreen)
{
#if !defined(AM_HEADLESS)
    setQuitOnLastWindowClosed(false);
    connect(this, &QGuiApplication::lastWindowClosed, this, &Main::shutDown);

    QQuickWindow *window = nullptr;
    QObject *rootObject = m_engine->rootObjects().constFirst();

    if (!rootObject->isWindowType()) {
        m_view = new QQuickView(m_engine, nullptr);
        StartupTimer::instance()->checkpoint("after WindowManager/QuickView instantiation");
        m_view->setContent(m_mainQml, nullptr, rootObject);
        window = m_view;
    } else {
        window = qobject_cast<QQuickWindow *>(rootObject);
        if (!m_engine->incubationController())
            m_engine->setIncubationController(window->incubationController());
    }
    Q_ASSERT(window);

    static QMetaObject::Connection conn = QObject::connect(window, &QQuickWindow::frameSwapped, this, []() {
        // this is a queued signal, so there may be still one in the queue after calling disconnect()
        if (conn) {
#if defined(Q_CC_MSVC)
            qApp->disconnect(conn); // MSVC2013 cannot call static member functions without capturing this
#else
            QObject::disconnect(conn);
#endif
            StartupTimer::instance()->checkpoint("after first frame drawn");
            StartupTimer::instance()->createReport(qSL("System UI"));
        }
    });

    m_windowManager->registerCompositorView(window);

    for (auto iface : qAsConst(m_startupPlugins))
        iface->beforeWindowShow(window);

    if (Q_LIKELY(showFullscreen))
        window->showFullScreen();
    else
        window->show();

    for (auto iface : qAsConst(m_startupPlugins))
        iface->afterWindowShow(window);

    StartupTimer::instance()->checkpoint("after window show");
#endif
}

void Main::setupDebugWrappers(const QVariantList &debugWrappers)
{
    // delay debug-wrapper setup
    //TODO: find a better solution than hardcoding an 1.5 sec delay
    QTimer::singleShot(1500, this, [this, debugWrappers]() {
        m_applicationManager->setDebugWrapperConfiguration(debugWrappers);
    });
}

void Main::setupShellServer(const QString &telnetAddress, quint16 telnetPort) Q_DECL_NOEXCEPT_EXPR(false)
{
     //TODO: could be delayed as well
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

    // have a JavaScript shell reachable via telnet protocol
    PTelnetServer telnetServer;
    AMShellFactory shellFactory(m_engine, m_engine->rootObjects().constFirst());
    telnetServer.setShellFactory(&shellFactory);

    if (!telnetServer.listen(QHostAddress(telnetAddress), telnetPort)) {
        throw Exception("could not start Telnet server");
    } else {
        qCDebug(LogSystem) << "Telnet server listening on \n " << telnetServer.serverAddress().toString()
                           << "port" << telnetServer.serverPort();
    }

    // register all objects that should be reachable from the telnet shell
    m_engine->rootContext()->setContextProperty("_ApplicationManager", m_am);
    m_engine->rootContext()->setContextProperty("_WindowManager", m_wm);
#else
    Q_UNUSED(telnetAddress)
    Q_UNUSED(telnetPort)
#endif // QT_PSHELLSERVER_LIB
}

void Main::setupSSDPService() Q_DECL_NOEXCEPT_EXPR(false)
{
     //TODO: could be delayed as well
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
    m_engine->rootContext()->setContextProperty("ssdp", &ssdp);
#endif // QT_PSSDP_LIB
}

const char *Main::dbusInterfaceName(QObject *o) const Q_DECL_NOEXCEPT_EXPR(false)
{
#if defined(QT_DBUS_LIB)
    int idx = o->metaObject()->indexOfClassInfo("D-Bus Interface");
    if (idx < 0) {
        throw Exception("Could not get class-info \"D-Bus Interface\" for D-Bus adapter %1")
            .arg(o->metaObject()->className());
    }
    return o->metaObject()->classInfo(idx).value();
#else
    Q_UNUSED(o)
    return nullptr;
#endif
}

void Main::registerDBusObject(QDBusAbstractAdaptor *adaptor, const QString &dbusName, const char *serviceName, const char *interfaceName, const char *path) Q_DECL_NOEXCEPT_EXPR(false)
{
#if defined(QT_DBUS_LIB)
    QString dbusAddress;
    QDBusConnection conn((QString()));

    if (dbusName.isEmpty()) {
        return;
    } else if (dbusName == qL1S("system")) {
        dbusAddress = qgetenv("DBUS_SYSTEM_BUS_ADDRESS");
#  if defined(Q_OS_LINUX)
        if (dbusAddress.isEmpty())
            dbusAddress = qL1S("unix:path=/var/run/dbus/system_bus_socket");
#  endif
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

    if (QByteArray::fromRawData(interfaceName, qstrlen(interfaceName)).startsWith("io.qt.")) {
        // Write the bus address of the interface to a file in /tmp. This is needed for the
        // controller tool, which does not even have a session bus, when started via ssh.

        QFile f(QDir::temp().absoluteFilePath(qL1S(interfaceName) + qSL(".dbus")));
        QByteArray dbusUtf8 = dbusAddress.isEmpty() ? dbusName.toUtf8() : dbusAddress.toUtf8();
        if (!f.open(QFile::WriteOnly | QFile::Truncate) || (f.write(dbusUtf8) != dbusUtf8.size()))
            throw Exception(f, "Could not write D-Bus address of interface %1").arg(interfaceName);

        static QStringList filesToDelete;
        if (filesToDelete.isEmpty())
            atexit([]() { for (const QString &ftd : qAsConst(filesToDelete)) QFile::remove(ftd); });
        filesToDelete << f.fileName();
    }
#else
    Q_UNUSED(adaptor)
    Q_UNUSED(serviceName)
    Q_UNUSED(path)
#endif // QT_DBUS_LIB
}

void Main::registerDBusInterfaces(const std::function<QString(const QString &)> &busForInterface,
                                  const std::function<QVariantMap(const QString &)> &policyForInterface)
{
#if defined(QT_DBUS_LIB)
    registerDBusTypes();

    try {
        qCDebug(LogSystem) << "Registering D-Bus services:";

        auto ama = new ApplicationManagerAdaptor(m_applicationManager);

        // connect this signal manually, since it needs a type conversion
        // (the automatic signal relay fails in this case)
        QObject::connect(m_applicationManager, &ApplicationManager::applicationRunStateChanged,
                         ama, [ama](const QString &id, ApplicationManager::RunState runState) {
            ama->applicationRunStateChanged(id, runState);
        });
        const char *amInterfaceName = dbusInterfaceName(m_applicationManager);
        registerDBusObject(ama, busForInterface(amInterfaceName), "io.qt.ApplicationManager",
                           amInterfaceName, "/ApplicationManager");
        if (!m_applicationManager->setDBusPolicy(policyForInterface(amInterfaceName)))
            throw Exception(Error::DBus, "could not set DBus policy for ApplicationManager");

#  if !defined(AM_DISABLE_INSTALLER)
        const char *aiInterfaceName = dbusInterfaceName(m_applicationInstaller);
        registerDBusObject(new ApplicationInstallerAdaptor(m_applicationInstaller),
                           busForInterface(aiInterfaceName), "io.qt.ApplicationManager",
                           aiInterfaceName, "/ApplicationInstaller");
        if (!m_applicationInstaller->setDBusPolicy(policyForInterface(aiInterfaceName)))
            throw Exception(Error::DBus, "could not set DBus policy for ApplicationInstaller");
#  endif

#  if !defined(AM_HEADLESS)
        try {
            const char *nmInterfaceName = dbusInterfaceName(m_notificationManager);
            registerDBusObject(new NotificationsAdaptor(m_notificationManager),
                               busForInterface(nmInterfaceName), "org.freedesktop.Notifications",
                               nmInterfaceName, "/org/freedesktop/Notifications");
        } catch (const Exception &e) {
            //TODO: what should we do here? on the desktop this will obviously always fail
            qCCritical(LogSystem) << "WARNING:" << e.what();
        }
        const char *wmInterfaceName = dbusInterfaceName(m_windowManager);
        registerDBusObject(new WindowManagerAdaptor(m_windowManager), busForInterface(wmInterfaceName),
                           "io.qt.ApplicationManager", wmInterfaceName, "/WindowManager");
        if (!m_windowManager->setDBusPolicy(policyForInterface(wmInterfaceName)))
            throw Exception(Error::DBus, "could not set DBus policy for WindowManager");
#endif
    } catch (const std::exception &e) {
        qCCritical(LogSystem) << "ERROR:" << e.what();
        qApp->exit(2);
    }
#endif // QT_DBUS_LIB
}

// copied straight from Qt 5.1.0 qmlscene/main.cpp for now - needs to be revised
void Main::loadDummyDataFiles()
{
    QString directory = QFileInfo(m_mainQml).path();

    QDir dir(directory + qSL("/dummydata"), qSL("*.qml"));
    QStringList list = dir.entryList();
    for (int i = 0; i < list.size(); ++i) {
        QString qml = list.at(i);
        QQmlComponent comp(m_engine, dir.filePath(qml));
        QObject *dummyData = comp.create();

        if (comp.isError()) {
            const QList<QQmlError> errors = comp.errors();
            for (const QQmlError &error : errors)
                qWarning() << error;
        }

        if (dummyData) {
            qWarning() << "Loaded dummy data:" << dir.filePath(qml);
            qml.truncate(qml.length()-4);
            m_engine->rootContext()->setContextProperty(qml, dummyData);
            dummyData->setParent(m_engine);
        }
    }
}

QVector<const Application *> Main::scanForApplication(const QString &singleAppInfoYaml, const QStringList &builtinAppsDirs) Q_DECL_NOEXCEPT_EXPR(false)
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

QVector<const Application *> Main::scanForApplications(const QStringList &builtinAppsDirs, const QString &installedAppsDir,
                                                        const QVector<InstallationLocation> &installationLocations) Q_DECL_NOEXCEPT_EXPR(false)
{
    QVector<const Application *> result;
    YamlApplicationScanner yas;

    auto scan = [&result, &yas, &installationLocations](const QDir &baseDir, bool scanningBuiltinApps) {
        auto flags = scanningBuiltinApps ? QDir::Dirs | QDir::NoDotAndDotDot
                                         : QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;
        const QStringList appDirNames = baseDir.entryList(flags);

        for (const QString &appDirName : appDirNames) {
            if (appDirName.endsWith('+') || appDirName.endsWith('-'))
                continue;
            QString appIdError;
            if (!Application::isValidApplicationId(appDirName, false, &appIdError)) {
                qCDebug(LogSystem) << "Ignoring application directory" << appDirName
                                   << ": not a valid application-id:" << qPrintable(appIdError);
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
                throw Exception(Error::Parse, "an info.yaml for built-in applications must be in a directory "
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
                for (const InstallationLocation &il : installationLocations) {
                    if (il.id() == report->installationLocationId()) {
                        a->setCodeDir(il.installationPath() + a->id());
                        break;
                    }
                }
#endif
                a->setInstallationReport(report.take());
                result << a.take();
            }
        }
    };

    for (const QString &dir : builtinAppsDirs)
        scan(dir, true);
#if !defined(AM_DISABLE_INSTALLER)
    scan(installedAppsDir, false);
#endif
    return result;
}

QString Main::hardwareId() const
{
#if defined(AM_HARDWARE_ID)
    return QString::fromLocal8Bit(AM_HARDWARE_ID);
#elif defined(AM_HARDWARE_ID_FROM_FILE)
    QFile f(QString::fromLocal8Bit(AM_HARDWARE_ID_FROM_FILE));
    if (f.open(QFile::ReadOnly))
        return QString::fromLocal8Bit(f.readAll().trimmed());
#else
    const auto allInterfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : allInterfaces) {
        if (iface.isValid() && (iface.flags() & QNetworkInterface::IsUp)
                && !(iface.flags() & (QNetworkInterface::IsPointToPoint | QNetworkInterface::IsLoopBack))
                && !iface.hardwareAddress().isEmpty()) {
            return iface.hardwareAddress().replace(qL1C(':'), qL1S("-"));
        }
    }
#endif
    return QString();
}

QT_END_NAMESPACE_AM
