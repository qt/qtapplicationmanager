// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QUrl>
#include <QRegularExpression>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QMetaObject>
#include <QUuid>
#include <QThread>
#include <QMimeDatabase>
#include <qplatformdefs.h>
#if defined(QT_GUI_LIB)
#  include <QDesktopServices>
#endif
#if defined(Q_OS_WINDOWS)
#  define write(a, b, c) _write(a, b, static_cast<unsigned int>(c))
#endif

#include "global.h"
#include "applicationinfo.h"
#include "installationreport.h"
#include "logging.h"
#include "exception.h"
#include "applicationmanager.h"
#include "applicationmodel.h"
#include "applicationmanager_p.h"
#include "application.h"
#include "runtimefactory.h"
#include "containerfactory.h"
#include "quicklauncher.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "globalruntimeconfiguration.h"
#include "qml-utilities.h"
#include "utilities.h"
#include "qtyaml.h"
#include "debugwrapper.h"
#include "amnamespace.h"
#include "package.h"
#include "packagemanager.h"

#include <memory>

using namespace Qt::StringLiterals;

/*!
    \qmltype ApplicationManager
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-singletons
    \brief The application model and controller.

    The ApplicationManager singleton type is the core of the application manager.
    It provides both a DBus and a QML API for all of its functionality.

    The type is derived from \c QAbstractListModel, so it can be used directly
    as a model in app-grid views.

    \target ApplicationManager Roles

    The following roles are available in this model:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c applicationId
        \li string
        \li The unique Id of an application, represented as a string (e.g. \c Browser or
            \c com.pelagicore.music)
    \row
        \li \c name
        \li string
        \li The name of the application. If possible, already translated to the current locale.
    \row
        \li \c icon
        \li string
        \li The URL of the application's icon.

    \row
        \li \c isRunning
        \li bool
        \li A boolean value representing the run-state of the application.
    \row
        \li \c isStartingUp
        \li bool
        \li A boolean value indicating whether the application is starting up and not fully operational yet.
    \row
        \li \c isShuttingDown
        \li bool
        \li A boolean value indicating whether the application is currently shutting down.
    \row
        \li \c isBlocked
        \li bool
        \li A boolean value that gets set when the application manager needs to block the application
        from running: this is normally only the case while an update is applied.
    \row
        \li \c isUpdating
        \li bool
        \li A boolean value indicating whether the application is currently being installed or updated.
            If \c true, the \c updateProgress can be used to track the actual progress.
    \row
        \li \c isRemovable
        \li bool
        \li A boolean value indicating whether this application is user-removable; \c true for all
            dynamically installed third party applications and \c false for all system applications.

    \row
        \li \c updateProgress
        \li real
        \li While \c isUpdating is \c true, querying this role returns the actual progress as a floating-point
            value in the \c 0.0 to \c 1.0 range.

    \row
        \li \c codeFilePath
        \li string
        \li The filesystem path to the main "executable". The format of this file is dependent on the
        actual runtime though: for QML applications the "executable" is the \c main.qml file.

    \row
        \li \c categories
        \li list<string>
        \li The categories this application is registered for via its meta-data file.

    \row
        \li \c version
        \li string
        \li The currently installed version of this application.

    \row
        \li \c application
        \li ApplicationObject
        \li The underlying \l ApplicationObject for quick access to the properties outside of a
            model delegate.
    \row
        \li \c applicationObject
        \li ApplicationObject
        \li Exactly the same as \c application. This was added to keep the role names between the
            PackageManager and ApplicationManager models as similar as possible.
            This role was introduced in Qt version 6.6.
    \endtable

    \note The index-based API is currently not available via DBus. However, the same functionality
    is provided by the id-based API.

    After importing, you can just use the ApplicationManager singleton as follows:

    \qml
    import QtQuick
    import QtApplicationManager.SystemUI

    ListView {
        id: appList
        model: ApplicationManager

        delegate: Text {
            text: name + "(" + applicationId + ")"

            MouseArea {
                anchors.fill: parent
                onClick: ApplicationManager.startApplication(applicationId)
            }
        }
    }
    \endqml
*/

/*!
    \qmlproperty int ApplicationManager::count
    \readonly

    This property holds the number of applications available.
*/

/*!
    \qmlproperty bool ApplicationManager::singleProcess
    \readonly

    This property indicates whether the application manager runs in single- or multi-process mode.
*/

/*!
    \qmlproperty bool ApplicationManager::shuttingDown
    \readonly

    This property is there to inform the System UI, when the application manager has entered its
    shutdown phase. New application starts are already prevented, but the System UI might want
    to impose additional restrictions in this state.
*/

/*!
    \qmlproperty bool ApplicationManager::securityChecksEnabled
    \readonly

    This property holds whether security related checks are enabled.

    \sa no-security
*/

/*!
    \qmlproperty var ApplicationManager::systemProperties
    \readonly

    Returns the project specific \l{system properties} that were set via the config file.
*/

/*!
    \qmlproperty var ApplicationManager::containerSelectionFunction

    A JavaScript function callback that will be called whenever the application manager needs to
    instantiate a container for running an application. See \l {Container Selection Configuration}
    for more information.
*/

/*!
    \qmlsignal ApplicationManager::applicationWasActivated(string id, string aliasId)

    This signal is emitted when an application identified by \a id is (re-)started via
    the ApplicationManager API, possibly through an alias, provided in \a aliasId.

    The window manager should take care of raising the application's window in this case.
*/

/*!
    \qmlsignal ApplicationManager::applicationRunStateChanged(string id, enumeration runState)

    This signal is emitted when the \a runState of the application identified by \a id changed.
    Possible values for the \l {ApplicationObject::runState} {runState} are defined by the
    ApplicationObject type.

    For example this signal can be used to restart an application in multi-process mode when
    it has crashed:

    \qml
    Connections {
        target: ApplicationManager
        function onApplicationRunStateChanged() {
            if (runState === Am.NotRunning
                && ApplicationManager.application(id).lastExitStatus === Am.CrashExit) {
                ApplicationManager.startApplication(id);
            }
        }
    }
    \endqml

    See also Application::runState
*/

