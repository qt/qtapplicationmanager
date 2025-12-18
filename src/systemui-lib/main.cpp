// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:execute-external-code

#include <iostream>
#include <memory>
#include <cstdlib>
#include <qglobal.h>

#include "qtappmancommon-config_p.h"

#if defined(QT_DBUS_LIB)
#  include <QDBusConnection>
#  include <QDBusAbstractAdaptor>
#  include <QDBusServer>
#  include "dbuspolicy.h"
#  include "dbuscontextadaptor.h"
#  include "applicationmanager_adaptor.h"
#  include "packagemanager_adaptor.h"
#  include "windowmanager_adaptor.h"
#  include "notifications_adaptor.h"
#endif

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
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QInputDevice>
#include <QLocalServer>
#include <QLibraryInfo>
#include <QStandardPaths>
#include <QLockFile>
#if defined(Q_OS_LINUX)
#  include <sys/file.h>
#  include <sys/auxv.h>
#endif

#include "logging.h"
#include "main.h"
#include "configuration.h"
#include "applicationmanager.h"
#include "packagemanager.h"
#include "packagedatabase.h"
#include "installationreport.h"
#include "yamlpackagescanner.h"
#include "sudo.h"
#if QT_CONFIG(am_installer)
#  include "packageutilities.h"
#endif
#include "runtimefactory.h"
#include "containerfactory.h"
#include "globalruntimeconfiguration.h"
#include "quicklauncher.h"
#if QT_CONFIG(am_multi_process)
#  include "processcontainer.h"
#  include "nativeruntime.h"
#endif
#include "plugincontainer.h"
#include "notification.h"
#include "notificationmanager.h"
#include "qmlinprocruntime.h"
#include "qml-utilities.h"
#include "dbus-utilities.h"
#include "intentserver.h"
#include "intentaminterface.h"
#include "windowmanager.h"
#include "applicationmanagerwindow.h"
#include "configuration.h"
#include "utilities.h"
#include "exception.h"
#include "crashhandler.h"
#include "qmllogger.h"
#include "startuptimer.h"
#include "unixsignalhandler.h"
#include "systemd.h"
#include "startupinterface.h"

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;


//TODO:
// AM_QML_REGISTER_TYPES(QtApplicationManager_SystemUI)
// AM_QML_REGISTER_TYPES(QtApplicationManager)
// AM_QML_REGISTER_TYPES(QtApplicationManager_Application)

QT_BEGIN_NAMESPACE_AM

static bool unexpectedShutdown = true;
bool Main::s_isRunningOnEmbedded = false;

// We need to do some things BEFORE the Q*Application constructor runs, so we're using this
// old trick to do this hooking transparently for the user of the class.
int &Main::preConstructor(int &argc, char **argv, InitFlags initFlags)
{
#if defined(Q_OS_LINUX)
    // prevent KDE/GNOME platform theme loading
    s_isRunningOnEmbedded = !qEnvironmentVariableIsSet("XDG_CURRENT_DESKTOP");
    qputenv("XDG_CURRENT_DESKTOP", "QTAM");

    if (::getauxval(AT_SECURE)) {
        qCInfo(LogSystem) << "The AT_SECURE kernel flag is set on this process "
                             "(see 'man getauxval' for possible implications)";
    }
#endif

    if (initFlags & InitFlag::InitializeLogging) {
        Logging::initialize(argc, argv);
        StartupTimer::instance()->checkpoint("after logging initialization");
    }
    if (initFlags & InitFlag::ForkSudoServer) {
        Sudo::forkServer(Sudo::DropPrivilegesPermanently);
        StartupTimer::instance()->checkpoint("after sudo server fork");
    } else {
        Sudo::fallbackServer();
    }
    return argc;
}

Main::Main(int &argc, char **argv, InitFlags initFlags)
    : MainBase(SharedMain::preConstructor(Main::preConstructor(argc, argv, initFlags)), argv)
    , SharedMain()
{
    static bool once = false;
    if (!once) {
        once = true;

        UnixSignalHandler::instance()->install(UnixSignalHandler::ForwardedToEventLoopHandler,
                                               { SIGINT, SIGTERM }, [](int sig) {
            UnixSignalHandler::instance()->resetToDefault(sig);
            if (auto *main = qobject_cast<Main *>(QCoreApplication::instance()))
                main->shutDown((sig == SIGINT) ? "Ctrl+C" : "SIGTERM");
        });
    }
    StartupTimer::instance()->checkpoint("after application constructor");
}

Main::~Main()
{
    delete m_engine;

    delete m_intentServer;
    delete m_notificationManager;
    delete m_windowManager;
    delete m_view;
    delete m_applicationManager;
    delete m_packageManager;
    delete m_quickLauncher;

    delete RuntimeFactory::instance();
    delete ContainerFactory::instance();
    delete StartupTimer::instance();

#if defined(QT_DBUS_LIB)
    delete DBusPolicy::instance();
#endif
}

