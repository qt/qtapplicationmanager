// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <memory>
#include <cstdlib>
#include <qglobal.h>

#include "qtappman_common-config_p.h"

#if defined(QT_DBUS_LIB) && QT_CONFIG(am_external_dbus_interfaces)
#  include <QDBusConnection>
#  include <QDBusAbstractAdaptor>
#  include <QDBusServer>
#  include "dbusdaemon.h"
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
#include "sudo.h"
#if QT_CONFIG(am_installer)
#  include "packageutilities.h"
#endif
#include "runtimefactory.h"
#include "containerfactory.h"
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
#include "frametimer.h"
#include "gpustatus.h"

#include "configuration.h"
#include "utilities.h"
#include "exception.h"
#include "crashhandler.h"
#include "qmllogger.h"
#include "startuptimer.h"
#include "unixsignalhandler.h"

// monitor-lib
#include "cpustatus.h"
#include "iostatus.h"
#include "memorystatus.h"
#include "monitormodel.h"
#include "processstatus.h"

#include "../plugin-interfaces/startupinterface.h"

using namespace Qt::StringLiterals;


AM_QML_REGISTER_TYPES(QtApplicationManager_SystemUI)
AM_QML_REGISTER_TYPES(QtApplicationManager)
AM_QML_REGISTER_TYPES(QtApplicationManager_Application)

QT_BEGIN_NAMESPACE_AM

// We need to do some things BEFORE the Q*Application constructor runs, so we're using this
// old trick to do this hooking transparently for the user of the class.
int &Main::preConstructor(int &argc, char **argv, InitFlags initFlags)
{
    if (initFlags & InitFlag::InitializeLogging) {
        Logging::initialize(argc, argv);
        StartupTimer::instance()->checkpoint("after logging initialization");
    }
    if (initFlags & InitFlag::ForkSudoServer) {
        Sudo::forkServer(Sudo::DropPrivilegesPermanently);
        StartupTimer::instance()->checkpoint("after sudo server fork");
    }
    return argc;
}