/*!
    \qmlsignal ApplicationManager::applicationAdded(string id)

    This signal is emitted after a new application, identified by \a id, has been installed via the
    PackageManager. The model has already been update before this signal is sent out.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application manager
          will also emit these "high-level" signals for System UIs that cannot work directly on the
          ApplicationManager model: applicationAdded, applicationAboutToBeRemoved and applicationChanged.
*/

/*!
    \qmlsignal ApplicationManager::applicationAboutToBeRemoved(string id)

    This signal is emitted before an existing application, identified by \a id, is removed via the
    PackageManager.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application manager
          will also emit these "high-level" signals for System UIs that cannot work directly on the
          ApplicationManager model: applicationAdded, applicationAboutToBeRemoved and applicationChanged.
*/

/*!
    \qmlsignal ApplicationManager::applicationChanged(string id, list<string> changedRoles)

    Emitted whenever one or more data roles, denoted by \a changedRoles, changed on the application
    identified by \a id. An empty list in the \a changedRoles argument means that all roles should
    be considered modified.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application manager
          will also emit these "high-level" signals for System UIs that cannot work directly on the
          ApplicationManager model: applicationAdded, applicationAboutToBeRemoved and applicationChanged.
*/

/*!
    \qmlproperty bool ApplicationManager::windowManagerCompositorReady
    \readonly

    This property starts with the value \c false and will change to \c true once the Wayland
    compositor is ready to accept connections from other processes. In multi-process mode, this
    happens implicitly after the System UI's main QML has been loaded.
*/

enum AMRoles
{
    Id = Qt::UserRole,
    Name,
    Icon,

    IsRunning,
    IsStartingUp,
    IsShuttingDown,
    IsBlocked,
    IsUpdating,
    IsRemovable,

    UpdateProgress,

    CodeFilePath,
    RuntimeName,
    RuntimeParameters,
    Capabilities,
    Categories,
    Version,
    ApplicationItem,
    ApplicationObject, // needed to keep the roles similar to PackageManager

    LastExitCode,
    LastExitStatus
};

QT_BEGIN_NAMESPACE_AM

ApplicationManagerPrivate::ApplicationManagerPrivate()
{
    currentLocale = QLocale::system().name(); //TODO: language changes

    roleNames.insert(AMRoles::Id, "applicationId");
    roleNames.insert(AMRoles::Name, "name");
    roleNames.insert(AMRoles::Icon, "icon");
    roleNames.insert(AMRoles::IsRunning, "isRunning");
    roleNames.insert(AMRoles::IsStartingUp, "isStartingUp");
    roleNames.insert(AMRoles::IsShuttingDown, "isShuttingDown");
    roleNames.insert(AMRoles::IsBlocked, "isBlocked");
    roleNames.insert(AMRoles::IsUpdating, "isUpdating");
    roleNames.insert(AMRoles::IsRemovable, "isRemovable");
    roleNames.insert(AMRoles::UpdateProgress, "updateProgress");
    roleNames.insert(AMRoles::CodeFilePath, "codeFilePath");
    roleNames.insert(AMRoles::RuntimeName, "runtimeName");
    roleNames.insert(AMRoles::RuntimeParameters, "runtimeParameters");
    roleNames.insert(AMRoles::Capabilities, "capabilities");
    roleNames.insert(AMRoles::Categories, "categories");
    roleNames.insert(AMRoles::Version, "version");
    roleNames.insert(AMRoles::ApplicationItem, "application");
    roleNames.insert(AMRoles::ApplicationObject, "applicationObject");
    roleNames.insert(AMRoles::LastExitCode, "lastExitCode");
    roleNames.insert(AMRoles::LastExitStatus, "lastExitStatus");
}

ApplicationManagerPrivate::~ApplicationManagerPrivate()
{
    for (const QString &scheme : std::as_const(registeredMimeSchemes))
        QDesktopServices::unsetUrlHandler(scheme);
    qDeleteAll(apps);
}

ApplicationManager *ApplicationManager::s_instance = nullptr;

ApplicationManager *ApplicationManager::createInstance(bool singleProcess)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("ApplicationManager::createInstance() was called a second time.");

    std::unique_ptr<ApplicationManager> am(new ApplicationManager(singleProcess));

    qRegisterMetaType<Am::RunState>();
    qRegisterMetaType<Am::ExitStatus>();
    qRegisterMetaType<Am::ProcessError>();

    if (Q_UNLIKELY(!PackageManager::instance()))
        qFatal("ApplicationManager::createInstance() was called before a PackageManager singleton was instantiated.");

    s_instance = am.release();
    connect(&PackageManager::instance()->internalSignals, &PackageManagerInternalSignals::registerApplication,
            s_instance, [](ApplicationInfo *applicationInfo, Package *package) {
        instance()->addApplication(applicationInfo, package);
        qCDebug(LogSystem).nospace().noquote() << " ++ application: " << applicationInfo->id() << " [package: " << package->id() << "]";
    });
    connect(&PackageManager::instance()->internalSignals, &PackageManagerInternalSignals::unregisterApplication,
            s_instance, [](ApplicationInfo *applicationInfo, Package *package) {
        instance()->removeApplication(applicationInfo, package);
        qCDebug(LogSystem).nospace().noquote() << " -- application: " << applicationInfo->id() << " [package: " << package->id() << "]";
    });

    return s_instance;
}

ApplicationManager *ApplicationManager::instance()
{
    if (!s_instance)
        qFatal("ApplicationManager::instance() was called before createInstance().");
    return s_instance;
}

ApplicationManager::ApplicationManager(bool singleProcess, QObject *parent)
    : QAbstractListModel(parent)
    , d(new ApplicationManagerPrivate())
{
    d->singleProcess = singleProcess;
    connect(this, &QAbstractItemModel::rowsInserted, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ApplicationManager::countChanged);
}

ApplicationManager::~ApplicationManager()
{
    delete d;
    s_instance = nullptr;
}

bool ApplicationManager::isSingleProcess() const
{
    return d->singleProcess;
}