/*! \internal
    The caller has to make sure that cfg will be available even after this function returns:
    we will access the cfg object from delayed init functions via lambdas!
*/
void Main::setup(const Configuration *cfg) noexcept(false)
{
    StartupTimer::instance()->checkpoint("after configuration parsing");

    static bool once = false;
    if (!once) {
        once = true;
        atexit([]() {    // registered after configuration parsing, because some options like '-h' might exit
            if (unexpectedShutdown)
                std::cerr << "ERROR: Some code outside the Qt ApplicationManager called exit()" << std::endl;
            unexpectedShutdown = true;
        });
    }

    qCInfo(LogSystem) << "Development mode:" <<
        ((cfg->yaml.flags.developmentMode == ConfigurationData::Flags::DevelopmentMode::Application)
             ? "enabled (application mode)"
             : ((cfg->yaml.flags.developmentMode == ConfigurationData::Flags::DevelopmentMode::System)
                    ? "enabled (system mode)" : "disabled"));

    CrashHandler::setCrashActionConfiguration(cfg->yaml.crashAction.printBacktrace,
                                              cfg->yaml.crashAction.printQmlStack,
                                              int(cfg->yaml.crashAction.waitForGdbAttach.count()),
                                              cfg->yaml.crashAction.dumpCore,
                                              cfg->yaml.crashAction.dumpCoreOnWatchdogKill,
                                              cfg->yaml.crashAction.stackFramesToIgnore.onCrash,
                                              cfg->yaml.crashAction.stackFramesToIgnore.onException);
    setupQmlDebugging(cfg->qmlDebugging());
    if (Logging::isDltAvailable()) {
        if (!cfg->yaml.logging.dlt.id.isEmpty() || !cfg->yaml.logging.dlt.description.isEmpty())
            Logging::setSystemUiDltId(cfg->yaml.logging.dlt.id.toLocal8Bit(),
                                      cfg->yaml.logging.dlt.description.toLocal8Bit());
        Logging::setDltLongMessageBehavior(cfg->yaml.logging.dlt.longMessageBehavior);
        Logging::registerUnregisteredDltContexts();
    }
    setupLogging(cfg->verbose(), cfg->yaml.logging.rules, cfg->yaml.logging.messagePattern,
                 cfg->yaml.logging.useAMConsoleLogger);

    if (!cfg->isWatchdogDisabled())
        setupWatchdog(cfg->yaml.watchdog);

    registerResources(cfg->yaml.ui.resources);

    setupOpenGL(cfg->yaml.ui.opengl);
    setupIconTheme(cfg->yaml.ui.iconThemeSearchPaths, cfg->yaml.ui.iconThemeName);

    loadStartupPlugins(cfg->yaml.plugins.startup);
    parseSystemProperties(cfg->yaml.systemProperties);

    setMainQmlFile(cfg->yaml.ui.mainQml);
    setupSingleOrMultiProcess(cfg);
    setupRuntimesAndContainers(cfg);

    loadPackageDatabase(cfg);

    setupSingletons(cfg);
    setupQuickLauncher(cfg);
    setupIntents(cfg);

    registerPackages();

    if (cfg->yaml.applications.installationDir.isEmpty())
        StartupTimer::instance()->checkpoint("skipping installer");
    else
        setupInstaller(cfg);

    setLibraryPaths(libraryPaths() + cfg->yaml.ui.pluginPaths);
    setupQmlEngine(cfg->yaml.ui.importPaths, cfg->yaml.ui.style);

    // For development only: set an icon, so you know which window is the AM
    if (!isRunningOnEmbedded() && !cfg->yaml.ui.windowIcon.isEmpty())
        QGuiApplication::setWindowIcon(QIcon(cfg->yaml.ui.windowIcon));

    setupWindowManager(cfg);
    setupDBus(cfg);

    createInstanceInfoFile(cfg->yaml.instanceId);

    m_showFullscreen = cfg->yaml.ui.fullscreen;
    m_shutdownTimeout = cfg->yaml.shutdownTimeout;
}

bool Main::isSingleProcessMode() const
{
    return m_isSingleProcessMode;
}

bool Main::isRunningOnEmbedded() const
{
    return s_isRunningOnEmbedded;
}

void Main::shutDown(const char *shutdownReason, int exitCode)
{
    if (m_shutdownStarted) // avoid recursion
        return;

    enum {
        ApplicationManagerDown = 0x01,
        QuickLauncherDown      = 0x02,
        WindowManagerDown      = 0x04,

        AllDown                = 0x07,
    };

    m_shutdownStarted = true;
    m_shutdownStage = AllDown;
    m_shutdownExitCode = exitCode;
    m_shutdownReason = shutdownReason;

    Systemd::instance()->notify(u"STOPPING=1"_s);

    auto finalShutdown = [this](bool force) {
        unexpectedShutdown = false;
        if (m_shutdownReason && (qEnvironmentVariableIntValue("QT_QTESTLIB_RUNNING") != 1))
            qCInfo(LogSystem) << "Shutting down due to" << m_shutdownReason;
        if (force)
            ::exit(m_shutdownExitCode);
        else
            QCoreApplication::exit(m_shutdownExitCode);
    };

    auto checkNextDown = [this, finalShutdown](int nextDown) {
        if (!(m_shutdownStage & nextDown)) {
            m_shutdownStage |= nextDown;
            if (m_shutdownStage == AllDown)
                finalShutdown(false);
        }
    };

    if (m_applicationManager) {
        m_shutdownStage &= ~ApplicationManagerDown;
        connect(&m_applicationManager->internalSignals, &ApplicationManagerInternalSignals::shutDownFinished,
                this, [checkNextDown]() { checkNextDown(ApplicationManagerDown); });
        m_applicationManager->shutDown();
    }
    if (m_quickLauncher) {
        m_shutdownStage &= ~QuickLauncherDown;
        connect(m_quickLauncher, &QuickLauncher::shutDownFinished,
                this, [checkNextDown]() { checkNextDown(QuickLauncherDown); });
        m_quickLauncher->shutDown();
    }
    if (m_windowManager) {
        m_shutdownStage &= ~WindowManagerDown;
        connect(&m_windowManager->internalSignals, &WindowManagerInternalSignals::shutDownFinished,
                this, [checkNextDown]() { checkNextDown(WindowManagerDown); });
        m_windowManager->shutDown();
    }

    if (m_shutdownStage == AllDown) {
        QTimer::singleShot(0, this, [finalShutdown]() { finalShutdown(false); });
    } else {
        QTimer::singleShot(m_shutdownTimeout, this, [this, finalShutdown] {
            QStringList resources;
            if (!(m_shutdownStage & ApplicationManagerDown))
                resources << u"runtimes"_s;
            if (!(m_shutdownStage & QuickLauncherDown))
                resources << u"quick-launchers"_s;
            if (!(m_shutdownStage & WindowManagerDown))
                resources << u"windows"_s;
            qCCritical(LogSystem, "There are still resources in use (%s). Check your System UI implementation. "
                                  "Exiting regardless.", qPrintable(resources.join(u", "_s)));
            finalShutdown(true);
        });
    }
}

