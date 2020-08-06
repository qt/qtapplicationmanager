/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#if defined(QT_DBUS_LIB) && !defined(AM_DISABLE_EXTERNAL_DBUS_INTERFACES)
#  include <QDBusConnection>
#  include <QDBusAbstractAdaptor>
#  include "dbusdaemon.h"
#  include "dbuspolicy.h"
#  include "applicationmanagerdbuscontextadaptor.h"
#  include "packagemanagerdbuscontextadaptor.h"
#  include "notificationmanagerdbuscontextadaptor.h"
#endif

#include "applicationipcmanager.h"

#include <QFile>
#include <QDir>
#include <QStringList>
#include <QVariant>
#include <QFileInfo>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QTimer>
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
#  include <QTouchDevice>
#  include <private/qopenglcontext_p.h>
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
#include "configuration.h"
#include "applicationmanager.h"
#include "package.h"
#include "packagemanager.h"
#include "packagedatabase.h"
#include "installationreport.h"
#include "yamlpackagescanner.h"
#if !defined(AM_DISABLE_INSTALLER)
#  include "applicationinstaller.h"
#  include "sudo.h"
#  include "packageutilities.h"
#endif
#include "runtimefactory.h"
#include "containerfactory.h"
#include "quicklauncher.h"
#if defined(AM_MULTI_PROCESS)
#  include "processcontainer.h"
#  include "nativeruntime.h"
#endif
#include "plugincontainer.h"
#include "notificationmanager.h"
#include "qmlinprocessruntime.h"
#include "qmlinprocessapplicationinterface.h"
#include "qml-utilities.h"
#include "dbus-utilities.h"
#include "intentserver.h"
#include "intentaminterface.h"

#if !defined(AM_HEADLESS)
#  include "windowmanager.h"
#  include "qmlinprocessapplicationmanagerwindow.h"
#  if defined(QT_DBUS_LIB) && !defined(AM_DISABLE_EXTERNAL_DBUS_INTERFACES)
#    include "windowmanagerdbuscontextadaptor.h"
#  endif
#  include "touchemulation.h"
#  include "windowframetimer.h"
#  include "gpustatus.h"
#endif

#include "configuration.h"
#include "utilities.h"
#include "exception.h"
#include "crashhandler.h"
#include "qmllogger.h"
#include "startuptimer.h"
#include "applicationipcmanager.h"
#include "unixsignalhandler.h"

// monitor-lib
#include "cpustatus.h"
#include "iostatus.h"
#include "memorystatus.h"
#include "monitormodel.h"
#include "processstatus.h"

#include "../plugin-interfaces/startupinterface.h"


QT_BEGIN_NAMESPACE_AM

// We need to do some things BEFORE the Q*Application constructor runs, so we're using this
// old trick to do this hooking transparently for the user of the class.
int &Main::preConstructor(int &argc)
{
#if !defined(AM_DISABLE_INSTALLER)
    // try to set a reasonable locale - we later verify with checkCorrectLocale() if we succeeded
    PackageUtilities::ensureCorrectLocale();
#endif
    return argc;
}

Main::Main(int &argc, char **argv)
    : MainBase(SharedMain::preConstructor(Main::preConstructor(argc)), argv)
    , SharedMain()
{
    m_isRunningOnEmbedded =
#if defined(Q_OS_LINUX)
            !qEnvironmentVariableIsSet("XDG_CURRENT_DESKTOP");
#else
            false;
#endif

    // this might be needed later on by the native runtime to find a suitable qml runtime launcher
    setProperty("_am_build_dir", qSL(AM_BUILD_DIR));

    UnixSignalHandler::instance()->install(UnixSignalHandler::ForwardedToEventLoopHandler,
                                           { SIGINT, SIGTERM },
                                           [](int sig) {
        UnixSignalHandler::instance()->resetToDefault(sig);
        if (sig == SIGINT)
            fputs("\n*** received SIGINT / Ctrl+C ... exiting ***\n\n", stderr);
        static_cast<Main *>(QCoreApplication::instance())->shutDown();
    });
    StartupTimer::instance()->checkpoint("after application constructor");
}

Main::~Main()
{
#if defined(QT_PSSDP_LIB)
    if (m_ssdpOk)
        m_ssdp.setActive(false);
#endif // QT_PSSDP_LIB

    delete m_engine;

    delete m_intentServer;
    delete m_notificationManager;
#  if !defined(AM_HEADLESS)
    delete m_windowManager;
    delete m_view;
#  endif
    delete m_applicationManager;
#if !defined(AM_DISABLE_INSTALLER)
    delete m_applicationInstaller;
#endif
    delete m_packageManager;
    delete m_quickLauncher;
    delete m_applicationIPCManager;

    delete RuntimeFactory::instance();
    delete ContainerFactory::instance();
    delete StartupTimer::instance();
}