bool ApplicationManager::isShuttingDown() const
{
    return d->shuttingDown;
}

bool ApplicationManager::securityChecksEnabled() const
{
    return d->securityChecksEnabled;
}

void ApplicationManager::setSecurityChecksEnabled(bool enabled)
{
    d->securityChecksEnabled = enabled;
}

QVariantMap ApplicationManager::systemProperties() const
{
    return d->systemProperties;
}

void ApplicationManager::setSystemProperties(const QVariantMap &map)
{
    d->systemProperties = map;
}

void ApplicationManager::setContainerSelectionConfiguration(const QList<std::pair<QString, QString>> &containerSelectionConfig)
{
    d->containerSelectionConfig = containerSelectionConfig;
}

QJSValue ApplicationManager::containerSelectionFunction() const
{
    return d->containerSelectionFunction;
}

void ApplicationManager::setContainerSelectionFunction(const QJSValue &callback)
{
    if (callback.isCallable() && !callback.equals(d->containerSelectionFunction)) {
        d->containerSelectionFunction = callback;
        emit containerSelectionFunctionChanged();
    }
}

bool ApplicationManager::isWindowManagerCompositorReady() const
{
    return d->windowManagerCompositorReady;
}

void ApplicationManager::setWindowManagerCompositorReady(bool ready)
{
    if (d->windowManagerCompositorReady != ready) {
        d->windowManagerCompositorReady = ready;
        emit windowManagerCompositorReadyChanged(ready);
    }
}

QStringList ApplicationManager::availableRuntimeIds() const
{
    return RuntimeFactory::instance()->runtimeIds();
}

QStringList ApplicationManager::availableContainerIds() const
{
    return ContainerFactory::instance()->containerIds();
}

QVector<Application *> ApplicationManager::applications() const
{
    return d->apps;
}

Application *ApplicationManager::fromId(const QString &id) const
{
    for (Application *app : std::as_const(d->apps)) {
        if (app->id() == id)
            return app;
    }
    return nullptr;
}

QVector<Application *> ApplicationManager::fromProcessId(qint64 pid) const
{
    QVector<Application *> apps;

    // pid could be an indirect child (e.g. when started via gdbserver)
    qint64 appmanPid = QCoreApplication::applicationPid();

    int level = 0;
    while ((pid > 1) && (pid != appmanPid) && (level < 5)) {
        for (Application *app : std::as_const(d->apps)) {
            if (apps.contains(app))
                continue;
            if (app->currentRuntime() && (app->currentRuntime()->applicationProcessId() == pid))
                apps.append(app);
        }
        pid = getParentPid(pid);
        ++level;
    }
    return apps;
}

Application *ApplicationManager::fromSecurityToken(const QByteArray &securityToken) const
{
    if (securityToken.size() != AbstractRuntime::SecurityTokenSize)
        return nullptr;

    for (Application *app : std::as_const(d->apps)) {
        if (app->currentRuntime() && app->currentRuntime()->securityToken() == securityToken)
            return app;
    }
    return nullptr;
}

QVector<Application *> ApplicationManager::schemeHandlers(const QString &scheme) const
{
    QVector<Application *> handlers;

    for (Application *app : std::as_const(d->apps)) {
        const auto mimeTypes = app->supportedMimeTypes();
        for (const QString &mime : mimeTypes) {
            auto pos = mime.indexOf(u'/');

            if ((pos > 0)
                    && (mime.left(pos) == u"x-scheme-handler")
                    && (mime.mid(pos + 1) == scheme)) {
                handlers << app;
            }
        }
    }
    return handlers;
}

QVector<Application *> ApplicationManager::mimeTypeHandlers(const QString &mimeType) const
{
    QVector<Application *> handlers;

    for (Application *app : std::as_const(d->apps)) {
        if (app->supportedMimeTypes().contains(mimeType))
            handlers << app;
    }
    return handlers;
}

QVariantMap ApplicationManager::get(Application *app) const
{
    QVariantMap map;
    if (app) {
        const QHash<int, QByteArray> roles = roleNames();
        for (auto it = roles.begin(); it != roles.end(); ++it)
            map.insert(QString::fromLatin1(it.value()), dataForRole(app, it.key()));
    }
    return map;
}

void ApplicationManager::registerMimeTypes()
{
#if defined(QT_GUI_LIB)
    QSet<QString> schemes;
    schemes << u"file"_s << u"http"_s << u"https"_s;

    for (Application *app : std::as_const(d->apps)) {
        const auto mimeTypes = app->supportedMimeTypes();
        for (const QString &mime : mimeTypes) {
            auto pos = mime.indexOf(u'/');

            if ((pos > 0) && (mime.left(pos) == u"x-scheme-handler"))
                schemes << mime.mid(pos + 1);
        }
    }
    QSet<QString> registerSchemes = schemes;
    registerSchemes.subtract(d->registeredMimeSchemes);
    QSet<QString> unregisterSchemes = d->registeredMimeSchemes;
    unregisterSchemes.subtract(schemes);

    for (const QString &scheme : std::as_const(unregisterSchemes))
        QDesktopServices::unsetUrlHandler(scheme);
    for (const QString &scheme : std::as_const(registerSchemes))
        QDesktopServices::setUrlHandler(scheme, this, "openUrlRelay");

    d->registeredMimeSchemes = schemes;
#endif
}