QQmlApplicationEngine *Main::qmlEngine() const
{
    return m_engine;
}

int Main::exec()
{
    return MainBase::exec();
}

void Main::registerResources(const QStringList &resources) const
{
    if (resources.isEmpty())
        return;
    for (const QString &resource: resources) {
        try {
            loadResource(resource);
        } catch (const Exception &e) {
            qCWarning(LogSystem).noquote() << e.errorString();
        }
    }
    StartupTimer::instance()->checkpoint("after resource registration");
}

void Main::loadStartupPlugins(const QStringList &startupPluginPaths) noexcept(false)
{
    QStringList systemStartupPluginPaths;
    const QDir systemStartupPluginDir(QLibraryInfo::path(QLibraryInfo::PluginsPath) + QDir::separator() + u"appman_startup"_s);
    const auto allPluginNames = systemStartupPluginDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &pluginName : allPluginNames) {
        const QString filePath = systemStartupPluginDir.absoluteFilePath(pluginName);
        if (!QLibrary::isLibrary(filePath))
            continue;
        systemStartupPluginPaths += filePath;
    }

    m_startupPlugins = loadPlugins<StartupInterface>("startup", systemStartupPluginPaths + startupPluginPaths);
    StartupTimer::instance()->checkpoint("after startup-plugin load");
}

void Main::parseSystemProperties(const QVariantMap &rawSystemProperties)
{
    m_systemProperties.resize(SP_SystemUi + 1);
    QVariantMap rawMap = rawSystemProperties;

    m_systemProperties[SP_ThirdParty] = rawMap.value(u"public"_s).toMap();

    m_systemProperties[SP_BuiltIn] = m_systemProperties.at(SP_ThirdParty);
    const QVariantMap pro = rawMap.value(u"protected"_s).toMap();
    for (auto it = pro.cbegin(); it != pro.cend(); ++it)
        m_systemProperties[SP_BuiltIn].insert(it.key(), it.value());

    m_systemProperties[SP_SystemUi] = m_systemProperties.at(SP_BuiltIn);
    const QVariantMap pri = rawMap.value(u"private"_s).toMap();
    for (auto it = pri.cbegin(); it != pri.cend(); ++it)
        m_systemProperties[SP_SystemUi].insert(it.key(), it.value());

    for (auto iface : std::as_const(m_startupPlugins))
        iface->initialize(m_systemProperties.at(SP_SystemUi));
}

void Main::setMainQmlFile(const QString &mainQml) noexcept(false)
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

void Main::setupSingleOrMultiProcess(const Configuration *cfg) noexcept(false)
{
    m_isSingleProcessMode = cfg->yaml.flags.forceSingleProcess;
    if (cfg->yaml.flags.forceMultiProcess && m_isSingleProcessMode)
        throw Exception("You cannot enforce multi- and single-process mode at the same time.");

    if (!QT_CONFIG(am_multi_process)) {
        if (cfg->yaml.flags.forceMultiProcess)
            throw Exception("This application manager build is not multi-process capable.");
        m_isSingleProcessMode = true;
    }
}

void Main::setupRuntimesAndContainers(const Configuration *cfg)
{
    auto &grc = GlobalRuntimeConfiguration::instance();
    grc.openGLConfiguration = cfg->yaml.ui.opengl;
    grc.watchdogDisabled = cfg->isWatchdogDisabled();
    grc.watchdogConfiguration = cfg->yaml.watchdog;
    grc.iconThemeSearchPaths = cfg->yaml.ui.iconThemeSearchPaths;
    grc.iconThemeName = cfg->yaml.ui.iconThemeName;
    grc.systemPropertiesForBuiltInApps = m_systemProperties.at(SP_BuiltIn);
    grc.systemPropertiesForThirdPartyApps= m_systemProperties.at(SP_ThirdParty);

    QVector<PluginContainerManager *> pluginContainerManagers;

    if (m_isSingleProcessMode) {
        RuntimeFactory::instance()->registerRuntime(new QmlInProcRuntimeManager());
        RuntimeFactory::instance()->registerRuntime(new QmlInProcRuntimeManager(u"qml"_s));
    } else {
        RuntimeFactory::instance()->registerRuntime(new QmlInProcRuntimeManager());
#if QT_CONFIG(am_multi_process)
        RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager());
        RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager(u"qml"_s));

        for (const QString &runtimeId : cfg->yaml.runtimes.additionalLaunchers)
            RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager(runtimeId));

        ContainerFactory::instance()->registerContainer(new ProcessContainerManager());