Main::Main(int &argc, char **argv, InitFlags initFlags)
    : MainBase(SharedMain::preConstructor(Main::preConstructor(argc, argv, initFlags)), argv)
    , SharedMain()
{
    m_isRunningOnEmbedded =
#if defined(Q_OS_LINUX)
            !qEnvironmentVariableIsSet("XDG_CURRENT_DESKTOP");
#else
            false;
#endif

    // this might be needed later on by the native runtime to find a suitable qml runtime launcher
    setProperty("_am_build_dir", QString::fromLatin1(AM_BUILD_DIR));

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

#if defined(QT_DBUS_LIB) && QT_CONFIG(am_external_dbus_interfaces)
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

    // basics that are needed in multiple setup functions below
    m_noSecurity = cfg->noSecurity();
    m_developmentMode = cfg->developmentMode();
    m_builtinAppsManifestDirs = cfg->builtinAppsManifestDirs();
    m_installationDir = cfg->installationDir();
    m_installationDirMountPoint = cfg->installationDirMountPoint();
    m_documentDir = cfg->documentDir();

    CrashHandler::setCrashActionConfiguration(cfg->managerCrashAction());
    setupQmlDebugging(cfg->qmlDebugging());
    if (!cfg->dltId().isEmpty() || !cfg->dltDescription().isEmpty())
        Logging::setSystemUiDltId(cfg->dltId().toLocal8Bit(), cfg->dltDescription().toLocal8Bit());
    Logging::setDltLongMessageBehavior(cfg->dltLongMessageBehavior());
    Logging::registerUnregisteredDltContexts();
    setupLogging(cfg->verbose(), cfg->loggingRules(), cfg->messagePattern(), cfg->useAMConsoleLogger());

    registerResources(cfg->resources());

    setupOpenGL(cfg->openGLConfiguration());
    setupIconTheme(cfg->iconThemeSearchPaths(), cfg->iconThemeName());

    loadStartupPlugins(cfg->pluginFilePaths("startup"));
    parseSystemProperties(cfg->rawSystemProperties());

    setMainQmlFile(cfg->mainQmlFile());
    setupSingleOrMultiProcess(cfg->forceSingleProcess(), cfg->forceMultiProcess());
    setupRuntimesAndContainers(cfg->runtimeConfigurations(), cfg->runtimeAdditionalLaunchers(),
                               cfg->containerConfigurations(), cfg->pluginFilePaths("container"),
                               cfg->openGLConfiguration(),
                               cfg->iconThemeSearchPaths(), cfg->iconThemeName());

    loadPackageDatabase(cfg->clearCache() || cfg->noCache(), cfg->singleApp());

    setupSingletons(cfg->containerSelectionConfiguration());
    setupQuickLauncher(cfg->quickLaunchRuntimesPerContainer(), cfg->quickLaunchIdleLoad(),
                       cfg->quickLaunchFailedStartLimit(), cfg->quickLaunchFailedStartLimitIntervalSec());

    if (!cfg->disableIntents()) {
        setupIntents(cfg->intentTimeoutForDisambiguation(), cfg->intentTimeoutForStartApplication(),
                     cfg->intentTimeoutForReplyFromApplication(), cfg->intentTimeoutForReplyFromSystem());
    }

    registerPackages();

    if (m_installationDir.isEmpty() || cfg->disableInstaller())
        StartupTimer::instance()->checkpoint("skipping installer");
    else
        setupInstaller(cfg->allowUnsignedPackages(), cfg->caCertificates());

    setLibraryPaths(libraryPaths() + cfg->pluginPaths());
    setupQmlEngine(cfg->importPaths(), cfg->style());
    setupWindowTitle(QString(), cfg->windowIcon());
    setupWindowManager(cfg->waylandSocketName(), cfg->waylandExtraSockets(), cfg->slowAnimations(),
                       cfg->noUiWatchdog(), cfg->allowUnknownUiClients());

    setupDBus(std::bind(&Configuration::dbusRegistration, cfg, std::placeholders::_1),
              std::bind(&Configuration::dbusPolicy, cfg, std::placeholders::_1));
    createInstanceInfoFile(cfg->instanceId());
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
        connect(&m_applicationManager->internalSignals, &ApplicationManagerInternalSignals::shutDownFinished,
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
    if (m_windowManager) {
        connect(&m_windowManager->internalSignals, &WindowManagerInternalSignals::shutDownFinished,
                this, []() { checkShutDownFinished(WindowManagerDown); });
        m_windowManager->shutDown();
    }

    QTimer::singleShot(5000, this, [exitCode] {
        QStringList resources;
        if (!(down & ApplicationManagerDown))
            resources << u"runtimes"_s;
        if (!(down & QuickLauncherDown))
            resources << u"quick-launchers"_s;
        if (!(down & WindowManagerDown))
            resources << u"windows"_s;
        qCCritical(LogSystem, "There are still resources in use (%s). Check your System UI implementation. "
                              "Exiting anyhow.", resources.join(u", "_s).toLocal8Bit().constData());
        QCoreApplication::exit(exitCode);
    });
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

void Main::setupSingleOrMultiProcess(bool forceSingleProcess, bool forceMultiProcess) noexcept(false)
{
    m_isSingleProcessMode = forceSingleProcess;
    if (forceMultiProcess && m_isSingleProcessMode)
        throw Exception("You cannot enforce multi- and single-process mode at the same time.");

#if !QT_CONFIG(am_multi_process)
    if (forceMultiProcess)
        throw Exception("This application manager build is not multi-process capable.");
    m_isSingleProcessMode = true;
#endif
}

void Main::setupRuntimesAndContainers(const QVariantMap &runtimeConfigurations, const QStringList &runtimeAdditionalLaunchers,
                                      const QVariantMap &containerConfigurations, const QStringList &containerPluginPaths,
                                      const QVariantMap &openGLConfiguration,
                                      const QStringList &iconThemeSearchPaths, const QString &iconThemeName)
{
    QVector<PluginContainerManager *> pluginContainerManagers;

    if (m_isSingleProcessMode) {
        RuntimeFactory::instance()->registerRuntime(new QmlInProcRuntimeManager());
        RuntimeFactory::instance()->registerRuntime(new QmlInProcRuntimeManager(u"qml"_s));
    } else {
        RuntimeFactory::instance()->registerRuntime(new QmlInProcRuntimeManager());
#if QT_CONFIG(am_multi_process)
        RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager());
        RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager(u"qml"_s));

        for (const QString &runtimeId : runtimeAdditionalLaunchers)
            RuntimeFactory::instance()->registerRuntime(new NativeRuntimeManager(runtimeId));

        ContainerFactory::instance()->registerContainer(new ProcessContainerManager());
#else
        if (!runtimeAdditionalLaunchers.isEmpty())
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

        QSet<QString> containersWithConfiguration (containerConfigurations.keyBegin(), containerConfigurations.keyEnd());

        auto containerPlugins = loadPlugins<ContainerManagerInterface>("container", systemContainerPluginPaths + containerPluginPaths);
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

    ContainerFactory::instance()->setConfiguration(containerConfigurations);

    for (auto pcm : std::as_const(pluginContainerManagers)) {
        if (!pcm->initialize()) {
            ContainerFactory::instance()->disableContainer(pcm->identifier());
            qCWarning(LogSystem).noquote() << "Disabling container plugin" << pcm->identifier() << "as it failed to initialize";
        }
    }

    RuntimeFactory::instance()->setConfiguration(runtimeConfigurations);
    RuntimeFactory::instance()->setSystemOpenGLConfiguration(openGLConfiguration);
    RuntimeFactory::instance()->setIconTheme(iconThemeSearchPaths, iconThemeName);
    RuntimeFactory::instance()->setSystemProperties(m_systemProperties.at(SP_ThirdParty),
                                                    m_systemProperties.at(SP_BuiltIn));

    StartupTimer::instance()->checkpoint("after runtime registration");
}

void Main::loadPackageDatabase(bool recreateDatabase, const QString &singlePackage) noexcept(false)
{
    if (!singlePackage.isEmpty()) {
        m_packageDatabase = new PackageDatabase(singlePackage);
    } else {
        m_packageDatabase = new PackageDatabase(m_builtinAppsManifestDirs, m_installationDir,
                                                m_installationDirMountPoint);
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
        const auto apps = package->applications();

        for (const auto app : apps) {
            if (!RuntimeFactory::instance()->manager(app->runtimeName()))
                qCWarning(LogSystem) << "Application" << app->id() << "uses an unknown runtime:" << app->runtimeName();
        }
    }

    StartupTimer::instance()->checkpoint("after package database loading");
}

void Main::setupIntents(int disambiguationTimeout, int startApplicationTimeout,
                        int replyFromApplicationTimeout, int replyFromSystemTimeout) noexcept(false)
{
    m_intentServer = IntentAMImplementation::createIntentServerAndClientInstance(m_packageManager,
                                                                                 disambiguationTimeout,
                                                                                 startApplicationTimeout,
                                                                                 replyFromApplicationTimeout,
                                                                                 replyFromSystemTimeout);
    StartupTimer::instance()->checkpoint("after IntentServer instantiation");
}

void Main::setupSingletons(const QList<QPair<QString, QString>> &containerSelectionConfiguration) noexcept(false)
{
    m_packageManager = PackageManager::createInstance(m_packageDatabase, m_documentDir);
    m_applicationManager = ApplicationManager::createInstance(m_isSingleProcessMode);

    if (m_noSecurity)
        m_applicationManager->setSecurityChecksEnabled(false);
    if (m_developmentMode)
        m_packageManager->setDevelopmentMode(true);

    m_applicationManager->setSystemProperties(m_systemProperties.at(SP_SystemUi));
    m_applicationManager->setContainerSelectionConfiguration(containerSelectionConfiguration);

    StartupTimer::instance()->checkpoint("after ApplicationManager instantiation");

    m_notificationManager = NotificationManager::createInstance();
    StartupTimer::instance()->checkpoint("after NotificationManager instantiation");
}

void Main::setupQuickLauncher(const QHash<std::pair<QString, QString>, int> &runtimesPerContainer,
                              qreal idleLoad, int failedStartLimit,
                              int failedStartLimitIntervalSec) noexcept(false)
{
    if (!runtimesPerContainer.isEmpty()) {
        m_quickLauncher = QuickLauncher::createInstance(runtimesPerContainer, idleLoad,
                                                        failedStartLimit, failedStartLimitIntervalSec);
        StartupTimer::instance()->checkpoint("after quick-launcher setup");
    } else {
        qCDebug(LogSystem) << "Not setting up the quick-launch pool (runtimesPerContainer is 0)";
    }
}

void Main::setupInstaller(bool allowUnsigned, const QStringList &caCertificatePaths) noexcept(false)
{
#if QT_CONFIG(am_installer)
    // make sure the installation and document dirs are valid
    Q_ASSERT(!m_installationDir.isEmpty());
    const auto instPath = QDir(m_installationDir).canonicalPath();
    const auto docPath = m_documentDir.isEmpty() ? QString { }
                                                 : QDir(m_documentDir).canonicalPath();

    if (!docPath.isEmpty() && (instPath.startsWith(docPath) || docPath.startsWith(instPath)))
        throw Exception("either installationDir or documentDir cannot be a sub-directory of the other");

    if (Q_UNLIKELY(hardwareId().isEmpty()))
        throw Exception("the installer is enabled, but the device-id is empty");

    if (!isRunningOnEmbedded()) { // this is just for convenience sake during development
        if (Q_UNLIKELY(!m_installationDir.isEmpty() && !QDir::root().mkpath(m_installationDir)))
            throw Exception("could not create package installation directory: \'%1\'").arg(m_installationDir);
        if (Q_UNLIKELY(!m_documentDir.isEmpty() && !QDir::root().mkpath(m_documentDir)))
            throw Exception("could not create document directory for packages: \'%1\'").arg(m_documentDir);
    }

    StartupTimer::instance()->checkpoint("after installer setup checks");

    if (m_noSecurity || allowUnsigned)
        m_packageManager->setAllowInstallationOfUnsignedPackages(true);

    if (!m_noSecurity) {
        QByteArrayList caCertificateList;
        caCertificateList.reserve(caCertificatePaths.size());

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

    m_packageManager->enableInstaller();

    StartupTimer::instance()->checkpoint("after installer setup");
#else
    Q_UNUSED(allowUnsigned)
    Q_UNUSED(caCertificatePaths)
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
    connect(m_engine, &QQmlEngine::quit, this, [this]() { shutDown(); });
    connect(m_engine, &QQmlEngine::exit, this, [this](int retCode) { shutDown(retCode); });
    CrashHandler::setQmlEngine(m_engine);
    new QmlLogger(m_engine);
    m_engine->setOutputWarningsToStandardError(false);
    m_engine->setImportPathList(m_engine->importPathList() + importPaths);

    StartupTimer::instance()->checkpoint("after QML engine instantiation");
}

void Main::setupWindowTitle(const QString &title, const QString &iconPath)
{
    // For development only: set an icon, so you know which window is the AM
    if (Q_UNLIKELY(!isRunningOnEmbedded())) {
        if (!iconPath.isEmpty())
            QGuiApplication::setWindowIcon(QIcon(iconPath));
        if (!title.isEmpty())
            QGuiApplication::setApplicationDisplayName(title);
    }
}

void Main::setupWindowManager(const QString &waylandSocketName, const QVariantList &waylandExtraSockets,
                              bool slowAnimations, bool noUiWatchdog, bool allowUnknownUiClients)
{
    QUnifiedTimer::instance()->setSlowModeEnabled(slowAnimations);

    m_windowManager = WindowManager::createInstance(m_engine, waylandSocketName);
    m_windowManager->setAllowUnknownUiClients(m_noSecurity || allowUnknownUiClients);
    m_windowManager->setSlowAnimations(slowAnimations);
    m_windowManager->enableWatchdog(!noUiWatchdog);

#if defined(QT_WAYLANDCOMPOSITOR_LIB)
    connect(&m_windowManager->internalSignals, &WindowManagerInternalSignals::compositorAboutToBeCreated,
            this, [this, waylandExtraSockets] {
        // This needs to be delayed until directly before creating the Wayland compositor.
        // Otherwise we have a "dangling" socket you cannot connect to.

        for (const auto &v : waylandExtraSockets) {
            const QVariantMap &wes = v.toMap();

            const QString path = wes.value(u"path"_s).toString();

            if (path.isEmpty())
                continue;

            try {
                QFileInfo fi(path);
                if (!fi.dir().mkpath(u"."_s))
                    throw Exception("could not create path to extra Wayland socket: %1").arg(path);

                auto sudo = SudoClient::instance();

                if (!sudo || sudo->isFallbackImplementation()) {
                    if (!QLocalServer::removeServer(path)) {
                        throw Exception("could not clean up leftover extra Wayland socket %1").arg(path);
                    }
                } else {
                    sudo->removeRecursive(path);
                }

                std::unique_ptr<QLocalServer> extraSocket(new QLocalServer);
                extraSocket->setMaxPendingConnections(0); // disable Qt's new connection handling
                if (!extraSocket->listen(path)) {
                    throw Exception("could not listen on extra Wayland socket %1: %2")
                        .arg(path, extraSocket->errorString());
                }
                int mode = wes.value(u"permissions"_s, -1).toInt();
                int uid = wes.value(u"userId"_s, -1).toInt();
                int gid = wes.value(u"groupId"_s, -1).toInt();

                QByteArray encodedPath = QFile::encodeName(path);

                if ((mode > 0) || (uid != -1) || (gid != -1)) {
                    if (!sudo || sudo->isFallbackImplementation()) {
                        if (mode > 0) {
                            if (::chmod(encodedPath, static_cast<mode_t>(mode)) != 0)
                                throw Exception(errno, "could not chmod(mode: %1) the extra Wayland socket %2")
                                    .arg(QString::number(mode, 8), path);
                        }
                        if ((uid != -1) || (gid != -1)) {
                            if (::chown(encodedPath, static_cast<uid_t>(uid), static_cast<gid_t>(gid)) != 0)
                                throw Exception(errno, "could not chown(uid: %1, gid: %2) the extra Wayland socket %3")
                                    .arg(uid).arg(gid).arg(path);
                        }
                    } else {
                        if (!sudo->setOwnerAndPermissionsRecursive(path, static_cast<uid_t>(uid),
                                                                   static_cast<gid_t>(gid),
                                                                   static_cast<mode_t>(mode))) {
                            throw Exception(Error::IO, "could not change the owner to %1:%2 and the permission"
                                                       " bits to %3 for the extra Wayland socket %4: %5")
                                    .arg(uid).arg(gid).arg(mode, 0, 8).arg(path).arg(sudo->lastError());
                        }
                        // if we changed the owner, ~QLocalServer might not be able to clean up the
                        // socket inode, so we need to sudo this removal as well
                        QObject::connect(extraSocket.get(), &QObject::destroyed, [path, sudo]() {
                            sudo->removeRecursive(path);
                        });
                    }
                }

                m_windowManager->addWaylandSocket(extraSocket.release());
            } catch (const std::exception &e) {
                qCCritical(LogSystem) << "ERROR:" << e.what();
            }
        }
    });
#else
    Q_UNUSED(waylandExtraSockets)
#endif

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
        lockf.reset(new QLockFile(rtDir.absoluteFilePath(tryPattern + u".lock"_s)));
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

void Main::loadQml(bool loadDummyData) noexcept(false)
{
    for (auto iface : std::as_const(m_startupPlugins))
        iface->beforeQmlEngineLoad(m_engine);

    // protect our namespace from this point onward
    qmlProtectModule("QtApplicationManager", 1);
    qmlProtectModule("QtApplicationManager.SystemUI", 1);
    qmlProtectModule("QtApplicationManager", 2);
    qmlProtectModule("QtApplicationManager.SystemUI", 2);

    if (Q_UNLIKELY(loadDummyData)) {
        qCWarning(LogDeployment) << "Loading dummy data is deprecated and will be removed soon";

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

    for (auto iface : std::as_const(m_startupPlugins))
        iface->afterQmlEngineLoad(m_engine);

    StartupTimer::instance()->checkpoint("after loading main QML file");
}

void Main::showWindow(bool showFullscreen)
{
    setQuitOnLastWindowClosed(false);
    connect(this, &QGuiApplication::lastWindowClosed, this, [this]() { shutDown(); });

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
        if (Q_LIKELY(showFullscreen))
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

void Main::setupDBus(const std::function<QString(const char *)> &busForInterface,
                     const std::function<QVariantMap(const char *)> &policyForInterface)
{
#if defined(QT_DBUS_LIB) && QT_CONFIG(am_external_dbus_interfaces)
    registerDBusTypes();

    DBusPolicy::createInstance([](qint64 pid) { return ApplicationManager::instance()->identifyAllApplications(pid); },
                               [](const QString &appId) { return ApplicationManager::instance()->capabilities(appId); });

    // <0> DBusContextAdaptor instance
    // <1> D-Bus name (extracted from callback function busForInterface)
    // <2> D-Bus service
    // <3> D-Bus path
    // <4> Interface name (extracted from Q_CLASSINFO below)

    std::vector<std::tuple<DBusContextAdaptor *, QString, const char *, const char *, const char *>> ifaces;

    auto addInterface = [&ifaces, &busForInterface](DBusContextAdaptor *adaptor, const char *service, const char *path) {
        int idx = adaptor->parent()->metaObject()->indexOfClassInfo("D-Bus Interface");
        if (idx < 0) {
            throw Exception("Could not get class-info \"D-Bus Interface\" for D-Bus adapter %1")
                    .arg(QString::fromLatin1(adaptor->parent()->metaObject()->className()));
        }
        const char *interfaceName = adaptor->parent()->metaObject()->classInfo(idx).value();
        ifaces.emplace_back(adaptor, busForInterface(interfaceName), service, path, interfaceName);
    };

    addInterface(DBusContextAdaptor::create<PackageManagerAdaptor>(m_packageManager),
                 "io.qt.ApplicationManager", "/PackageManager");
    addInterface(DBusContextAdaptor::create<WindowManagerAdaptor>(m_windowManager),
                 "io.qt.ApplicationManager", "/WindowManager");
    addInterface(DBusContextAdaptor::create<NotificationsAdaptor>(m_notificationManager),
                 "org.freedesktop.Notifications", "/org/freedesktop/Notifications");
    addInterface(DBusContextAdaptor::create<ApplicationManagerAdaptor>(m_applicationManager),
                 "io.qt.ApplicationManager", "/ApplicationManager");

    bool autoOnly = true;
    bool noneOnly = true;

    // check if all interfaces are on the "auto" bus and replace the "none" bus with nullptr
    for (auto &&iface : ifaces) {
        QString dbusName = std::get<1>(iface);
        if (dbusName != u"auto"_s)
            autoOnly = false;
        if (dbusName == u"none")
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
#  if defined(Q_OS_LINUX)
            qCWarning(LogDBus) << "Disabling external D-Bus interfaces:" << e.what();
            for (auto &&iface : ifaces)
                std::get<1>(iface).clear();
            noneOnly = true;
#  else
            qCWarning(LogDBus) << "Could not start a private dbus-daemon:" << e.what();
            qCInfo(LogDBus) << "Enabling DBus P2P access for appman-controller";
            for (auto &&iface : ifaces) {
                QString &dbusName = std::get<1>(iface);
                if (dbusName == u"auto")
                    dbusName = u"p2p"_s;
            }
#  endif
        }
    }

    if (!noneOnly) {
        qCDebug(LogDBus) << "Registering D-Bus services:";

        for (auto &&iface : ifaces) {
            auto *generatedAdaptor = std::get<0>(iface)->generatedAdaptor<QDBusAbstractAdaptor>();
            QString &dbusName = std::get<1>(iface);
            const char *interfaceName = std::get<4>(iface);

            if (dbusName.isEmpty())
                continue;

            auto dbusAddress = registerDBusObject(generatedAdaptor, dbusName,
                                                  std::get<2>(iface), std::get<3>(iface));
            if (!DBusPolicy::instance()->add(generatedAdaptor, policyForInterface(interfaceName)))
                throw Exception(Error::DBus, "could not set DBus policy for %1").arg(QString::fromLatin1(interfaceName));

            // Write the bus address to our info file for the appman-controller tool
            if (QByteArrayView { interfaceName }.startsWith("io.qt.")) {
                auto map = m_infoFileContents[u"dbus"_s].toMap();
                map.insert(QString::fromLatin1(interfaceName), dbusAddress);
                m_infoFileContents[u"dbus"_s] = map;
            }
        }
    }
#else
    Q_UNUSED(busForInterface)
    Q_UNUSED(policyForInterface)
    Q_UNUSED(m_p2pServer)
    Q_UNUSED(m_p2pAdaptors)
    Q_UNUSED(m_p2pFailed)
#endif // defined(QT_DBUS_LIB) && QT_CONFIG(am_external_dbus_interfaces)
}

QString Main::registerDBusObject(QDBusAbstractAdaptor *adaptor, const QString &dbusName,
                                 const char *serviceName, const char *path) noexcept(false)
{
#if defined(QT_DBUS_LIB) && QT_CONFIG(am_external_dbus_interfaces)
    QString dbusAddress;
    QString dbusRealName = dbusName;
    QDBusConnection conn((QString()));
    bool isP2P = false;

    if (dbusName.isEmpty()) {
        return { };
    } else if (dbusName == u"system") {
        dbusAddress = QString::fromLocal8Bit(qgetenv("DBUS_SYSTEM_BUS_ADDRESS"));
#  if defined(Q_OS_LINUX)
        if (dbusAddress.isEmpty())
            dbusAddress = u"unix:path=/var/run/dbus/system_bus_socket"_s;
#  endif
        conn = QDBusConnection::systemBus();
    } else if (dbusName == u"session") {
        dbusAddress = QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS"));
        conn = QDBusConnection::sessionBus();
    } else if (dbusName == u"auto") {
        dbusAddress = QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS"));
        // we cannot be using QDBusConnection::sessionBus() here, because some plugin
        // might have called that function before we could spawn our own session bus. In
        // this case, Qt has cached the bus name and we would get the old one back.
        conn = QDBusConnection::connectToBus(dbusAddress, u"qtam_session"_s);
        if (!conn.isConnected())
            return { };
        dbusRealName = u"session"_s;
    } else if (dbusName == u"p2p") {
        if (!m_p2pServer && !m_p2pFailed) {
            m_p2pServer = new QDBusServer(this);
            m_p2pServer->setAnonymousAuthenticationAllowed(true);

            if (!m_p2pServer->isConnected()) {
                m_p2pFailed = true;
                delete m_p2pServer;
                m_p2pServer = nullptr;
                qCCritical(LogDBus) << "Failed to create a P2P DBus server for appman-controller";
            } else {
                QObject::connect(m_p2pServer, &QDBusServer::newConnection,
                                 this, [this](const QDBusConnection &conn) {
                    for (const auto &[path, object] : std::as_const(m_p2pAdaptors).asKeyValueRange())
                        object->registerOnDBus(conn, path);
                });
            }
        }
        if (m_p2pFailed)
            return { };
        m_p2pAdaptors.insert(QString::fromLatin1(path), qobject_cast<DBusContextAdaptor *>(adaptor->parent()));
        dbusAddress = u"p2p:"_s + m_p2pServer->address();
        isP2P = true;
    } else {
        dbusAddress = dbusName;
        conn = QDBusConnection::connectToBus(dbusAddress, u"custom"_s);
    }

    if (!isP2P) {
        if (!conn.isConnected()) {
            throw Exception("could not connect to D-Bus (%1): %2")
                .arg(dbusAddress.isEmpty() ? dbusRealName : dbusAddress).arg(conn.lastError().message());
        }

        if (!conn.registerObject(QString::fromLatin1(path), adaptor->parent(), QDBusConnection::ExportAdaptors)) {
            throw Exception("could not register object %1 on D-Bus (%2): %3")
                .arg(QString::fromLatin1(path)).arg(dbusRealName).arg(conn.lastError().message());
        }

        if (!conn.registerService(QString::fromLatin1(serviceName))) {
            throw Exception("could not register service %1 on D-Bus (%2): %3")
                .arg(QString::fromLatin1(serviceName)).arg(dbusRealName).arg(conn.lastError().message());
        }
    }

    if (adaptor->parent() && adaptor->parent()->parent()) {
        // we need this information later on to tell apps where services are listening
        adaptor->parent()->parent()->setProperty("_am_dbus_name", dbusRealName);
        adaptor->parent()->parent()->setProperty("_am_dbus_address", dbusAddress);
    }

    qCDebug(LogDBus).nospace().noquote() << " * " << serviceName << path << " [on bus: " << dbusRealName << "]";

    return dbusAddress.isEmpty() ? dbusRealName : dbusAddress;
#else
    Q_UNUSED(adaptor)
    Q_UNUSED(dbusName)
    Q_UNUSED(serviceName)
    Q_UNUSED(path)
    return { };
#endif // defined(QT_DBUS_LIB) && QT_CONFIG(am_external_dbus_interfaces)
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

QT_END_NAMESPACE_AM

#include "moc_main.cpp"