bool ApplicationManager::startApplicationInternal(const QString &appId, const QString &documentUrl,
                                                  const QString &documentMimeType,
                                                  const QString &debugWrapperSpecification,
                                                  QVector<int> &&stdioRedirections)  noexcept(false)
{
    auto redirectionGuard = qScopeGuard([&stdioRedirections]() {
        closeAndClearFileDescriptors(stdioRedirections);
    });

    if (d->shuttingDown)
        throw Exception("Cannot start applications during shutdown");
    QPointer<Application> app = fromId(appId);
    if (!app)
        throw Exception("Cannot start application: id '%1' is not known").arg(appId);
    if (app->isBlocked())
        throw Exception("Application %1 is blocked - cannot start").arg( app->id());

    AbstractRuntime *runtime = app->currentRuntime();
    auto runtimeManager = runtime ? runtime->manager() : RuntimeFactory::instance()->manager(app->runtimeName());
    if (!runtimeManager)
        throw Exception("No RuntimeManager found for runtime: %1").arg(app->runtimeName());
    bool inProcess = runtimeManager->inProcess();

    // validate stdio redirections
    if (stdioRedirections.size() > 3) {
        throw Exception("Tried to start application %1 using an invalid standard IO redirection specification")
                .arg(app->id());
    }
    bool hasStdioRedirections = !stdioRedirections.isEmpty();
    if (hasStdioRedirections) {
        // we have an array - check if it just consists of -1 fds
        hasStdioRedirections = false;
        std::for_each(stdioRedirections.cbegin(), stdioRedirections.cend(), [&hasStdioRedirections](int fd) {
            if (fd >= 0)
                hasStdioRedirections = true;
        });
    }

    // validate the debug-wrapper
    QStringList debugWrapperCommand;
    QMap<QString, QString> debugEnvironmentVariables;
    if (!debugWrapperSpecification.isEmpty()) {
        if (isSingleProcess())
            throw Exception("Using debug-wrappers is not supported when the application manager is running in single-process mode.");
        if (inProcess) {
            throw Exception("Using debug-wrappers is not supported when starting an app using an in-process runtime (%1).")
                .arg(runtimeManager->identifier());
        }

        if (!DebugWrapper::parseSpecification(debugWrapperSpecification, debugWrapperCommand,
                                              debugEnvironmentVariables)) {
            throw Exception("Tried to start application %1 using an invalid debug-wrapper specification: %2")
                    .arg(app->id(), debugWrapperSpecification);
        }
    }

    if (runtime) {
        switch (runtime->state()) {
        case Am::StartingUp:
        case Am::Running:
            if (!debugWrapperCommand.isEmpty()) {
                throw Exception("Application %1 is already running - cannot start with debug-wrapper: %2")
                        .arg(app->id(), debugWrapperSpecification);
            }
            if (hasStdioRedirections) {
                throw Exception("Application %1 is already running - cannot set standard IO redirections")
                        .arg(app->id());
            }
            if (!documentUrl.isNull())
                runtime->openDocument(documentUrl, documentMimeType);
            else if (!app->documentUrl().isNull())
                runtime->openDocument(app->documentUrl(), documentMimeType);

            emitActivated(app);
            return true;

        case Am::ShuttingDown:
            return false;

        case Am::NotRunning:
            throw Exception("Application %1 is not running, but still has a Runtime object attached")
                    .arg(app->id());
        }
    }

    AbstractContainer *container = nullptr;
    QString containerId;

    if (!inProcess) {
        if (d->containerSelectionConfig.isEmpty()) {
            containerId = u"process"_s;
        } else {
            // check config file
            for (const auto &it : std::as_const(d->containerSelectionConfig)) {
                const QString &key = it.first;
                const QString &value = it.second;
                bool hasAsterisk = key.contains(u'*');

                if ((hasAsterisk && key.length() == 1)
                        || (!hasAsterisk && key == app->id())
                        || QRegularExpression(QRegularExpression::wildcardToRegularExpression(key)).match(app->id()).hasMatch()) {
                    containerId = value;
                    break;
                }
            }
        }

        if (d->containerSelectionFunction.isCallable()) {
            QJSValueList args = { QJSValue(app->id()), QJSValue(containerId) };
            containerId = d->containerSelectionFunction.call(args).toString();
        }

        if (!ContainerFactory::instance()->manager(containerId))
            throw Exception("No ContainerManager found for container: %1").arg(containerId);
    }
    bool attachRuntime = false;

    if (!runtime) {
        if (!inProcess) {
            if (QuickLauncher::instance()) {
                // we cannot use the quicklaunch pool, if
                //  (a) a debug-wrapper is being used,
                //  (b) stdio is redirected or
                //  (c) the app requests special environment variables or
                //  (d) the app requests a different OpenGL config from the AM
                const char *cannotUseQuickLaunch = nullptr;

                if (!debugWrapperCommand.isEmpty())
                    cannotUseQuickLaunch = "the app is started using a debug-wrapper";
                else if (hasStdioRedirections)
                    cannotUseQuickLaunch = "standard I/O is redirected";
                else if (!app->runtimeParameters().value(u"environmentVariables"_s).toMap().isEmpty())
                    cannotUseQuickLaunch = "the app requests custom environment variables";
                else if (app->info()->openGLConfiguration() != GlobalRuntimeConfiguration::instance().openGLConfiguration)
                    cannotUseQuickLaunch = "the app requests a custom OpenGL configuration";

                if (cannotUseQuickLaunch) {
                    qCDebug(LogSystem) << "Cannot use quick-launch for application" << app->id()
                                       << "because" << cannotUseQuickLaunch;
                } else {
                    // check quicklaunch pool
                    QPair<AbstractContainer *, AbstractRuntime *> quickLaunch =
                            QuickLauncher::instance()->take(containerId, app->info()->runtimeName());
                    container = quickLaunch.first;
                    runtime = quickLaunch.second;

                    if (container || runtime) {
                        qCDebug(LogSystem) << "Found a quick-launch entry for container" << containerId
                                           << "and runtime" << app->info()->runtimeName() << "->"
                                           << container << runtime;

                        if (!container && runtime) {
                            runtime->deleteLater();
                            qCCritical(LogSystem) << "ERROR: QuickLauncher provided a runtime without a container.";
                            return false;
                        }
                    }
                }
            }

            if (!container) {
                container = ContainerFactory::instance()->create(containerId, app, std::move(stdioRedirections),
                                                                 debugEnvironmentVariables, debugWrapperCommand);
            } else {
                container->setApplication(app);
            }
            if (!container) {
                qCCritical(LogSystem) << "ERROR: Couldn't create Container for Application (" << app->id() <<")!";
                return false;
            }
            if (runtime)
                attachRuntime = true;

        } else { // inProcess
            if (hasStdioRedirections) {
                static const char *noRedirectMsg = "NOTE: redirecting standard IO is not possible for in-process runtimes";

                int fd = stdioRedirections.value(2, -1);
                if (fd < 0)
                    fd = stdioRedirections.value(1, -1);
                if (fd >= 0) {
                    auto dummy = write(fd, noRedirectMsg, qstrlen(noRedirectMsg));
                    dummy += write(fd, "\n", 1);
                    Q_UNUSED(dummy)
                }
                qCWarning(LogSystem) << noRedirectMsg;
            }
        }

        if (!runtime)
            runtime = RuntimeFactory::instance()->create(container, app);

        if (runtime)
            emit internalSignals.newRuntimeCreated(runtime);
    }

    if (!runtime) {
        qCCritical(LogSystem) << "ERROR: Couldn't create Runtime for Application (" << app->id() <<")!";
        return false;
    }

    // if an app is stopped because of a removal and the container is slow to stop, we might
    // end up with a dead app pointer in this callback at some point
    connect(runtime, &AbstractRuntime::stateChanged, this, [this, app, appId](Am::RunState newRuntimeState) {
        if (app)
            app->setRunState(newRuntimeState);
        emit applicationRunStateChanged(appId, newRuntimeState);
        if (app)
            emitDataChanged(app, QVector<int> { AMRoles::IsRunning, AMRoles::IsStartingUp, AMRoles::IsShuttingDown });
    });

    if (!documentUrl.isNull())
        runtime->openDocument(documentUrl, documentMimeType);
    else if (!app->documentUrl().isNull())
        runtime->openDocument(app->documentUrl(), documentMimeType);

    qCDebug(LogSystem) << "Starting application" << app->id() << "in container" << containerId
                       << "using runtime" << runtimeManager->identifier();
    if (!documentUrl.isEmpty())
        qCDebug(LogSystem) << "  documentUrl:" << documentUrl;

    // We can only start the app when both the container and the windowmanager are ready.
    // Using a state-machine would be one option, but then we would need that state-machine
    // object plus the per-app state. Relying on 2 lambdas is the easier choice for now.

    auto doStartInContainer = [this, app, attachRuntime, runtime, containerId]() -> bool {
        bool successfullyStarted = false;
        if (app) {
            successfullyStarted = attachRuntime ? runtime->attachApplicationToQuickLauncher(app)
                                                : runtime->start();
        }
        if (successfullyStarted) {
            emitActivated(app);
        } else {
            qCWarning(LogSystem) << "Failed to start application" << app->id() << "in container"
                                 << containerId << "using runtime" << runtime->manager()->identifier();
            delete runtime; // ~Runtime() will clean up app->m_runtime
        }
        return successfullyStarted;
    };

    auto tryStartInContainer = [this, container, inProcess, doStartInContainer]() -> bool {
        if (inProcess || container->isReady()) {
            // Since the container is already ready, start the app immediately
            return doStartInContainer();
        } else {
            // We postpone the starting of the application to a later point in time,
            // since the container is not ready yet
            // since the container is not ready yet
#if defined(Q_CC_MSVC)
            qApp->connect(container, &AbstractContainer::ready, this, doStartInContainer); // MSVC cannot distinguish between static and non-static overloads in lambdas
#else
            connect(container, &AbstractContainer::ready, this, doStartInContainer);
#endif
            return true;
        }
    };

    if (inProcess || isWindowManagerCompositorReady()) {
        return tryStartInContainer();
    } else {
        connect(this, &ApplicationManager::windowManagerCompositorReadyChanged, tryStartInContainer);
        return true;
    }
}