#else
        if (!cfg->yaml.runtimes.additionalLaunchers.isEmpty())
            qCWarning(LogSystem) << "Addtional runtime launchers are ignored in single-process mode";
#endif
        QStringList systemContainerPluginPaths;
        const QDir systemContainerPluginDir(QLibraryInfo::path(QLibraryInfo::PluginsPath) + QDir::separator() + u"appman_container"_s);
        const auto allPluginNames = systemContainerPluginDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        for (const auto &pluginName : allPluginNames) {
            const QString filePath = systemContainerPluginDir.absoluteFilePath(pluginName);
            if (!QLibrary::isLibrary(filePath))
                continue;
            systemContainerPluginPaths += filePath;
        }

        QSet<QString> containersWithConfiguration(cfg->yaml.containers.configurations.keyBegin(),
                                                  cfg->yaml.containers.configurations.keyEnd());

        auto containerPlugins = loadPlugins<ContainerManagerInterface>("container", systemContainerPluginPaths + cfg->yaml.plugins.container);
        pluginContainerManagers.reserve(containerPlugins.size());
        for (auto iface : std::as_const(containerPlugins)) {
            if (!containersWithConfiguration.contains(iface->identifier()))
                continue;
            auto pcm = new PluginContainerManager(iface);
            pluginContainerManagers << pcm;
            ContainerFactory::instance()->registerContainer(pcm);
        }
    }
    for (auto iface : std::as_const(m_startupPlugins))
        iface->afterRuntimeRegistration();

    ContainerFactory::instance()->setConfiguration(cfg->yaml.containers.configurations);

    for (auto pcm : std::as_const(pluginContainerManagers)) {
        if (!pcm->initialize()) {
            ContainerFactory::instance()->disableContainer(pcm->identifier());
            qCWarning(LogSystem).noquote() << "Disabling container plugin" << pcm->identifier() << "as it failed to initialize";
        }
    }

    RuntimeFactory::instance()->setConfiguration(cfg->yaml.runtimes.configurations);

    StartupTimer::instance()->checkpoint("after runtime registration");
}

void Main::loadPackageDatabase(const Configuration *cfg) noexcept(false)
{
    if (!cfg->singleApp().isEmpty()) {
        m_packageDatabase = new PackageDatabase(cfg->singleApp());
    } else {
        m_packageDatabase = new PackageDatabase(cfg->yaml.applications.builtinAppsManifestDir,
                                                cfg->yaml.applications.installationDir,
                                                cfg->yaml.applications.installationDirMountPoint);
        if (!cfg->clearCache() && !cfg->noCache())
            m_packageDatabase->enableLoadFromCache();
        m_packageDatabase->enableSaveToCache();
    }
    m_packageDatabase->parse();

    const QVector<PackageInfo *> allPackages =
            m_packageDatabase->builtInPackages()
            + m_packageDatabase->installedPackages();

    for (auto package : allPackages) {
        // check that the runtimes are supported in this instance of the AM
        const auto apps = package->applications();

        for (const auto app : apps) {
            if (!RuntimeFactory::instance()->manager(app->runtimeName()))
                qCWarning(LogSystem) << "Application" << app->id() << "uses an unknown runtime:" << app->runtimeName();
        }
    }

    StartupTimer::instance()->checkpoint("after package database loading");
}

void Main::setupIntents(const Configuration *cfg)
{
    m_intentServer = IntentAMImplementation::createIntentServerAndClientInstance(
        m_packageManager,
        int(cfg->yaml.intents.timeouts.disambiguation.count()),
        int(cfg->yaml.intents.timeouts.startApplication.count()),
        int(cfg->yaml.intents.timeouts.replyFromApplication.count()),
        int(cfg->yaml.intents.timeouts.replyFromSystem.count()));
    StartupTimer::instance()->checkpoint("after IntentServer instantiation");
}

void Main::setupSingletons(const Configuration *cfg) noexcept(false)
{
    m_packageManager = PackageManager::createInstance(m_packageDatabase, cfg->yaml.applications.documentDir);
    m_applicationManager = ApplicationManager::createInstance(m_isSingleProcessMode);

    if (cfg->yaml.flags.noSecurity)
        m_applicationManager->setSecurityChecksEnabled(false);

    // map the enums - they are the same, but live in different layers
    const auto pmDevMode = [](auto configDevMode) {
        switch (configDevMode) {
        default:
        case ConfigurationData::Flags::DevelopmentMode::Disabled:
            return PackageManager::DevelopmentMode::Disabled;
        case ConfigurationData::Flags::DevelopmentMode::System:
            return PackageManager::DevelopmentMode::System;
        case ConfigurationData::Flags::DevelopmentMode::Application:
            return PackageManager::DevelopmentMode::Application;
        }
    }(cfg->yaml.flags.developmentMode);
    m_packageManager->setDevelopmentMode(pmDevMode);
    m_packageManager->setAllowedInstallationURLs(cfg->yaml.installer.allowedURLs);

    m_applicationManager->setSystemProperties(m_systemProperties.at(SP_SystemUi));
    m_applicationManager->setContainerSelectionConfiguration(cfg->yaml.containers.selection);

    StartupTimer::instance()->checkpoint("after ApplicationManager instantiation");

    m_notificationManager = NotificationManager::createInstance();
    StartupTimer::instance()->checkpoint("after NotificationManager instantiation");
}