/*! \internal
    The caller has to make sure that cfg will be available even after this function returns:
    we will access the cfg object from delayed init functions via lambdas!
*/
void Main::setup(const Configuration *cfg, const QStringList &deploymentWarnings) Q_DECL_NOEXCEPT_EXPR(false)
{
    Q_UNUSED(deploymentWarnings)
    setup(cfg);
}

void Main::setup(const Configuration *cfg) Q_DECL_NOEXCEPT_EXPR(false)
{
    // basics that are needed in multiple setup functions below
    m_noSecurity = cfg->noSecurity();
    m_developmentMode = cfg->developmentMode();
    m_builtinAppsManifestDirs = cfg->builtinAppsManifestDirs();
    m_installationDir = cfg->installationDir();
    m_documentDir = cfg->documentDir();

    CrashHandler::setCrashActionConfiguration(cfg->managerCrashAction());
    setupQmlDebugging(cfg->qmlDebugging());
    if (!cfg->dltId().isEmpty() || !cfg->dltDescription().isEmpty())
        Logging::setSystemUiDltId(cfg->dltId().toLocal8Bit(), cfg->dltDescription().toLocal8Bit());
    Logging::registerUnregisteredDltContexts();
    setupLogging(cfg->verbose(), cfg->loggingRules(), cfg->messagePattern(), cfg->useAMConsoleLogger());

    registerResources(cfg->resources());

    setupOpenGL(cfg->openGLConfiguration());
    setupIconTheme(cfg->iconThemeSearchPaths(), cfg->iconThemeName());

    loadStartupPlugins(cfg->pluginFilePaths("startup"));
    parseSystemProperties(cfg->rawSystemProperties());

    setMainQmlFile(cfg->mainQmlFile());
    setupSingleOrMultiProcess(cfg->forceSingleProcess(), cfg->forceMultiProcess());
    setupRuntimesAndContainers(cfg->runtimeConfigurations(), cfg->openGLConfiguration(),
                               cfg->containerConfigurations(), cfg->pluginFilePaths("container"),
                               cfg->iconThemeSearchPaths(), cfg->iconThemeName());

    loadPackageDatabase(cfg->clearCache() || cfg->noCache(), cfg->singleApp());

    setupSingletons(cfg->containerSelectionConfiguration(), cfg->quickLaunchRuntimesPerContainer(),
                    cfg->quickLaunchIdleLoad());

    if (!cfg->disableIntents()) {
        setupIntents(cfg->intentTimeoutForDisambiguation(), cfg->intentTimeoutForStartApplication(),
                     cfg->intentTimeoutForReplyFromApplication(), cfg->intentTimeoutForReplyFromSystem());
    }

    registerPackages();

    if (m_installationDir.isEmpty() || cfg->disableInstaller()) {
        StartupTimer::instance()->checkpoint("skipping installer");
    } else {
        setupInstaller(cfg->caCertificates(),
                       std::bind(&Configuration::applicationUserIdSeparation, cfg,
                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    setLibraryPaths(libraryPaths() + cfg->pluginPaths());
    setupQmlEngine(cfg->importPaths(), cfg->style());
    setupWindowTitle(QString(), cfg->windowIcon());
    setupWindowManager(cfg->waylandSocketName(), cfg->slowAnimations(), cfg->noUiWatchdog());
    setupTouchEmulation(cfg->enableTouchEmulation());
    setupShellServer(QString(), 0); // remove
    setupSSDPService();

    setupDBus(std::bind(&Configuration::dbusRegistration, cfg, std::placeholders::_1),
              std::bind(&Configuration::dbusPolicy, cfg, std::placeholders::_1));
}

bool Main::isSingleProcessMode() const
{
    return m_isSingleProcessMode;
}

bool Main::isRunningOnEmbedded() const
{
    return m_isRunningOnEmbedded;
}

void Main::shutDown(int exitCode)
{
    enum {
        ApplicationManagerDown = 0x01,
        QuickLauncherDown = 0x02,
        WindowManagerDown = 0x04
    };

    static int down = 0;

    static auto checkShutDownFinished = [exitCode](int nextDown) {
        down |= nextDown;
        if (down == (ApplicationManagerDown | QuickLauncherDown | WindowManagerDown)) {
            down = 0;
            QCoreApplication::exit(exitCode);
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
    } else {
        down |= QuickLauncherDown;
    }
#if !defined(AM_HEADLESS)
    if (m_windowManager) {
        connect(m_windowManager, &WindowManager::shutDownFinished,
                this, []() { checkShutDownFinished(WindowManagerDown); });
        m_windowManager->shutDown();
    }
#else
    checkShutDownFinished(WindowManagerDown);
#endif

    QTimer::singleShot(5000, [exitCode] {
        QStringList resources;
        if (!(down & ApplicationManagerDown))
            resources << qSL("runtimes");
        if (!(down & QuickLauncherDown))
            resources << qSL("quick-launchers");
        if (!(down & WindowManagerDown))
            resources << qSL("windows");
        qCCritical(LogSystem, "There are still resources in use (%s). Check your System UI implementation. "
                              "Exiting anyhow.", resources.join(qSL(", ")).toLocal8Bit().constData());
        QCoreApplication::exit(exitCode);
    });
}

QQmlApplicationEngine *Main::qmlEngine() const
{
    return m_engine;
}

void Main::registerResources(const QStringList &resources) const
{
    for (const QString &resource: resources) {
        if (!loadResource(resource))
            qCWarning(LogSystem) << "Cannot register resource:" << resource;
    }
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

void Main::setMainQmlFile(const QString &mainQml) Q_DECL_NOEXCEPT_EXPR(false)
{
    // For some weird reason, QFile cannot cope with "qrc:/" and QUrl cannot cope with ":/",
    // so we have to translate ourselves between those two "worlds".

    m_mainQml = filePathToUrl(mainQml, QDir::currentPath());
    m_mainQmlLocalFile = urlToLocalFilePath(m_mainQml);

    if (!QFileInfo(m_mainQmlLocalFile).isFile()) {
        if (mainQml.isEmpty())
            throw Exception("No main QML file specified");

        // basically accept schemes other than file and qrc:
        if (!m_mainQmlLocalFile.isEmpty())
            throw Exception("Invalid main QML file specified: %1").arg(mainQml);
    }
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

void Main::setupRuntimesAndContainers(const QVariantMap &runtimeConfigurations, const QVariantMap &openGLConfiguration,
                                      const QVariantMap &containerConfigurations, const QStringList &containerPluginPaths,
                                      const QStringList &iconThemeSearchPaths, const QString &iconThemeName)
{
    if (m_isSingleProcessMode) {
        RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager());
        RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager(qSL("qml")));
    } else {
        RuntimeFactory::instance()->registerRuntime(new QmlInProcessRuntimeManager());
#if defined(AM_MULTI_PROCESS)
        RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager());
        RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager(qSL("qml")));
        //RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager(qSL("html")));

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
    RuntimeFactory::instance()->setSystemOpenGLConfiguration(openGLConfiguration);
    RuntimeFactory::instance()->setIconTheme(iconThemeSearchPaths, iconThemeName);
    RuntimeFactory::instance()->setSystemProperties(m_systemProperties.at(SP_ThirdParty),
                                                    m_systemProperties.at(SP_BuiltIn));

    StartupTimer::instance()->checkpoint("after runtime registration");
}

void Main::loadPackageDatabase(bool recreateDatabase, const QString &singlePackage) Q_DECL_NOEXCEPT_EXPR(false)
{
    if (!singlePackage.isEmpty()) {
        m_packageDatabase = new PackageDatabase(singlePackage);
    } else {
        m_packageDatabase = new PackageDatabase(m_builtinAppsManifestDirs, m_installationDir);
        if (!recreateDatabase)
            m_packageDatabase->enableLoadFromCache();
        m_packageDatabase->enableSaveToCache();
    }
    m_packageDatabase->parse();

    const QVector<PackageInfo *> allPackages =
            m_packageDatabase->builtInPackages()
            + m_packageDatabase->installedPackages();

    for (auto package : allPackages) {
        // check that the runtimes are supported in this instance of the AM
        auto apps = package->applications();

        for (const auto app : apps) {
            if (!RuntimeFactory::instance()->manager(app->runtimeName()))
                qCWarning(LogSystem) << "Application" << app->id() << "uses an unknown runtime:" << app->runtimeName();
        }
    }

    StartupTimer::instance()->checkpoint("after package database loading");
}

void Main::setupIntents(int disambiguationTimeout, int startApplicationTimeout,
                        int replyFromApplicationTimeout, int replyFromSystemTimeout) Q_DECL_NOEXCEPT_EXPR(false)
{
    m_intentServer = IntentAMImplementation::createIntentServerAndClientInstance(m_packageManager,
                                                                                 disambiguationTimeout,
                                                                                 startApplicationTimeout,
                                                                                 replyFromApplicationTimeout,
                                                                                 replyFromSystemTimeout);
    StartupTimer::instance()->checkpoint("after IntentServer instantiation");
}

void Main::setupSingletons(const QList<QPair<QString, QString>> &containerSelectionConfiguration,
                           int quickLaunchRuntimesPerContainer,
                           qreal quickLaunchIdleLoad) Q_DECL_NOEXCEPT_EXPR(false)
{
    m_packageManager = PackageManager::createInstance(m_packageDatabase, m_documentDir);
    m_applicationManager = ApplicationManager::createInstance(m_isSingleProcessMode);

    connect(&m_packageManager->internalSignals, &PackageManagerInternalSignals::registerApplication,
            m_applicationManager, [this](ApplicationInfo *applicationInfo, Package *package) {
        m_applicationManager->addApplication(applicationInfo, package);
        qCDebug(LogSystem).nospace().noquote() << " ++ application: " << applicationInfo->id() << " [package: " << package->id() << "]";
    });
    connect(&m_packageManager->internalSignals, &PackageManagerInternalSignals::unregisterApplication,
            m_applicationManager, [this](ApplicationInfo *applicationInfo, Package *package) {
        m_applicationManager->removeApplication(applicationInfo, package);
        qCDebug(LogSystem).nospace().noquote() << " -- application: " << applicationInfo->id() << " [package: " << package->id() << "]";
    });

    if (m_noSecurity)
        m_applicationManager->setSecurityChecksEnabled(false);

    m_applicationManager->setSystemProperties(m_systemProperties.at(SP_SystemUi));
    m_applicationManager->setContainerSelectionConfiguration(containerSelectionConfiguration);

    StartupTimer::instance()->checkpoint("after ApplicationManager instantiation");

    m_notificationManager = NotificationManager::createInstance();
    StartupTimer::instance()->checkpoint("after NotificationManager instantiation");

    m_applicationIPCManager = ApplicationIPCManager::createInstance();
    connect(&m_applicationManager->internalSignals, &ApplicationManagerInternalSignals::newRuntimeCreated,
            m_applicationIPCManager, &ApplicationIPCManager::attachToRuntime);
    StartupTimer::instance()->checkpoint("after ApplicationIPCManager instantiation");

    if (quickLaunchRuntimesPerContainer > 0) {
        m_quickLauncher = QuickLauncher::createInstance(quickLaunchRuntimesPerContainer, quickLaunchIdleLoad);
        StartupTimer::instance()->checkpoint("after quick-launcher setup");
    } else {
        qCDebug(LogSystem) << "Not setting up the quick-launch pool (runtimesPerContainer is 0)";
    }
}

void Main::setupInstaller(const QStringList &caCertificatePaths,
                          const std::function<bool(uint *, uint *, uint *)> &userIdSeparation) Q_DECL_NOEXCEPT_EXPR(false)
{
#if !defined(AM_DISABLE_INSTALLER)
    if (Q_UNLIKELY(!PackageUtilities::checkCorrectLocale())) {
        // we should really throw here, but so many embedded systems are badly set up
        qCWarning(LogDeployment) << "The appman installer needs a UTF-8 locale to work correctly: "
                                    "even automatically switching to C.UTF-8 or en_US.UTF-8 failed.";
    }

    // we only output these deployment warnings, if we are on embedded, applicationUserIdSeparation
    // is enabled and either...
    if (isRunningOnEmbedded() && userIdSeparation) {
        // ... the user forgot to call Sudo::forkServer() quite early in main()
        if (Q_UNLIKELY(!SudoClient::instance())) {
            qCWarning(LogDeployment) << "In order for the package-manager to support \"applicationUserIdSeparation\", "
                                        "please call Sudo::forkServer() as early as possible in main()";
        // ... or the user called Sudo::forkServer() explicitly AND that called failed.
        } else if (Q_UNLIKELY(SudoClient::instance()->isFallbackImplementation())) {
            qCWarning(LogDeployment) << "In order for the package-manager to support \"applicationUserIdSeparation\", "
                                        "please run as root either via sudo or SUID (preferred)";
        }
    }

    if (Q_UNLIKELY(hardwareId().isEmpty()))
        throw Exception("the installer is enabled, but the device-id is empty");

    if (Q_UNLIKELY(!m_installationDir.isEmpty() && !QDir::root().mkpath(m_installationDir)))
        throw Exception("could not create package installation directory: \'%1\'").arg(m_installationDir);
    if (Q_UNLIKELY(!m_documentDir.isEmpty() && !QDir::root().mkpath(m_documentDir)))
        throw Exception("could not create document directory for packages: \'%1\'").arg(m_documentDir);

    StartupTimer::instance()->checkpoint("after installer setup checks");

    m_applicationInstaller = ApplicationInstaller::createInstance(m_packageManager);

    if (m_developmentMode)
        m_packageManager->setDevelopmentMode(true);

    if (m_noSecurity) {
        m_packageManager->setAllowInstallationOfUnsignedPackages(true);
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
        m_packageManager->setCACertificates(caCertificateList);
    }

    uint minUserId, maxUserId, commonGroupId;
    if (userIdSeparation && userIdSeparation(&minUserId, &maxUserId, &commonGroupId)) {
#  if defined(Q_OS_LINUX)
        if (!m_packageManager->enableApplicationUserIdSeparation(minUserId, maxUserId, commonGroupId))
            throw Exception("could not enable application user-id separation in the installer.");
#  else
        qCCritical(LogSystem) << "WARNING: application user-id separation requested, but not possible on this platform.";
#  endif // Q_OS_LINUX
    }

    m_packageManager->enableInstaller();

    //TODO: this could be delayed, but needs to have a lock on the app-db in this case
    m_packageManager->cleanupBrokenInstallations();

    StartupTimer::instance()->checkpoint("after installer setup");
#else
    Q_UNUSED(caCertificatePaths)
    Q_UNUSED(userIdSeparation)
#endif // AM_DISABLE_INSTALLER
}

void Main::registerPackages()
{
    m_packageManager->registerPackages();
    StartupTimer::instance()->checkpoint("after package registration");
}

void Main::setupQmlEngine(const QStringList &importPaths, const QString &quickControlsStyle)
{
    if (!quickControlsStyle.isEmpty())
        qputenv("QT_QUICK_CONTROLS_STYLE", quickControlsStyle.toLocal8Bit());

    qmlRegisterType<QmlInProcessNotification>("QtApplicationManager", 2, 0, "Notification");
    qmlRegisterType<QmlInProcessApplicationInterfaceExtension>("QtApplicationManager.Application", 2, 0, "ApplicationInterfaceExtension");

#if !defined(AM_HEADLESS)
    qmlRegisterType<QmlInProcessApplicationManagerWindow>("QtApplicationManager.Application", 2, 0, "ApplicationManagerWindow");
#endif

    // monitor-lib
    qmlRegisterType<CpuStatus>("QtApplicationManager", 2, 0, "CpuStatus");
#if !defined(AM_HEADLESS)
    qmlRegisterType<WindowFrameTimer>("QtApplicationManager", 2, 0, "FrameTimer");
    qmlRegisterType<GpuStatus>("QtApplicationManager", 2, 0, "GpuStatus");
#endif
    qmlRegisterType<IoStatus>("QtApplicationManager", 2, 0, "IoStatus");
    qmlRegisterType<MemoryStatus>("QtApplicationManager", 2, 0, "MemoryStatus");
    qmlRegisterType<MonitorModel>("QtApplicationManager", 2, 0, "MonitorModel");
    qmlRegisterType<ProcessStatus>("QtApplicationManager.SystemUI", 2, 0, "ProcessStatus");

    StartupTimer::instance()->checkpoint("after QML registrations");

    m_engine = new QQmlApplicationEngine(this);
    disconnect(m_engine, &QQmlEngine::quit, qApp, nullptr);
    disconnect(m_engine, &QQmlEngine::exit, qApp, nullptr);
    connect(m_engine, &QQmlEngine::quit, this, [this]() { shutDown(); });
    connect(m_engine, &QQmlEngine::exit, this, [this](int retCode) { shutDown(retCode); });
    CrashHandler::setQmlEngine(m_engine);
    new QmlLogger(m_engine);
    m_engine->setOutputWarningsToStandardError(false);
    m_engine->setImportPathList(m_engine->importPathList() + importPaths);
    m_engine->rootContext()->setContextProperty(qSL("StartupTimer"), StartupTimer::instance());

    StartupTimer::instance()->checkpoint("after QML engine instantiation");
}

void Main::setupWindowTitle(const QString &title, const QString &iconPath)
{
#if defined(AM_HEADLESS)
    Q_UNUSED(title)
    Q_UNUSED(iconPath)
#else
    // For development only: set an icon, so you know which window is the AM
    if (Q_UNLIKELY(!isRunningOnEmbedded())) {
        if (!iconPath.isEmpty())
            QGuiApplication::setWindowIcon(QIcon(iconPath));
        if (!title.isEmpty())
            QGuiApplication::setApplicationDisplayName(title);
    }
#endif // AM_HEADLESS
}

void Main::setupWindowManager(const QString &waylandSocketName, bool slowAnimations, bool uiWatchdog)
{
#if defined(AM_HEADLESS)
    Q_UNUSED(waylandSocketName)
    Q_UNUSED(slowAnimations)
    Q_UNUSED(uiWatchdog)
#else
    QUnifiedTimer::instance()->setSlowModeEnabled(slowAnimations);

    m_windowManager = WindowManager::createInstance(m_engine, waylandSocketName);
    m_windowManager->setSlowAnimations(slowAnimations);
    m_windowManager->enableWatchdog(!uiWatchdog);

    QObject::connect(&m_applicationManager->internalSignals, &ApplicationManagerInternalSignals::newRuntimeCreated,
                     m_windowManager, &WindowManager::setupInProcessRuntime);
    QObject::connect(m_applicationManager, &ApplicationManager::applicationWasActivated,
                     m_windowManager, &WindowManager::raiseApplicationWindow);
#endif
}

void Main::setupTouchEmulation(bool enableTouchEmulation)
{
#if defined(AM_HEADLESS)
    Q_UNUSED(enableTouchEmulation)
#else
    if (enableTouchEmulation) {
        if (TouchEmulation::isSupported()) {
            if (QTouchDevice::devices().isEmpty()) {
                TouchEmulation::createInstance();
                qCDebug(LogGraphics) << "Touch emulation is enabled: all mouse events will be "
                                        "converted to touch events.";
            } else {
                qCDebug(LogGraphics) << "Touch emulation is disabled, because at least one touch "
                                        "input device was detected";
            }
        } else {
            qCWarning(LogGraphics) << "Touch emulation cannot be enabled. Either it was disabled at "
                                      "build time or the platform does not support it.";
        }
    }
#endif
}

void Main::loadQml(bool loadDummyData) Q_DECL_NOEXCEPT_EXPR(false)
{
    for (auto iface : qAsConst(m_startupPlugins))
        iface->beforeQmlEngineLoad(m_engine);

    // protect our namespace from this point onward
    qmlProtectModule("QtApplicationManager", 1);
    qmlProtectModule("QtApplicationManager.SystemUI", 1);
    qmlProtectModule("QtApplicationManager", 2);
    qmlProtectModule("QtApplicationManager.SystemUI", 2);

    if (Q_UNLIKELY(loadDummyData)) {
        if (m_mainQmlLocalFile.isEmpty()) {
            qCDebug(LogQml) << "Not loading QML dummy data on non-local URL" << m_mainQml;
        } else {
            loadQmlDummyDataFiles(m_engine, QFileInfo(m_mainQmlLocalFile).path());
            StartupTimer::instance()->checkpoint("after loading dummy-data");
        }
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
#if defined(AM_HEADLESS)
    Q_UNUSED(showFullscreen)
#else
    setQuitOnLastWindowClosed(false);
    connect(this, &QGuiApplication::lastWindowClosed, this, [this]() { shutDown(); });

    QQuickWindow *window = nullptr;
    QObject *rootObject = m_engine->rootObjects().constFirst();

    if (!rootObject->isWindowType()) {
        QQuickItem *contentItem = qobject_cast<QQuickItem *>(rootObject);
        if (contentItem) {
            m_view = new QQuickView(m_engine, nullptr);
            m_view->setContent(m_mainQml, nullptr, rootObject);
            m_view->setVisible(contentItem->isVisible());
            connect(contentItem, &QQuickItem::visibleChanged, this, [this, contentItem]() {
                m_view->setVisible(contentItem->isVisible());
            });
            window = m_view;
        }
    } else {
        window = qobject_cast<QQuickWindow *>(rootObject);
        if (!m_engine->incubationController())
            m_engine->setIncubationController(window->incubationController());
    }

    for (auto iface : qAsConst(m_startupPlugins))
        iface->beforeWindowShow(window);

    if (!window) {
        const QWindowList windowList = allWindows();
        for (QWindow *w : windowList) {
            if (w->isVisible()) {
                window = qobject_cast<QQuickWindow*>(w);
                break;
            }
        }
    }

    if (window) {
        StartupTimer::instance()->checkpoint("after Window instantiation/setup");

        static QMetaObject::Connection conn = QObject::connect(window, &QQuickWindow::frameSwapped, this, []() {
            // this is a queued signal, so there may be still one in the queue after calling disconnect()
            if (conn) {
#  if defined(Q_CC_MSVC)
                qApp->disconnect(conn); // MSVC cannot distinguish between static and non-static overloads in lambdas
#  else
                QObject::disconnect(conn);
#  endif
                auto st = StartupTimer::instance();
                st->checkFirstFrame();
                st->createAutomaticReport(qSL("System UI"));
            }
        });

        // Main window will always be shown, neglecting visible property for backwards compatibility
        if (Q_LIKELY(showFullscreen))
            window->showFullScreen();
        else
            window->show();

        // now check the surface format, in case we had requested a specific GL version/profile
        checkOpenGLFormat("main window", window->format());

        for (auto iface : qAsConst(m_startupPlugins))
            iface->afterWindowShow(window);

        StartupTimer::instance()->checkpoint("after window show");
    } else {
        static QMetaObject::Connection conn =
            connect(this, &QGuiApplication::focusWindowChanged, this, [this] (QWindow *win) {
            if (conn) {
                QObject::disconnect(conn);
                checkOpenGLFormat("first window", win->format());
            }
        });
        StartupTimer::instance()->createAutomaticReport(qSL("System UI"));
    }
#endif
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


#if defined(QT_DBUS_LIB) && !defined(AM_DISABLE_EXTERNAL_DBUS_INTERFACES)

void Main::registerDBusObject(QDBusAbstractAdaptor *adaptor, QString dbusName, const char *serviceName, const char *interfaceName, const char *path) Q_DECL_NOEXCEPT_EXPR(false)
{
    QString dbusAddress;
    QDBusConnection conn((QString()));

    if (dbusName.isEmpty()) {
        return;
    } else if (dbusName == qL1S("system")) {
        dbusAddress = QString::fromLocal8Bit(qgetenv("DBUS_SYSTEM_BUS_ADDRESS"));
#  if defined(Q_OS_LINUX)
        if (dbusAddress.isEmpty())
            dbusAddress = qL1S("unix:path=/var/run/dbus/system_bus_socket");
#  endif
        conn = QDBusConnection::systemBus();
    } else if (dbusName == qL1S("session")) {
        dbusAddress = QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS"));
        conn = QDBusConnection::sessionBus();
    } else if (dbusName == qL1S("auto")) {
        dbusAddress = QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS"));
        conn = QDBusConnection::sessionBus();
        if (!conn.isConnected())
            return;
        dbusName = qL1S("session");
    } else {
        dbusAddress = dbusName;
        conn = QDBusConnection::connectToBus(dbusAddress, qSL("custom"));
    }

    if (!conn.isConnected()) {
        throw Exception("could not connect to D-Bus (%1): %2")
                .arg(dbusAddress.isEmpty() ? dbusName : dbusAddress).arg(conn.lastError().message());
    }

    if (adaptor->parent() && adaptor->parent()->parent()) {
        // we need this information later on to tell apps where services are listening
        adaptor->parent()->parent()->setProperty("_am_dbus_name", dbusName);
        adaptor->parent()->parent()->setProperty("_am_dbus_address", dbusAddress);
    }

    if (!conn.registerObject(qL1S(path), adaptor->parent(), QDBusConnection::ExportAdaptors)) {
        throw Exception("could not register object %1 on D-Bus (%2): %3")
                .arg(qL1S(path)).arg(dbusName).arg(conn.lastError().message());
    }

    if (!conn.registerService(qL1S(serviceName))) {
        throw Exception("could not register service %1 on D-Bus (%2): %3")
                .arg(qL1S(serviceName)).arg(dbusName).arg(conn.lastError().message());
    }

    qCDebug(LogSystem).nospace().noquote() << " * " << serviceName << path << " [on bus: " << dbusName << "]";

    if (QByteArray::fromRawData(interfaceName, int(qstrlen(interfaceName))).startsWith("io.qt.")) {
        // Write the bus address of the interface to a file in /tmp. This is needed for the
        // controller tool, which does not even have a session bus, when started via ssh.

        QFile f(QDir::temp().absoluteFilePath(qL1S(interfaceName) + qSL(".dbus")));
        QByteArray dbusUtf8 = dbusAddress.isEmpty() ? dbusName.toUtf8() : dbusAddress.toUtf8();
        if (!f.open(QFile::WriteOnly | QFile::Truncate) || (f.write(dbusUtf8) != dbusUtf8.size()))
            throw Exception(f, "Could not write D-Bus address of interface %1").arg(qL1S(interfaceName));

        static QStringList filesToDelete;
        if (filesToDelete.isEmpty())
            atexit([]() { for (const QString &ftd : qAsConst(filesToDelete)) QFile::remove(ftd); });
        filesToDelete << f.fileName();
    }
}

#endif // defined(QT_DBUS_LIB) && !defined(AM_DISABLE_EXTERNAL_DBUS_INTERFACES)


void Main::setupDBus(const std::function<QString(const char *)> &busForInterface,
                     const std::function<QVariantMap(const char *)> &policyForInterface)
{
#if defined(QT_DBUS_LIB) && !defined(AM_DISABLE_EXTERNAL_DBUS_INTERFACES)
    registerDBusTypes();

    // <0> AbstractDBusContextAdaptor instance
    // <1> D-Bus name (extracted from callback function busForInterface)
    // <2> D-Bus service
    // <3> D-Bus path
    // <4> Interface name (extracted from Q_CLASSINFO below)

    std::vector<std::tuple<AbstractDBusContextAdaptor *, QString, const char *, const char *, const char *>> ifaces;

    auto addInterface = [&ifaces, &busForInterface](AbstractDBusContextAdaptor *adaptor, const char *service, const char *path) {
        int idx = adaptor->parent()->metaObject()->indexOfClassInfo("D-Bus Interface");
        if (idx < 0) {
            throw Exception("Could not get class-info \"D-Bus Interface\" for D-Bus adapter %1")
                    .arg(qL1S(adaptor->parent()->metaObject()->className()));
        }
        const char *interfaceName = adaptor->parent()->metaObject()->classInfo(idx).value();
        ifaces.emplace_back(adaptor, busForInterface(interfaceName), service, path, interfaceName);
    };

#  if !defined(AM_DISABLE_INSTALLER)
    if (m_packageManager) {
        addInterface(new PackageManagerDBusContextAdaptor(m_packageManager),
                     "io.qt.ApplicationManager", "/PackageManager");
    }
#  endif
#  if !defined(AM_HEADLESS)
    addInterface(new WindowManagerDBusContextAdaptor(m_windowManager),
                 "io.qt.ApplicationManager", "/WindowManager");
    addInterface(new NotificationManagerDBusContextAdaptor(m_notificationManager),
                 "org.freedesktop.Notifications", "/org/freedesktop/Notifications");
#  endif
    addInterface(new ApplicationManagerDBusContextAdaptor(m_applicationManager),
                 "io.qt.ApplicationManager", "/ApplicationManager");

    bool autoOnly = true;
    bool noneOnly = true;

    // check if all interfaces are on the "auto" bus and replace the "none" bus with nullptr
    for (auto &&iface : ifaces) {
        QString dbusName = std::get<1>(iface);
        if (dbusName != qSL("auto"))
            autoOnly = false;
        if (dbusName == qSL("none"))
            std::get<1>(iface).clear();
        else
            noneOnly = false;
    }

    // start a private dbus-daemon session instance if all interfaces are set to "auto"
    if (Q_UNLIKELY(autoOnly)) {
        try {
            DBusDaemonProcess::start();
            StartupTimer::instance()->checkpoint("after starting session D-Bus");
        } catch (const Exception &e) {
            qCWarning(LogSystem) << "Disabling external D-Bus interfaces:" << e.what();
            for (auto &&iface : ifaces)
                std::get<1>(iface).clear();
            noneOnly = true;
        }
    }

    if (!noneOnly) {
        try {
            qCDebug(LogSystem) << "Registering D-Bus services:";

            for (auto &&iface : ifaces) {
                AbstractDBusContextAdaptor *dbusAdaptor = std::get<0>(iface);
                QString &dbusName = std::get<1>(iface);
                const char *interfaceName = std::get<4>(iface);

                if (dbusName.isEmpty())
                    continue;

                registerDBusObject(dbusAdaptor->generatedAdaptor(), dbusName,
                                   std::get<2>(iface),interfaceName, std::get<3>(iface));
                if (!DBusPolicy::add(dbusAdaptor->generatedAdaptor(), policyForInterface(interfaceName)))
                    throw Exception(Error::DBus, "could not set DBus policy for %1").arg(qL1S(interfaceName));
            }
        } catch (const std::exception &e) {
            qCCritical(LogSystem) << "ERROR:" << e.what();
            qApp->exit(2);
        }
    }
#else
    Q_UNUSED(busForInterface)
    Q_UNUSED(policyForInterface)
#endif // defined(QT_DBUS_LIB) && !defined(AM_DISABLE_EXTERNAL_DBUS_INTERFACES)
}

QString Main::hardwareId() const
{
    static QString hardwareId;
    if (hardwareId.isEmpty()) {
#if defined(AM_HARDWARE_ID)
        hardwareId = QString::fromLocal8Bit(AM_HARDWARE_ID);
#elif defined(AM_HARDWARE_ID_FROM_FILE)
        QFile f(QString::fromLocal8Bit(AM_HARDWARE_ID_FROM_FILE));
        if (f.open(QFile::ReadOnly))
            hardwareId = QString::fromLocal8Bit(f.readAll().trimmed());
#else
        QVector<QNetworkInterface> candidateIfaces;
        for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
            if (iface.isValid()
                    && !(iface.flags() & (QNetworkInterface::IsPointToPoint | QNetworkInterface::IsLoopBack))
                    && iface.type() > QNetworkInterface::Virtual
                    && !iface.hardwareAddress().isEmpty()) {
                candidateIfaces << iface;
            }
        }
        if (!candidateIfaces.isEmpty()) {
            std::sort(candidateIfaces.begin(), candidateIfaces.end(), [](const QNetworkInterface &first, const QNetworkInterface &second) {
                return first.name().compare(second.name()) < 0;
            });
            hardwareId = candidateIfaces.constFirst().hardwareAddress().replace(qL1C(':'), qL1S("-"));
        }
#endif
    }
    return hardwareId;
}

QT_END_NAMESPACE_AM

#include "moc_main.cpp"