void ApplicationManager::stopApplicationInternal(Application *app, bool forceKill)
{
    if (!app)
        return;
    AbstractRuntime *rt = app->currentRuntime();
    if (rt)
        rt->stop(forceKill);
}

/*!
    \qmlmethod bool ApplicationManager::startApplication(string id, string document)

    Instructs the application manager to start the application identified by its unique \a id. The
    optional argument \a document will be supplied to the application as is - most commonly this
    is used to refer to a document to display.
    Returns \c true if the application \a id is valid and the application manager was able to start
    the runtime plugin. Returns \c false otherwise. Note that even though this call may
    indicate success, the application may still later fail to start correctly as the actual
    startup process within the runtime plugin may be asynchronous.

    \sa ApplicationObject::start
*/
bool ApplicationManager::startApplication(const QString &id, const QString &documentUrl)
{
    try {
        return startApplicationInternal(id, documentUrl);
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        return false;
    }
}

/*!
    \qmlmethod bool ApplicationManager::debugApplication(string id, string debugWrapper, string document)

    Instructs the application manager to start the application identified by its unique \a id, just
    like startApplication. The application is started via the given \a debugWrapper though. The
    optional argument \a document will be supplied to the application as is - most commonly this is
    used to refer to a document to display.

    Returns a \c bool value indicating success. See the full documentation at
    ApplicationManager::startApplication for more information.

    Please see the \l{Debugging} page for more information on how to setup and use these
    debug-wrappers.

    \sa ApplicationObject::debug
*/

bool ApplicationManager::debugApplication(const QString &id, const QString &debugWrapper, const QString &documentUrl)
{
    try {
        return startApplicationInternal(id, documentUrl, QString(), debugWrapper);
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        return false;
    }
}

/*!
    \qmlmethod ApplicationManager::stopApplication(string id, bool forceKill)

    Tells the application manager to stop an application identified by its unique \a id. The
    meaning of the \a forceKill parameter is runtime dependent, but in general you should always try
    to stop an application with \a forceKill set to \c false first in order to allow a clean
    shutdown. Use \a forceKill set to \c true only as a last resort to kill hanging applications.

    QML applications and native applications that \l {manifest supportsApplicationInterface}
    {support the ApplicationInterface} will be notified via ApplicationInterface::quit().
    All other applications will be sent the Unix \c TERM signal.

    \sa ApplicationObject::stop
*/
void ApplicationManager::stopApplication(const QString &id, bool forceKill)
{
    stopApplicationInternal(fromId(id), forceKill);
}