void Main::setupQuickLauncher(const Configuration *cfg)
{
    if (!cfg->yaml.quicklaunch.runtimesPerContainer.isEmpty()) {
        m_quickLauncher = QuickLauncher::createInstance(cfg->yaml.quicklaunch.runtimesPerContainer,
                                                        cfg->yaml.quicklaunch.idleLoad,
                                                        cfg->yaml.quicklaunch.failedStartLimit,
                                                        int(cfg->yaml.quicklaunch.failedStartLimitIntervalSec.count()));
        StartupTimer::instance()->checkpoint("after quick-launcher setup");
    } else {
        qCDebug(LogSystem) << "Not setting up the quick-launch pool (runtimesPerContainer is 0)";
    }
}

void Main::setupInstaller(const Configuration *cfg) noexcept(false)
{
#if QT_CONFIG(am_installer)
    // make sure the installation and document dirs are valid
    Q_ASSERT(!cfg->yaml.applications.installationDir.isEmpty());
    const auto instPath = QDir(cfg->yaml.applications.installationDir).canonicalPath();
    const auto docPath = cfg->yaml.applications.documentDir.isEmpty()
                             ? QString { } : QDir(cfg->yaml.applications.documentDir).canonicalPath();

    if (!docPath.isEmpty() && (instPath.startsWith(docPath) || docPath.startsWith(instPath)))
        throw Exception("either installationDir or documentDir cannot be a sub-directory of the other");

#  if defined(Q_OS_LINUX)
    // make sure that a typo in the config does not wipe out system directories
    static const QVector<QString> fhsPaths = {
        u"/bin/"_s,
        u"/boot/"_s,
        u"/dev/"_s,
        u"/etc/"_s,
        u"/lib/"_s,
        u"/lib64/"_s,
        u"/proc/"_s,
        u"/root/"_s,
        u"/run/"_s,
        u"/sbin/"_s,
        u"/sys/"_s,
        u"/usr/"_s,
    };

    static auto checkFhsPaths = [](const QString &path) -> bool {
        for (const auto &fhsPath : fhsPaths) {
            if (Q_UNLIKELY(path.startsWith(fhsPath)))
                return false;
        }
        return true;
    };

    if (!docPath.isEmpty() && Q_UNLIKELY(!checkFhsPaths(docPath)))
        throw Exception("To prevent accidents, documentDir cannot be below a system directory (%1)").arg(fhsPaths);
    if (Q_UNLIKELY(!checkFhsPaths(instPath)))
        throw Exception("To prevent accidents, installationDir cannot be below a system directory (%1)").arg(fhsPaths);
#  endif

    if (Q_UNLIKELY(hardwareId().isEmpty()))
        throw Exception("the installer is enabled, but the device-id is empty");

    if (!isRunningOnEmbedded()) { // this is just for convenience sake during development
        if (Q_UNLIKELY(!cfg->yaml.applications.installationDir.isEmpty() && !QDir::root().mkpath(cfg->yaml.applications.installationDir)))
            throw Exception("could not create package installation directory: \'%1\'").arg(cfg->yaml.applications.installationDir);
        if (Q_UNLIKELY(!cfg->yaml.applications.documentDir.isEmpty() && !QDir::root().mkpath(cfg->yaml.applications.documentDir)))
            throw Exception("could not create document directory for packages: \'%1\'").arg(cfg->yaml.applications.documentDir);
    }

    StartupTimer::instance()->checkpoint("after installer setup checks");

    if (isRunningOnEmbedded()) {
        // We do not want the PackageManager from cleaning up its installation directories using the
        // sudo-helper on desktop systems. A simple typo in the config file might cause an
        // accidental disk wipe when run via sudo.
        m_packageManager->setUseSudoForDirectoryRemoval(true);
    }

    if (cfg->yaml.flags.noSecurity || cfg->yaml.flags.allowUnsignedPackages)
        m_packageManager->setAllowInstallationOfUnsignedPackages(true);

    if (!cfg->yaml.flags.noSecurity) {
        m_packageManager->loadCertificates(cfg->yaml.installer.caCertificates.common,
                                           cfg->yaml.installer.caCertificates.developer,
                                           cfg->yaml.installer.caCertificates.store,
                                           cfg->yaml.installer.certificateRevocationLists);

        m_packageManager->setIssuerCertificateFingerprints(cfg->yaml.installer.issuerCertificateFingerprints.developer,
                                                           cfg->yaml.installer.issuerCertificateFingerprints.store);
    }

    m_packageManager->enableInstaller();

    // no changes possible to the PM configuration anymore after this point in non-developer builds
    m_packageManager->lockConfiguration();

    StartupTimer::instance()->checkpoint("after installer setup");
#else
    Q_UNUSED(cfg)
#endif // QT_CONFIG(am_installer)
}

void Main::registerPackages()
{
    // the installation dir might not be mounted yet, so we have to watch for the package
    // DB's signal and then register these packages later in the already running system
    if (!(m_packageDatabase->parsedPackageLocations() & PackageDatabase::Installed)) {
        connect(m_packageDatabase, &PackageDatabase::installedPackagesParsed,
                m_packageManager, [this]() {
            // we are not in main() anymore: we can't just throw
            try {
                m_packageManager->registerPackages();
            } catch (const Exception &e) {
                qCCritical(LogInstaller) << "Failed to register packages:" << e.what();
                std::abort(); // there is no qCFatal()
            }
        });
        StartupTimer::instance()->checkpoint("after package registration (delayed)");
    } else {
        m_packageManager->registerPackages();
        StartupTimer::instance()->checkpoint("after package registration");
    }
}

void Main::setupQmlEngine(const QStringList &importPaths, const QString &quickControlsStyle)
{
    if (!quickControlsStyle.isEmpty())
        qputenv("QT_QUICK_CONTROLS_STYLE", quickControlsStyle.toLocal8Bit());

    StartupTimer::instance()->checkpoint("after QML registrations");

    m_engine = new QQmlApplicationEngine(this);
    disconnect(m_engine, &QQmlEngine::quit, qApp, nullptr);
    disconnect(m_engine, &QQmlEngine::exit, qApp, nullptr);
    connect(m_engine, &QQmlEngine::quit, this, [this]() { shutDown("Qt.quit()"); });
    connect(m_engine, &QQmlEngine::exit, this, [this](int retCode) { shutDown("Qt.exit()", retCode); });
    CrashHandler::setQmlEngine(m_engine);
    new QmlLogger(m_engine);
    m_engine->setOutputWarningsToStandardError(false);
    m_engine->setImportPathList(m_engine->importPathList() + importPaths);

    StartupTimer::instance()->checkpoint("after QML engine instantiation");
}

void Main::setupWindowManager(const Configuration *cfg)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
    QUnifiedTimer::instance()->setSlowModeEnabled(cfg->slowAnimations());
#else
    QUnifiedTimer::instance()->setSpeedModifier(cfg->slowAnimations() ? slowAnimationSpeed() : 1.0f);
#endif

    auto waylandSocketName = [cfg]() -> QString {
        QString socketName = cfg->waylandSocketName(); // get the default value
        if (!socketName.isEmpty())
            return socketName;
        if (!cfg->yaml.wayland.socketName.isEmpty())
            return cfg->yaml.wayland.socketName;

#if defined(Q_OS_LINUX)
        // modelled after wl_socket_lock() in wayland_server.c
        const QString xdgDir = qEnvironmentVariable("XDG_RUNTIME_DIR") + u"/"_s;
        const QString pattern = u"qtam-wayland-%1"_s;
        const QString lockSuffix = u".lock"_s;

        for (int i = 0; i < 32; ++i) {
            socketName = pattern.arg(i);
            QFile lock(xdgDir + socketName + lockSuffix);
            if (lock.open(QIODevice::ReadWrite)) {
                if (::flock(lock.handle(), LOCK_EX | LOCK_NB) == 0) {
                    QFile socket(xdgDir + socketName);
                    if (!socket.exists() || socket.remove())
                        return socketName;
                }
            }
        }
#endif
        return { };
    };

    m_windowManager = WindowManager::createInstance(m_engine, waylandSocketName());
    m_windowManager->setAllowUnknownUiClients(cfg->yaml.flags.noSecurity || cfg->yaml.flags.allowUnknownUiClients);
    m_windowManager->setSlowAnimations(cfg->slowAnimations());
    m_windowManager->addWaylandSockets(Systemd::instance()->listenFds(
        QRegularExpression::fromWildcard(u"wayland.*"_s), true));
    if (!cfg->isWatchdogDisabled()) {
        m_windowManager->setWatchdogTimeouts(cfg->yaml.watchdog.wayland.checkInterval,
                                             cfg->yaml.watchdog.wayland.warnTimeout,
                                             cfg->yaml.watchdog.wayland.killTimeout);
    }

    QObject::connect(&m_applicationManager->internalSignals, &ApplicationManagerInternalSignals::newRuntimeCreated,
                     m_windowManager, &WindowManager::setupInProcessRuntime);
    QObject::connect(m_applicationManager, &ApplicationManager::applicationWasActivated,
                     m_windowManager, &WindowManager::raiseApplicationWindow);
}

void Main::createInstanceInfoFile(const QString &instanceId) noexcept(false)
{
    // This is needed for the appman-controller tool to talk to running appman instances.
    // (the tool does not even have a session bus, when started via ssh)

    static const QString defaultInstanceId = u"appman"_s;

    QString rtPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (rtPath.isEmpty())
        rtPath = QDir::tempPath();
    QDir rtDir(rtPath + u"/qtapplicationmanager"_s);
    if (!rtDir.mkpath(u"."_s))
        throw Exception("Could not create runtime state directory (%1) for the instance info").arg(rtDir.absolutePath());

    QString fileName;
    QString filePattern = (instanceId.isEmpty() ? defaultInstanceId : instanceId) + u"-%1";

    static std::unique_ptr<QLockFile> lockf;
    static std::unique_ptr<QFile, void (*)(QFile *)> infof(nullptr, [](QFile *f) { f->remove(); delete f; });

    for (int i = 0; i < 32; ++i) { // Wayland sockets are limited to 32 instances as well
        QString tryPattern = filePattern.arg(i);
        lockf = std::make_unique<QLockFile>(rtDir.absoluteFilePath(tryPattern + u".lock"_s));
        lockf->setStaleLockTime(0);
        if (lockf->tryLock()) {
            fileName = tryPattern;
            break; // found a free instance id
        }
    }
    if (fileName.isEmpty())
        throw Exception("Could not create a lock file for the instance info at %1").arg(rtDir.absolutePath());;

    infof.reset(new QFile(rtDir.absoluteFilePath(fileName + u".json"_s)));

    const QByteArray json = QJsonDocument::fromVariant(m_infoFileContents).toJson(QJsonDocument::Indented);
    if (!infof->open(QIODevice::WriteOnly))
        throw Exception(*infof.get(), "failed to create instance info file");
    if (infof->write(json) !=json.size())
        throw Exception(*infof.get(), "failed to write instance info file");
    infof->close();
}