/*!
    \qmlmethod ApplicationManager::stopAllApplications(bool forceKill)

    Tells the application manager to stop all running applications. The meaning of the \a forceKill
    parameter is runtime dependent, but in general you should always try to stop an application
    with \a forceKill set to \c false first in order to allow a clean shutdown.
    Use \a forceKill set to \c true only as a last resort to kill hanging applications.

    \sa stopApplication
*/
void ApplicationManager::stopAllApplications(bool forceKill)
{
    for (Application *app : std::as_const(d->apps)) {
        AbstractRuntime *rt = app->currentRuntime();
        if (rt)
            rt->stop(forceKill);
    }
}

/*!
    \qmlmethod bool ApplicationManager::openUrl(string url)

    Tries start an application that is capable of handling \a url. The application manager will
    first look at the URL's scheme:
    \list
    \li If it is \c{file:}, the operating system's MIME database will be consulted, which will
        try to find a MIME type match, based on file endings or file content. In case this is
        successful, the application manager will use this MIME type to find all of its applications
        that claim support for it (see the \l{mimeTypes field} in the application's manifest).
        A music player application that can handle \c mp3 and \c wav files, could add this to its
        manifest:
        \badcode
        mimeTypes: [ 'audio/mpeg', 'audio/wav' ]
        \endcode
    \li If it is something other than \c{file:}, the application manager will consult its
        internal database of applications that claim support for a matching \c{x-scheme-handler/...}
        MIME type. In order to have your web-browser application handle \c{http:} and \c{https:}
        URLs, you would have to have this in your application's manifest:
        \badcode
        mimeTypes: [ 'x-scheme-handler/http', 'x-scheme-handler/https' ]
        \endcode
    \endlist

    If there is at least one possible match, it depends on the signal openUrlRequested() being
    connected within the System UI:
    In case the signal is not connected, an arbitrary application from the matching set will be
    started. Otherwise the application manager will emit the openUrlRequested signal and
    return \c true. It is up to the receiver of this signal to choose from one of possible
    applications via acknowledgeOpenUrlRequest or deny the request entirely via rejectOpenUrlRequest.
    Not calling one of these two functions will result in memory leaks.

    If an application is started by one of the two mechanisms, the \a url is supplied to
    the application as a document to open via its ApplicationInterface.

    Returns \c true, if a match was found in the database, or \c false otherwise.

    \sa openUrlRequested, acknowledgeOpenUrlRequest, rejectOpenUrlRequest
*/
bool ApplicationManager::openUrl(const QString &urlStr)
{
    //TODO: relay to a well-known Intent call

    // QDesktopServices::openUrl has a special behavior when called recursively, which makes sense
    // on the desktop, but is completely counter-productive for the AM.
    static bool recursionGuard = false;
    if (recursionGuard) {
        QMetaObject::invokeMethod(this, [urlStr, this]() { openUrl(urlStr); }, Qt::QueuedConnection);
        return true; // this is not correct, but the best we can do in this situation
    }
    recursionGuard = true;

    QUrl url(urlStr);
    QString mimeTypeName;
    QVector<Application *> apps;

    if (url.isValid()) {
        QString scheme = url.scheme();
        if (scheme != u"file")
            apps = schemeHandlers(scheme);

        if (apps.isEmpty()) {
            QMimeDatabase mdb;
            QMimeType mt = mdb.mimeTypeForUrl(url);
            mimeTypeName = mt.name();

            apps = mimeTypeHandlers(mimeTypeName);
        }
    }

    if (!apps.isEmpty()) {
        if (!isSignalConnected(QMetaMethod::fromSignal(&ApplicationManager::openUrlRequested))) {
            // If the System UI does not react to the signal, then just use the first match.
            try {
                startApplicationInternal(apps.constFirst()->id(), urlStr, mimeTypeName);
            } catch (const Exception &e) {
                qCWarning(LogSystem) << "openUrl for" << urlStr << "requested app" << apps.constFirst()->id()
                                     << "which could not be started:" << e.errorString();
            }
        } else {
            ApplicationManagerPrivate::OpenUrlRequest req {
                QUuid::createUuid().toString(),
                        urlStr,
                        mimeTypeName,
                        QStringList()
            };
            for (const auto &app : std::as_const(apps))
                req.possibleAppIds << app->id();
            d->openUrlRequests << req;

            emit openUrlRequested(req.requestId, req.urlStr, req.mimeTypeName, req.possibleAppIds);
        }
    }

    recursionGuard = false;
    return !apps.isEmpty();
}

/*!
    \qmlsignal ApplicationManager::openUrlRequested(string requestId, string url, string mimeType, list<string> possibleAppIds)

    This signal is emitted when the application manager is requested to open an URL. This can happen
    by calling
    \list
    \li Qt.openUrlExternally in an application,
    \li Qt.openUrlExternally in the System UI,
    \li ApplicationManager::openUrl in the System UI or
    \li \c io.qt.ApplicationManager.openUrl via D-Bus
    \endlist
    \note This signal is only emitted, if there is a receiver connected at all - see openUrl for the
          fallback behavior.

    The receiver of this signal can inspect the requested \a url and its \a mimeType. It can then
    either call acknowledgeOpenUrlRequest to choose from one of the supplied \a possibleAppIds or
    rejectOpenUrlRequest to ignore the request. In both cases the unique \a requestId needs to be
    sent to identify the request.
    Not calling one of these two functions will result in memory leaks.

    \sa openUrl, acknowledgeOpenUrlRequest, rejectOpenUrlRequest
*/