void Main::loadQml() noexcept(false)
{
    for (auto iface : std::as_const(m_startupPlugins))
        iface->beforeQmlEngineLoad(m_engine);

    // protect our namespace from this point onward
    qmlProtectModule("QtApplicationManager", 1);
    qmlProtectModule("QtApplicationManager.SystemUI", 1);
    qmlProtectModule("QtApplicationManager", 2);
    qmlProtectModule("QtApplicationManager.SystemUI", 2);

    m_engine->load(m_mainQml);
    if (Q_UNLIKELY(m_engine->rootObjects().isEmpty()))
        throw Exception("Qml scene does not have a root object");

    for (auto iface : std::as_const(m_startupPlugins))
        iface->afterQmlEngineLoad(m_engine);

    StartupTimer::instance()->checkpoint("after loading main QML file");

    Systemd::instance()->notify(u"READY=1"_s);
}

void Main::showWindow()
{
    setQuitOnLastWindowClosed(false);
    connect(this, &QGuiApplication::lastWindowClosed, this, [this]() { shutDown("window closed"); });

    QQuickWindow *window = nullptr;
    QObject *rootObject = m_engine->rootObjects().constFirst();

    if (rootObject->isWindowType()) {
        window = qobject_cast<QQuickWindow *>(rootObject);
    } else {
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
    }

    for (auto iface : std::as_const(m_startupPlugins))
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
        Q_ASSERT(m_engine->incubationController());

        StartupTimer::instance()->checkpoint("after Window instantiation/setup");

        static QMetaObject::Connection conn = QObject::connect(window, &QQuickWindow::frameSwapped, this, []() {
            // this is a queued signal, so there may be still one in the queue after calling disconnect()
            if (conn) {
#if defined(Q_CC_MSVC)
                qApp->disconnect(conn); // MSVC cannot distinguish between static and non-static overloads in lambdas
#else
                QObject::disconnect(conn);
#endif
                auto st = StartupTimer::instance();
                st->checkFirstFrame();
                st->createAutomaticReport(u"System UI"_s);
            }
        });

        // Main window will always be shown, neglecting visible property for backwards compatibility
        if (Q_LIKELY(m_showFullscreen))
            window->showFullScreen();
        else
            window->setVisible(true);

        // now check the surface format, in case we had requested a specific GL version/profile
        checkOpenGLFormat("main window", window->format());

        for (auto iface : std::as_const(m_startupPlugins))
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
        StartupTimer::instance()->createAutomaticReport(u"System UI"_s);
    }
}

void Main::setupDBus(const Configuration *cfg)
{
#if defined(QT_DBUS_LIB)
    // Initialize the policy checker
    DBusPolicy::createInstance([](qint64 pid) { return ApplicationManager::instance()->identifyAllApplications(pid); },
                               [](const QString &appId) { return ApplicationManager::instance()->capabilities(appId); });

    // Don't repeat yourself: we already have the interface name in the class infos
    static auto interfaceNameForAdaptor = [](const QMetaObject *mobj) -> QString {
        const int idx = mobj->indexOfClassInfo("D-Bus Interface");
        if (idx < 0) {
            throw Exception("Could not get class-info \"D-Bus Interface\" for D-Bus adapter %1")
                .arg(mobj->className());
        }
        return QString::fromLatin1(mobj->classInfo(idx).value());
    };

    // All the available adaptors: <factory, service, path, interface, availableInDevMode>
    const std::array<std::tuple<std::function<DBusContextAdaptor *()>, QString, QString, QString, bool>, 4> adaptors { {
        {
            [this] { return DBusContextAdaptor::create<ApplicationManagerAdaptor>(m_applicationManager); },
            u"io.qt.ApplicationManager"_s,
            u"/ApplicationManager"_s,
            interfaceNameForAdaptor(&ApplicationManagerAdaptor::staticMetaObject),
            true /*devmode*/
        },
        {
            [this] { return DBusContextAdaptor::create<PackageManagerAdaptor>(m_packageManager); },
            u"io.qt.ApplicationManager"_s,
            u"/PackageManager"_s,
            interfaceNameForAdaptor(&PackageManagerAdaptor::staticMetaObject),
            true /*devmode*/
        },
        {
            [this] { return DBusContextAdaptor::create<WindowManagerAdaptor>(m_windowManager); },
            u"io.qt.ApplicationManager"_s,
            u"/WindowManager"_s,
            interfaceNameForAdaptor(&WindowManagerAdaptor::staticMetaObject),
            false /*devmode*/
        },
        {
            [this] { return DBusContextAdaptor::create<NotificationsAdaptor>(m_notificationManager); },
            u"org.freedesktop.Notifications"_s,
            u"/org/freedesktop/Notifications"_s,
            interfaceNameForAdaptor(&NotificationsAdaptor::staticMetaObject),
            false /*devmode*/
        }
    } };

    // If development mode is active, we start a P2P server and register the AM and PM singletons
    // on that bus. The adaptors here are special, as they do development mode checks
    if (cfg->yaml.flags.developmentMode != ConfigurationData::Flags::DevelopmentMode::Disabled) {
        m_p2pServer = new QDBusServer(this);
        m_p2pServer->setAnonymousAuthenticationAllowed(true);

        if (!m_p2pServer->isConnected()) {
             throw Exception("Failed to create a P2P D-Bus server for the appman-controller: %1")
                .arg(m_p2pServer->lastError().message());
        }

        auto dbusMap = m_infoFileContents.value(u"dbus"_s).toMap();
        const QString dbusAddress = u"p2p:"_s + m_p2pServer->address();

        qCDebug(LogDBus) << "Registering development mode D-Bus services:";

        for (const auto &[create, serviceName, path, interfaceName, devMode] : adaptors) {
            if (!devMode)
                continue;
            DBusContextAdaptor *contextAdaptor = create();
            auto generatedAdaptor = contextAdaptor->generatedAdaptor<QDBusAbstractAdaptor>();
            // The header for the adaptors is autogenerated, so we cannot add a C++ member variable
            generatedAdaptor->setProperty("developmentModeChecksEnabled", true);
            m_p2pAdaptors.insert(path, contextAdaptor);
            dbusMap.insert(interfaceName, dbusAddress);

            qCDebug(LogDBus).nospace().noquote() << " * " << serviceName << path;
        }

        QObject::connect(m_p2pServer, &QDBusServer::newConnection,
                         this, [this](const QDBusConnection &conn) {
            for (const auto &[path, adaptor] : std::as_const(m_p2pAdaptors).asKeyValueRange())
                adaptor->registerOnDBus(conn, path);

            auto *timer = new QTimer(m_p2pServer);
            // The connName capture is on purpose to avoid holding a reference to conn
            connect(timer, &QTimer::timeout, this, [timer, connName = conn.name()]() {
                if (!QDBusConnection(connName).isConnected()) {
                    QDBusConnection::disconnectFromPeer(connName);
                    timer->deleteLater();
                }
            });
            timer->start(500ms);
        });

        // Write the bus address to our info file for the appman-controller tool
        m_infoFileContents[u"dbus"_s] = dbusMap;
    }

    // Create all adaptors for external busses that have been explicitly requested via the config
    bool first = true;
    for (const auto &[create, serviceName, path, interfaceName, devMode] : adaptors) {
        auto iit = cfg->yaml.dbus.registrations.constFind(interfaceName);
        if (iit != cfg->yaml.dbus.registrations.cend()) {
            auto dbusName = iit->toString();

            if ((dbusName == u"auto") || (dbusName == u"p2p"))
                throw Exception("The 'auto' and 'p2p' D-Bus names are not supported anymore");

            if (dbusName.isEmpty() || (dbusName == u"none"))
                continue;

            if (first) {
                qCDebug(LogDBus) << "Registering external D-Bus services:";
                first = false;
            }

            DBusContextAdaptor *contextAdaptor = create();
            auto generatedAdaptor = contextAdaptor->generatedAdaptor<QDBusAbstractAdaptor>();

            QDBusConnection conn = [dbusName, customId = serviceName] { // C++17 capture kludge
                if (dbusName == u"system")
                    return QDBusConnection::systemBus();
                else if (dbusName == u"session")
                    return QDBusConnection::sessionBus();
                else
                    return QDBusConnection::connectToBus(dbusName, u"custom_"_s + customId);
            }();

            if (!conn.isConnected()) {
                throw Exception("could not connect to D-Bus (%1): %2")
                    .arg(dbusName).arg(conn.lastError().message());
            }

            if (!conn.registerObject(path, generatedAdaptor->parent(), QDBusConnection::ExportAdaptors)) {
                throw Exception("could not register object %1 on D-Bus (%2): %3")
                    .arg(path).arg(dbusName).arg(conn.lastError().message());
            }

            if (!conn.registerService(serviceName)) {
                throw Exception("could not register service %1 on D-Bus (%2): %3")
                    .arg(serviceName).arg(dbusName).arg(conn.lastError().message());
            }

            qCDebug(LogDBus).nospace().noquote() << " * " << serviceName << path << " [on bus: " << dbusName << "]";

            auto policy = cfg->yaml.dbus.policies.value(interfaceName).toMap();
            if (!DBusPolicy::instance()->add(generatedAdaptor, policy))
                throw Exception(Error::DBus, "could not set D-Bus policy for %1").arg(interfaceName);
        }
    }
    if (first)
        qCDebug(LogDBus) << "Not registering any external D-Bus services";

#else
    Q_UNUSED(cfg)
    Q_UNUSED(m_p2pServer)
    Q_UNUSED(m_p2pAdaptors)
#endif // defined(QT_DBUS_LIB)
}

QString Main::hardwareId() const
{
    static QString hardwareId;
    if (hardwareId.isEmpty()) {
#if defined(QT_AM_HARDWARE_ID)
        hardwareId = QString::fromLocal8Bit(QT_AM_HARDWARE_ID);
        if (hardwareId.startsWith(u'@')) {
            QFile f(hardwareId.mid(1));
            hardwareId.clear();
            if (f.open(QFile::ReadOnly))
                hardwareId = QString::fromLocal8Bit(f.readAll().trimmed());
        }
#else
        QVector<QNetworkInterface> candidateIfaces;
        const auto allIfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &iface : allIfaces) {
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
            hardwareId = candidateIfaces.constFirst().hardwareAddress().replace(u':', u'-');
        }
#endif
    }
    return hardwareId;
}

bool Main::notify(QObject *receiver, QEvent *event)
{
    const SharedMain::EventNotifyWatcher enw(receiver, event);
    return MainBase::notify(receiver, event);
}

QT_END_NAMESPACE_AM

#include "moc_main.cpp"