/*!
    \qmlmethod ApplicationManager::acknowledgeOpenUrlRequest(string requestId, string appId)

    Tells the application manager to go ahead with the request to open an URL, identified by \a
    requestId. The chosen \a appId needs to be one of the \c possibleAppIds supplied to the
    receiver of the openUrlRequested signal.

    \sa openUrl, openUrlRequested
*/
void ApplicationManager::acknowledgeOpenUrlRequest(const QString &requestId, const QString &appId)
{
    for (auto it = d->openUrlRequests.cbegin(); it != d->openUrlRequests.cend(); ++it) {
        if (it->requestId == requestId) {
            if (it->possibleAppIds.contains(appId)) {
                try {
                    startApplicationInternal(appId, it->urlStr, it->mimeTypeName);
                } catch (const Exception &e) {
                    qCWarning(LogSystem) << "acknowledgeOpenUrlRequest for" << it->urlStr << "requested app"
                                         << appId << "which could not be started:" << e.errorString();
                }
            } else {
                qCWarning(LogSystem) << "acknowledgeOpenUrlRequest for" << it->urlStr << "requested app"
                                     << appId << "which is not one of the registered possibilities:"
                                     << it->possibleAppIds;
            }
            d->openUrlRequests.erase(it);
            break;
        }
    }
}

/*!
    \qmlmethod ApplicationManager::rejectOpenUrlRequest(string requestId)

    Tells the application manager to ignore the request to open an URL, identified by \a requestId.

    \sa openUrl, openUrlRequested
*/
void ApplicationManager::rejectOpenUrlRequest(const QString &requestId)
{
    for (auto it = d->openUrlRequests.cbegin(); it != d->openUrlRequests.cend(); ++it) {
        if (it->requestId == requestId) {
            d->openUrlRequests.erase(it);
            break;
        }
    }
}

/*!
    \qmlmethod list<string> ApplicationManager::capabilities(string id)

    Returns a list of all capabilities granted by the user to the application identified by \a id.
    Returns an empty list if the application \a id is not valid.
*/
QStringList ApplicationManager::capabilities(const QString &id) const
{
    Application *app = fromId(id);
    return app ? app->capabilities() : QStringList();
}

/*!
    \qmlmethod string ApplicationManager::identifyApplication(int pid)

    Validates the process running with process-identifier \a pid as a process started by the
    application manager.

    \note If multiple applications are running within the same container process, this function
          will return only the first matching application. See identifyAllApplications() for
          a way to retrieve all application ids.

    Returns the application's \c id on success, or an empty string on failure.
*/
QString ApplicationManager::identifyApplication(qint64 pid) const
{
    const auto apps = fromProcessId(pid);
    return !apps.isEmpty() ? apps.constFirst()->id() : QString();
}

/*!
    \qmlmethod list<string> ApplicationManager::identifyAllApplications(int pid)

    Validates the process running with process-identifier \a pid as a process started by the
    application manager.

    If multiple applications are running within the same container process, this function will
    return all those application ids.

    Returns a list with the applications' \c ids on success, or an empty list on failure.
*/
QStringList ApplicationManager::identifyAllApplications(qint64 pid) const
{
    const auto apps = fromProcessId(pid);
    QStringList result;
    result.reserve(apps.size());
    for (const auto &app : apps)
        result << app->id();
    return result;
}

void ApplicationManager::shutDown()
{
    d->shuttingDown = true;
    emit shuttingDownChanged();

    auto shutdownHelper = [this]() {
        bool activeRuntime = false;
        for (Application *app : std::as_const(d->apps)) {
            AbstractRuntime *rt = app->currentRuntime();
            if (rt) {
                activeRuntime = true;
                break;
            }
        }
        if (!activeRuntime)
            emit internalSignals.shutDownFinished();
    };

    for (Application *app : std::as_const(d->apps)) {
        AbstractRuntime *rt = app->currentRuntime();
        if (rt) {
            connect(rt, &AbstractRuntime::destroyed,
                    this, shutdownHelper);
            rt->stop();
        }
    }
    shutdownHelper();
}

void ApplicationManager::openUrlRelay(const QUrl &url)
{
    if (QThread::currentThread() != thread()) {
        staticMetaObject.invokeMethod(this, "openUrlRelay", Qt::QueuedConnection, Q_ARG(QUrl, url));
        return;
    }
    openUrl(url.toString());
}

void ApplicationManager::emitDataChanged(Application *app, const QVector<int> &roles)
{
    auto row = d->apps.indexOf(app);
    if (row >= 0) {
        emit dataChanged(index(int(row)), index(int(row)), roles);

        static const auto appChanged = QMetaMethod::fromSignal(&ApplicationManager::applicationChanged);
        if (isSignalConnected(appChanged)) {
            QStringList stringRoles;
            stringRoles.reserve(roles.size());
            for (auto role : roles)
                stringRoles << QString::fromLatin1(d->roleNames[role]);
            emit applicationChanged(app->id(), stringRoles);
        }
    }
}

void ApplicationManager::emitActivated(Application *app)
{
    emit applicationWasActivated(app->id(), app->id());
    emit app->activated();
}

// item model part

int ApplicationManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return int(d->apps.count());
}

QVariant ApplicationManager::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return QVariant();

    Application *app = d->apps.at(index.row());
    return dataForRole(app, role);
}

QVariant ApplicationManager::dataForRole(Application *app, int role) const
{
    switch (role) {
    case AMRoles::Id:
        return app->id();
    case AMRoles::Name:
        return app->name();
    case AMRoles::Icon:
        return app->icon();
    case AMRoles::IsRunning:
        return app->currentRuntime() ? (app->currentRuntime()->state() == Am::Running) : false;
    case AMRoles::IsStartingUp:
        return app->currentRuntime() ? (app->currentRuntime()->state() == Am::StartingUp) : false;
    case AMRoles::IsShuttingDown:
        return app->currentRuntime() ? (app->currentRuntime()->state() == Am::ShuttingDown) : false;
    case AMRoles::IsBlocked:
        return app->isBlocked();
    case AMRoles::IsUpdating:
        return app->state() != Application::Installed;
    case AMRoles::UpdateProgress:
        return app->progress();
    case AMRoles::IsRemovable:
        return !app->isBuiltIn();
    case AMRoles::CodeFilePath:
        return app->info()->absoluteCodeFilePath();
    case AMRoles::RuntimeName:
        return app->runtimeName();
    case AMRoles::RuntimeParameters:
        return app->runtimeParameters();
    case AMRoles::Capabilities:
        return app->capabilities();
    case AMRoles::Categories:
        return app->categories();
    case AMRoles::Version:
        return app->version();
    case AMRoles::ApplicationItem:
    case AMRoles::ApplicationObject:
        return QVariant::fromValue(app);
    case AMRoles::LastExitCode:
        return app->lastExitCode();
    case AMRoles::LastExitStatus:
        return app->lastExitStatus();
    }
    return QVariant();
}

QHash<int, QByteArray> ApplicationManager::roleNames() const
{
    return d->roleNames;
}

int ApplicationManager::count() const
{
    return rowCount();
}

/*!
    \qmlmethod object ApplicationManager::get(int index)

    Retrieves the model data at \a index as a JavaScript object. See the
    \l {ApplicationManager Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a index is invalid.

    \note This is very inefficient if you only want to access a single property from QML; use
          application() instead to access the Application object's properties directly.
*/
QVariantMap ApplicationManager::get(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "ApplicationManager::get(index): invalid index:" << index;
        return QVariantMap();
    }
    return get(d->apps.at(index));
}

/*!
    \qmlmethod ApplicationObject ApplicationManager::application(int index)

    Returns the \l{ApplicationObject}{application} corresponding to the given \a index in the
    model, or \c null if the index is invalid.

    \note The object ownership of the returned Application object stays with the application manager.
          If you want to store this pointer, you can use the ApplicationManager's QAbstractListModel
          signals or the applicationAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
Application *ApplicationManager::application(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "ApplicationManager::application(index): invalid index:" << index;
        return nullptr;
    }
    return d->apps.at(index);
}

/*!
    \qmlmethod ApplicationObject ApplicationManager::application(string id)

    Returns the \l{ApplicationObject}{application} corresponding to the given application \a id,
    or \c null if the id does not exist.

    \note The object ownership of the returned Application object stays with the application manager.
          If you want to store this pointer, you can use the ApplicationManager's QAbstractListModel
          signals or the applicationAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
Application *ApplicationManager::application(const QString &id) const
{
    auto index = indexOfApplication(id);
    return (index < 0) ? nullptr : application(index);
}

/*!
    \qmlmethod int ApplicationManager::indexOfApplication(string id)

    Maps the application corresponding to the given \a id to its position within the model. Returns
    \c -1 if the specified \a id is invalid.
*/
int ApplicationManager::indexOfApplication(const QString &id) const
{
    for (int i = 0; i < d->apps.size(); ++i) {
        if (d->apps.at(i)->id() == id)
            return i;
    }
    return -1;
}

/*!
    \qmlmethod int ApplicationManager::indexOfApplication(ApplicationObject application)

    Maps the \a application to its position within this model. Returns \c -1 if the specified
    application is invalid.
*/
int ApplicationManager::indexOfApplication(Application *application) const
{
    return int(d->apps.indexOf(application));
}

/*!
    \qmlmethod list<string> ApplicationManager::applicationIds()

    Returns a list of all available application ids. This can be used to further query for specific
    information via get().
*/
QStringList ApplicationManager::applicationIds() const
{
    QStringList ids;
    ids.reserve(d->apps.size());
    for (int i = 0; i < d->apps.size(); ++i)
        ids << d->apps.at(i)->id();
    return ids;
}

/*!
    \qmlmethod object ApplicationManager::get(string id)

    Retrieves the model data for the application identified by \a id as a JavaScript object.
    See the \l {ApplicationManager Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a id is invalid.
*/
QVariantMap ApplicationManager::get(const QString &id) const
{
    return get(application(id));
}

Am::RunState ApplicationManager::applicationRunState(const QString &id) const
{
    int index = indexOfApplication(id);
    return (index < 0) ? Am::NotRunning : d->apps.at(index)->runState();
}

void ApplicationManager::addApplication(ApplicationInfo *appInfo, Package *package)
{
    // check for id clashes outside of the package (the scanner made sure the package itself is
    // consistent and doesn't have duplicates already)
    for (Application *checkApp : std::as_const(d->apps)) {
        if ((checkApp->id() == appInfo->id()) && (checkApp->package() != package)) {
            throw Exception("found an application with the same id in package %1")
                .arg(checkApp->packageInfo()->id());
        }
    }

    auto app = new Application(appInfo, package);
    QQmlEngine::setObjectOwnership(app, QQmlEngine::CppOwnership);

    app->requests.startRequested = [this, app](const QString &documentUrl) {
        return startApplication(app->id(), documentUrl);
    };

    app->requests.debugRequested =  [this, app](const QString &debugWrapper, const QString &documentUrl) {
        return debugApplication(app->id(), debugWrapper, documentUrl);
    };

    app->requests.stopRequested = [this, app](bool forceKill) {
        stopApplication(app->id(), forceKill);
    };

    connect(app, &Application::blockedChanged,
            this, [this, app]() {
        emitDataChanged(app, QVector<int> { AMRoles::IsBlocked });
    });
    connect(app, &Application::bulkChange,
            this, [this, app]() {
        emitDataChanged(app);
    });

    beginInsertRows(QModelIndex(), int(d->apps.count()), int(d->apps.count()));
    d->apps << app;

    endInsertRows();

    registerMimeTypes();

    package->addApplication(app);
    emit applicationAdded(appInfo->id());
}

void ApplicationManager::removeApplication(ApplicationInfo *appInfo, Package *package)
{
    int index = -1;

    for (int i = 0; i < d->apps.size(); ++i) {
        if (d->apps.at(i)->info() == appInfo) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return;

    Q_ASSERT(d->apps.at(index)->package() == package);

    emit applicationAboutToBeRemoved(appInfo->id());

    package->removeApplication(d->apps.at(index));

    beginRemoveRows(QModelIndex(), index, index);
    auto app = d->apps.takeAt(index);

    endRemoveRows();

    registerMimeTypes();

    delete app;
}

QT_END_NAMESPACE_AM

#include "moc_applicationmanager.cpp"
#include "moc_amnamespace.cpp" // amnamespace is header only, so we include it here
