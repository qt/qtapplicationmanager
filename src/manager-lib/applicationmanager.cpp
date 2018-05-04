/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

#include <QCoreApplication>
#include <QUrl>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QTimer>
#include <QUuid>
#include <QThread>
#include <QMimeDatabase>
#if defined(QT_GUI_LIB)
#  include <QDesktopServices>
#endif
#if defined(Q_OS_UNIX)
#  include <signal.h>
#endif

#include "global.h"
#include "applicationinfo.h"
#include "logging.h"
#include "exception.h"
#include "applicationdatabase.h"
#include "applicationmanager.h"
#include "applicationmodel.h"
#include "applicationmanager_p.h"
#include "application.h"
#include "runtimefactory.h"
#include "containerfactory.h"
#include "quicklauncher.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "qml-utilities.h"
#include "utilities.h"
#include "qtyaml.h"
#include "debugwrapper.h"

/*!
    \qmltype ApplicationManager
    \inqmlmodule QtApplicationManager
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
        \li The categories this application is registered for via its meta-data file (currently work in progress).

    \row
        \li \c version
        \li string
        \li The currently installed version of this application.

    \row
        \li \c application
        \li Application
        \li The underlying application object for quick access to the properties outside of a
            model delegate.
    \endtable

    \note The index-based API is currently not available via DBus. However, the same functionality
    is provided by the id-based API.

    After importing, you can just use the ApplicationManager singleton as follows:

    \qml
    import QtQuick 2.0
    import QtApplicationManager 1.0

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

    This property indicates whether the application-manager runs in single- or multi-process mode.
*/

/*!
    \qmlproperty bool ApplicationManager::shuttingDown
    \readonly

    This property is there to inform the System-UI, when the application-manager has entered its
    shutdown phase. New application starts are already prevented, but the System-UI might want
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

    A JavaScript function callback that will be called whenever the application-manager needs to
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
    The \a runState can be one of:

    For example this signal can be used to restart an application in multi-process mode when
    it has crashed:

    \qml
    Connections {
        target: ApplicationManager
        onApplicationRunStateChanged: {
            if (runState === ApplicationManager.NotRunning
                && ApplicationManager.application(id).lastExitStatus === Application.CrashExit) {
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
    ApplicationInstaller. The model has already been update before this signal is sent out.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application-manager
          will also emit these "high-level" signals for System-UIs that cannot work directly on the
          ApplicationManager model: applicationAdded, applicationAboutToBeRemoved and applicationChanged.
*/

/*!
    \qmlsignal ApplicationManager::applicationAboutToBeRemoved(string id)

    This signal is emitted before an existing application, identified by \a id, is removed via the
    ApplicationInstaller.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application-manager
          will also emit these "high-level" signals for System-UIs that cannot work directly on the
          ApplicationManager model: applicationAdded, applicationAboutToBeRemoved and applicationChanged.
*/

/*!
    \qmlsignal ApplicationManager::applicationChanged(string id, list<string> changedRoles)

    Emitted whenever one or more data roles, denoted by \a changedRoles, changed on the application
    identified by \a id. An empty list in the \a changedRoles argument means that all roles should
    be considered modified.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application-manager
          will also emit these "high-level" signals for System-UIs that cannot work directly on the
          ApplicationManager model: applicationAdded, applicationAboutToBeRemoved and applicationChanged.
*/

/*!
    \qmlproperty bool ApplicationManager::windowManagerCompositorReady
    \readonly

    This property starts with the value \c false and will change to \c true once the Wayland
    compositor is ready to accept connections from other processes. Please be aware that this
    will only happen either implicitly after the System-UI's main QML has been loaded or explicitly
    when calling WindowManager::registerCompositorView() from QML before the loading stage has
    completed.
*/

enum Roles
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
    LastExitCode,
    LastExitStatus
};

QT_BEGIN_NAMESPACE_AM

static AbstractApplication::RunState runtimeToApplicationRunState(AbstractRuntime::State rtState)
{
    switch (rtState) {
    case AbstractRuntime::Startup: return AbstractApplication::StartingUp;
    case AbstractRuntime::Active: return AbstractApplication::Running;
    case AbstractRuntime::Shutdown: return AbstractApplication::ShuttingDown;
    default: return AbstractApplication::NotRunning;
    }
}

ApplicationManagerPrivate::ApplicationManagerPrivate()
{
    currentLocale = QLocale::system().name(); //TODO: language changes

    roleNames.insert(Id, "applicationId");
    roleNames.insert(Name, "name");
    roleNames.insert(Icon, "icon");
    roleNames.insert(IsRunning, "isRunning");
    roleNames.insert(IsStartingUp, "isStartingUp");
    roleNames.insert(IsShuttingDown, "isShuttingDown");
    roleNames.insert(IsBlocked, "isBlocked");
    roleNames.insert(IsUpdating, "isUpdating");
    roleNames.insert(IsRemovable, "isRemovable");
    roleNames.insert(UpdateProgress, "updateProgress");
    roleNames.insert(CodeFilePath, "codeFilePath");
    roleNames.insert(RuntimeName, "runtimeName");
    roleNames.insert(RuntimeParameters, "runtimeParameters");
    roleNames.insert(Capabilities, "capabilities");
    roleNames.insert(Categories, "categories");
    roleNames.insert(Version, "version");
    roleNames.insert(ApplicationItem, "application");
    roleNames.insert(LastExitCode, "lastExitCode");
    roleNames.insert(LastExitStatus, "lastExitStatus");
}

ApplicationManagerPrivate::~ApplicationManagerPrivate()
{
    delete database;
    qDeleteAll(apps);
}

ApplicationManager *ApplicationManager::s_instance = nullptr;

ApplicationManager *ApplicationManager::createInstance(ApplicationDatabase *adb, bool singleProcess, QString *error)
{
    if (Q_UNLIKELY(s_instance))
        qFatal("ApplicationManager::createInstance() was called a second time.");

    QScopedPointer<ApplicationManager> am(new ApplicationManager(adb, singleProcess));

    try {
        if (adb) {
            am->d->apps = adb->read();

            // we need to set a valid parent on QObjects that get exposed to QML via
            // Q_INVOKABLE return values -- otherwise QML will take over ownership
            for (auto &app : am->d->apps)
                app->setParent(am.data());
        }
        am->registerMimeTypes();
    } catch (const Exception &e) {
        if (error)
            *error = e.errorString();
        return nullptr;
    }

    qmlRegisterSingletonType<ApplicationManager>("QtApplicationManager", 1, 0, "ApplicationManager",
                                                 &ApplicationManager::instanceForQml);
    qmlRegisterType<ApplicationModel>("QtApplicationManager", 1, 0, "ApplicationModel");
    qmlRegisterUncreatableType<AbstractApplication>("QtApplicationManager", 1, 0, "ApplicationObject",
                                                  qSL("Cannot create objects of type ApplicationObject"));
    qRegisterMetaType<AbstractApplication*>("AbstractApplication*");
    qmlRegisterUncreatableType<AbstractRuntime>("QtApplicationManager", 1, 0, "Runtime",
                                                qSL("Cannot create objects of type Runtime"));
    qRegisterMetaType<AbstractRuntime*>("AbstractRuntime*");
    qmlRegisterUncreatableType<AbstractContainer>("QtApplicationManager", 1, 0, "Container",
                                                  qSL("Cannot create objects of type Container"));
    qRegisterMetaType<AbstractContainer*>("AbstractContainer*");
    qRegisterMetaType<AbstractApplication::RunState>("AbstractApplication::RunState");

    return s_instance = am.take();
}

ApplicationManager *ApplicationManager::instance()
{
    if (!s_instance)
        qFatal("ApplicationManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *ApplicationManager::instanceForQml(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

ApplicationManager::ApplicationManager(ApplicationDatabase *adb, bool singleProcess, QObject *parent)
    : QAbstractListModel(parent)
    , d(new ApplicationManagerPrivate())
{
    d->singleProcess = singleProcess;
    d->database = adb;
    connect(this, &QAbstractItemModel::rowsInserted, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ApplicationManager::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ApplicationManager::countChanged);

    QTimer::singleShot(0, this, &ApplicationManager::preload);
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

void ApplicationManager::setContainerSelectionConfiguration(const QList<QPair<QString, QString>> &containerSelectionConfig)
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

QVector<AbstractApplication *> ApplicationManager::applications() const
{
    return d->apps;
}

AbstractApplication *ApplicationManager::fromId(const QString &id) const
{
    for (AbstractApplication *app : d->apps) {
        if (app->id() == id)
            return app;
    }
    return nullptr;
}

AbstractApplication *ApplicationManager::fromProcessId(qint64 pid) const
{
    // pid could be an indirect child (e.g. when started via gdbserver)
    qint64 appmanPid = QCoreApplication::applicationPid();

    while ((pid > 1) && (pid != appmanPid)) {
        for (AbstractApplication *app : d->apps) {
            if (app->currentRuntime() && (app->currentRuntime()->applicationProcessId() == pid))
                return app;
        }
        pid = getParentPid(pid);
    }
    return nullptr;
}

AbstractApplication *ApplicationManager::fromSecurityToken(const QByteArray &securityToken) const
{
    if (securityToken.size() != AbstractRuntime::SecurityTokenSize)
        return nullptr;

    for (AbstractApplication *app : d->apps) {
        if (app->currentRuntime() && app->currentRuntime()->securityToken() == securityToken)
            return app;
    }
    return nullptr;
}

QVector<AbstractApplication *> ApplicationManager::schemeHandlers(const QString &scheme) const
{
    QVector<AbstractApplication *> handlers;

    for (AbstractApplication *app : d->apps) {
        if (app->isAlias())
            continue;

        const auto mimeTypes = app->supportedMimeTypes();
        for (const QString &mime : mimeTypes) {
            int pos = mime.indexOf(QLatin1Char('/'));

            if ((pos > 0)
                    && (mime.left(pos) == qL1S("x-scheme-handler"))
                    && (mime.mid(pos + 1) == scheme)) {
                handlers << app;
            }
        }
    }
    return handlers;
}

QVector<AbstractApplication *> ApplicationManager::mimeTypeHandlers(const QString &mimeType) const
{
    QVector<AbstractApplication *> handlers;

    for (AbstractApplication *app : d->apps) {
        if (app->isAlias())
            continue;

        if (app->supportedMimeTypes().contains(mimeType))
            handlers << app;
    }
    return handlers;
}

void ApplicationManager::registerMimeTypes()
{
#if defined(QT_GUI_LIB)
    QSet<QString> schemes;
    schemes << qSL("file") << qSL("http") << qSL("https");

    for (AbstractApplication *app : qAsConst(d->apps)) {
        if (app->isAlias())
            continue;

        const auto mimeTypes = app->supportedMimeTypes();
        for (const QString &mime : mimeTypes) {
            int pos = mime.indexOf(QLatin1Char('/'));

            if ((pos > 0) && (mime.left(pos) == qL1S("x-scheme-handler")))
                schemes << mime.mid(pos + 1);
        }
    }
    QSet<QString> registerSchemes = schemes;
    registerSchemes.subtract(d->registeredMimeSchemes);
    QSet<QString> unregisterSchemes = d->registeredMimeSchemes;
    unregisterSchemes.subtract(schemes);

    for (const QString &scheme : qAsConst(unregisterSchemes))
        QDesktopServices::unsetUrlHandler(scheme);
    for (const QString &scheme : qAsConst(registerSchemes))
        QDesktopServices::setUrlHandler(scheme, this, "openUrlRelay");

    d->registeredMimeSchemes = schemes;
#endif
}

bool ApplicationManager::startApplication(AbstractApplication *app, const QString &documentUrl,
                                          const QString &documentMimeType,
                                          const QString &debugWrapperSpecification,
                                          const QVector<int> &stdioRedirections)  Q_DECL_NOEXCEPT_EXPR(false)
{
    if (d->shuttingDown)
        throw Exception("Cannot start applications during shutdown");
    if (!app)
        throw Exception("Cannot start an invalid application");
    if (app->isBlocked())
        throw Exception("Application %1 is blocked - cannot start").arg( app->id());

    Application* realApp = app->nonAliased();

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
            throw Exception("Using debug-wrappers is not supported when the application-manager is running in single-process mode.");
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
        case AbstractRuntime::Startup:
        case AbstractRuntime::Active:
            if (!debugWrapperCommand.isEmpty()) {
                throw Exception("Application %1 is already running - cannot start with debug-wrapper: %2")
                        .arg(app->id(), debugWrapperSpecification);
            }

            if (!documentUrl.isNull())
                runtime->openDocument(documentUrl, documentMimeType);
            else if (!app->documentUrl().isNull())
                runtime->openDocument(app->documentUrl(), documentMimeType);

            emitActivated(app);
            return true;

        case AbstractRuntime::Shutdown:
            return false;

        case AbstractRuntime::Inactive:
            break;
        }
    }

    AbstractContainer *container = nullptr;
    QString containerId;

    if (!inProcess) {
        if (d->containerSelectionConfig.isEmpty()) {
            containerId = qSL("process");
        } else {
            // check config file
            for (const auto &it : qAsConst(d->containerSelectionConfig)) {
                const QString &key = it.first;
                const QString &value = it.second;
                bool hasAsterisk = key.contains(qL1C('*'));

                if ((hasAsterisk && key.length() == 1)
                        || (!hasAsterisk && key == app->id())
                        || QRegExp(key, Qt::CaseSensitive, QRegExp::Wildcard).exactMatch(app->id())) {
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
            else if (!app->runtimeParameters().value(qSL("environmentVariables")).toMap().isEmpty())
                cannotUseQuickLaunch = "the app requests customs environment variables";
            else if (app->nonAliasedInfo()->openGLConfiguration() != runtimeManager->systemOpenGLConfiguration())
                cannotUseQuickLaunch = "the app requests a custom OpenGL configuration";

            if (cannotUseQuickLaunch) {
                qCDebug(LogSystem) << "Cannot use quick-launch for application" << app->id()
                                   << "because" << cannotUseQuickLaunch;
            } else {
                // check quicklaunch pool
                QPair<AbstractContainer *, AbstractRuntime *> quickLaunch =
                        QuickLauncher::instance()->take(containerId, app->nonAliasedInfo()->runtimeName());
                container = quickLaunch.first;
                runtime = quickLaunch.second;

                qCDebug(LogSystem) << "Found a quick-launch entry for container" << containerId
                                   << "and runtime" << app->nonAliasedInfo()->runtimeName() << "->" << container << runtime;

                if (!container && runtime) {
                    runtime->deleteLater();
                    qCCritical(LogSystem) << "ERROR: QuickLauncher provided a runtime without a container.";
                    return false;
                }
            }

            if (!container) {
                container = ContainerFactory::instance()->create(containerId, app, stdioRedirections,
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
        }
        if (!runtime)
            runtime = RuntimeFactory::instance()->create(container, realApp);

        if (runtime && inProcess)  {
            // evil hook to get in-process mode working transparently
            emit inProcessRuntimeCreated(runtime);
        }
    }

    if (!runtime) {
        qCCritical(LogSystem) << "ERROR: Couldn't create Runtime for Application (" << app->id() <<")!";
        return false;
    }

    connect(runtime, &AbstractRuntime::stateChanged, this, [this, app](AbstractRuntime::State newRuntimeState) {
        QVector<AbstractApplication *> apps;
        //Always emit the actual starting app/alias first
        apps.append(app);

        //Add the original app and all aliases
        AbstractApplication *nonAliasedApp = app->nonAliased();

        if (!apps.contains(nonAliasedApp))
            apps.append(nonAliasedApp);

        for (AbstractApplication *alias : qAsConst(d->apps)) {
            if (!apps.contains(alias) && alias->isAlias() && alias->nonAliased() == nonAliasedApp)
                apps.append(alias);
        }

        AbstractApplication::RunState newRunState = runtimeToApplicationRunState(newRuntimeState);

        static_cast<Application*>(nonAliasedApp)->setRunState(newRunState);

        for (AbstractApplication *app : qAsConst(apps)) {
            emit applicationRunStateChanged(app->id(), newRunState);
            emitDataChanged(app, QVector<int> { IsRunning, IsStartingUp, IsShuttingDown });
        }
    });

    if (!documentUrl.isNull())
        runtime->openDocument(documentUrl, documentMimeType);
    else if (!app->documentUrl().isNull())
        runtime->openDocument(app->documentUrl(), documentMimeType);

    emitActivated(app);

    qCDebug(LogSystem) << "Starting application" << app->id() << "in container" << containerId
                       << "using runtime" << runtimeManager->identifier();
    if (!documentUrl.isEmpty())
        qCDebug(LogSystem) << "  documentUrl:" << documentUrl;

    if (inProcess) {
        bool ok = runtime->start();
        if (!ok)
            runtime->deleteLater();
        return ok;
    } else {
        // We can only start the app when both the container and the windowmanager are ready.
        // Using a state-machine would be one option, but then we would need that state-machine
        // object plus the per-app state. Relying on 2 lambdas is the easier choice for now.

        auto doStartInContainer = [realApp, attachRuntime, runtime]() -> bool {
            bool successfullyStarted = attachRuntime ? runtime->attachApplicationToQuickLauncher(realApp)
                                                     : runtime->start();
            if (!successfullyStarted)
                runtime->deleteLater(); // ~Runtime() will clean realApp->m_runtime

            return successfullyStarted;
        };

        auto tryStartInContainer = [container, doStartInContainer]() -> bool {
            if (container->isReady()) {
                // Since the container is already ready, start the app immediately
                return doStartInContainer();
            } else {
                // We postpone the starting of the application to a later point in time,
                // since the container is not ready yet
                connect(container, &AbstractContainer::ready, doStartInContainer);
                return true;
            }
        };

#if defined(AM_HEADLESS)
        bool tryStartNow = true;
#else
        bool tryStartNow = isWindowManagerCompositorReady();
#endif

        if (tryStartNow) {
            return tryStartInContainer();
        } else {
            connect(this, &ApplicationManager::windowManagerCompositorReadyChanged, tryStartInContainer);
            return true;
        }
    }
}

void ApplicationManager::stopApplication(AbstractApplication *app, bool forceKill)
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
*/
bool ApplicationManager::startApplication(const QString &id, const QString &documentUrl)
{
    try {
        return startApplication(fromId(id), documentUrl);
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        return false;
    }
}

/*!
    \qmlmethod bool ApplicationManager::debugApplication(string id, string debugWrapper, string document)

    Instructs the application manager to start the application just like startApplication. The
    application is started via the given \a debugWrapper though. Please see the \l{Debugging} page
    for more information on how to setup and use these debug-wrappers.
*/

bool ApplicationManager::debugApplication(const QString &id, const QString &debugWrapper, const QString &documentUrl)
{
    try {
        return startApplication(fromId(id), documentUrl, QString(), debugWrapper);
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
*/
void ApplicationManager::stopApplication(const QString &id, bool forceKill)
{
    return stopApplication(fromId(id), forceKill);
}

/*!
    \qmlmethod ApplicationManager::stopAllApplications(bool forceKill)

    Tells the application manager to stop all running applications. The meaning of the \a forceKill
    parameter is runtime dependent, but in general you should always try to stop an application
    with \a forceKill set to \c false first in order to allow a clean shutdown.
    Use \a forceKill set to \c true only as a last resort to kill hanging applications.
*/
void ApplicationManager::stopAllApplications(bool forceKill)
{
    for (AbstractApplication *app : qAsConst(d->apps)) {
        if (!app->isAlias()) {
            AbstractRuntime *rt = app->currentRuntime();
            if (rt)
                rt->stop(forceKill);
        }
    }
}

/*!
    \qmlmethod bool ApplicationManager::openUrl(string url)

    Tries start an application that is capable of handling \a url. The application-manager will
    first look at the URL's scheme:
    \list
    \li If it is \c{file:}, the operating system's MIME database will be consulted, which will
        try to find a MIME type match, based on file endings or file content. In case this is
        successful, the application-manager will use this MIME type to find all of its applications
        that claim support for it (see the \l{mimeTypes field} in the application's manifest).
        A music player application that can handle \c mp3 and \c wav files, could add this to its
        manifest:
        \badcode
        mimeTypes: [ 'audio/mpeg', 'audio/wav' ]
        \endcode
    \li If it is something other than \c{file:}, the application-manager will consult its
        internal database of applications that claim support for a matching \c{x-scheme-handler/...}
        MIME type. In order to have your web-browser application handle \c{http:} and \c{https:}
        URLs, you would have to have this in your application's manifest:
        \badcode
        mimeTypes: [ 'x-scheme-handler/http', 'x-scheme-handler/https' ]
        \endcode
    \endlist

    If there is at least one possible match, it depends on the signal openUrlRequested() being
    connected within the System-UI:
    In case the signal is not connected, an arbitrary application from the matching set will be
    started. Otherwise the application-manager will emit the openUrlRequested signal and
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
    // QDesktopServices::openUrl has a special behavior when called recursively, which makes sense
    // on the desktop, but is completely counter-productive for the AM.
    static bool recursionGuard = false;
    if (recursionGuard) {
        QTimer::singleShot(0, this, [urlStr, this]() { openUrl(urlStr); });
        return true; // this is not correct, but the best we can do in this situation
    }
    recursionGuard = true;

    QUrl url(urlStr);
    QString mimeTypeName;
    QVector<AbstractApplication *> apps;

    if (url.isValid()) {
        QString scheme = url.scheme();
        if (scheme != qL1S("file")) {
            apps = schemeHandlers(scheme);

            for (auto it = apps.begin(); it != apps.end(); ++it) {
                AbstractApplication *&app = *it;

                // try to find a better matching alias, if available
                for (AbstractApplication *alias : d->apps) {
                    if (alias->isAlias() && alias->nonAliased() == app) {
                        if (url.toString(QUrl::PrettyDecoded) == alias->documentUrl()) {
                            app = alias;
                            break;
                        }
                    }
                }
            }
        }

        if (apps.isEmpty()) {
            QMimeDatabase mdb;
            QMimeType mt = mdb.mimeTypeForUrl(url);
            mimeTypeName = mt.name();

            apps = mimeTypeHandlers(mimeTypeName);
        }
    }

    if (!apps.isEmpty()) {
        if (!isSignalConnected(QMetaMethod::fromSignal(&ApplicationManager::openUrlRequested))) {
            // If the System-UI does not react to the signal, then just use the first match.
            startApplication(apps.constFirst(), urlStr, mimeTypeName);
        } else {
            ApplicationManagerPrivate::OpenUrlRequest req {
                QUuid::createUuid().toString(),
                urlStr,
                mimeTypeName,
                QStringList()
            };
            for (const auto &app : qAsConst(apps))
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

    This signal is emitted when the application-manager is requested to open an URL. This can happen
    by calling
    \list
    \li Qt.openUrlExternally in an application,
    \li Qt.openUrlExternally in the System-UI,
    \li ApplicationManager::openUrl in the System-UI or
    \li \c io.qt.ApplicationManager.openUrl via D-Bus
    \endlist
    \note This signal is only emitted, if there is a receiver connected at all - see openUrl for the
          fallback behavior.

    The receiver of this signal can inspect the requested \a url an its \a mimeType. It can then
    either call acknowledgeOpenUrlRequest to choose from one of the supplied \a possibleAppIds or
    rejectOpenUrlRequest to ignore the request. In both cases the unique \a requestId needs to be
    sent to identify the request.
    Not calling one of these two functions will result in memory leaks.

    \sa openUrl, acknowledgeOpenUrlRequest, rejectOpenUrlRequest
*/

/*!
    \qmlmethod ApplicationManager::acknowledgeOpenUrlRequest(string requestId, string appId)

    Tells the application-manager to go ahead with the request to open an URL, identified by \a
    requestId. The chosen \a appId needs to be one of the \c possibleAppIds supplied to the
    receiver of the openUrlRequested signal.

    \sa openUrl, openUrlRequested
*/
void ApplicationManager::acknowledgeOpenUrlRequest(const QString &requestId, const QString &appId)
{
    for (auto it = d->openUrlRequests.begin(); it != d->openUrlRequests.end(); ++it) {
        if (it->requestId == requestId) {
            if (it->possibleAppIds.contains(appId)) {
                startApplication(application(appId), it->urlStr, it->mimeTypeName);
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

    Tells the application-manager to ignore the request to open an URL, identified by \a requestId.

    \sa openUrl, openUrlRequested
*/
void ApplicationManager::rejectOpenUrlRequest(const QString &requestId)
{
    for (auto it = d->openUrlRequests.begin(); it != d->openUrlRequests.end(); ++it) {
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
    AbstractApplication *app = fromId(id);
    return app ? app->capabilities() : QStringList();
}

/*!
    \qmlmethod string ApplicationManager::identifyApplication(int pid)

    Validates the process running with process-identifier \a pid as a process started by the
    application manager.

    Returns the application's \c id on success, or an empty string on failure.
*/
QString ApplicationManager::identifyApplication(qint64 pid) const
{
    AbstractApplication *app = fromProcessId(pid);
    return app ? app->id() : QString();
}

bool ApplicationManager::blockApplication(const QString &id)
{
    AbstractApplication *app = fromId(id);
    if (!app)
        return false;
    if (!app->block())
        return false;
    emitDataChanged(app, QVector<int> { IsBlocked });
    stopApplication(app, true);
    emitDataChanged(app, QVector<int> { IsRunning });
    return true;
}

bool ApplicationManager::unblockApplication(const QString &id)
{
    AbstractApplication *app = fromId(id);
    if (!app)
        return false;
    if (!app->unblock())
        return false;
    emitDataChanged(app, QVector<int> { IsBlocked });
    return true;
}

bool ApplicationManager::startingApplicationInstallation(ApplicationInfo *info)
{
    // ownership of info is transferred to ApplicationManager
    QScopedPointer<ApplicationInfo> newInfo(info);

    if (!newInfo || newInfo->id().isEmpty())
        return false;
    AbstractApplication *absApp = fromId(newInfo->id());
    if (!RuntimeFactory::instance()->manager(newInfo->runtimeName()))
        return false;

    if (absApp) { // update
        Q_ASSERT(!absApp->isAlias());
        Application *app = static_cast<Application*>(absApp);

        if (!blockApplication(app->id()))
            return false;

        app->setInfo(newInfo.take());
        app->setState(Application::BeingUpdated);
        app->setProgress(0);
        emitDataChanged(app);
    } else { // installation
        Application *app = new Application(newInfo.take());

        beginInsertRows(QModelIndex(), d->apps.count(), d->apps.count());
        d->apps << app;
        endInsertRows();
        emit applicationAdded(app->id());
        emitDataChanged(app);

        app->block();
        app->setState(Application::BeingInstalled);
        app->setProgress(0);
    }
    return true;
}

bool ApplicationManager::startingApplicationRemoval(const QString &id)
{
    AbstractApplication *absApp = fromId(id);
    if (!absApp)
        return false;

    Q_ASSERT(!absApp->isAlias());

    Application *app = static_cast<Application*>(absApp);
    if (app->isBlocked() || (app->state() != Application::Installed))
        return false;

    if (app->isBuiltIn())
        return false;

    if (!blockApplication(id))
        return false;

    app->setState(Application::BeingRemoved);
    app->setProgress(0);
    emitDataChanged(app, QVector<int> { IsUpdating });
    return true;
}

void ApplicationManager::progressingApplicationInstall(const QString &id, qreal progress)
{
    AbstractApplication *absApp = fromId(id);
    if (!absApp)
        return;

    Q_ASSERT(!absApp->isAlias());
    Application *app = static_cast<Application*>(absApp);

    if (app->state() == Application::Installed)
        return;
    app->setProgress(progress);
    emitDataChanged(app, QVector<int> { UpdateProgress });
}

bool ApplicationManager::finishedApplicationInstall(const QString &id)
{
    AbstractApplication *absApp = fromId(id);
    if (!absApp)
        return false;

    Q_ASSERT(!absApp->isAlias());
    Application *app = static_cast<Application*>(absApp);

    switch (app->state()) {
    case Application::Installed:
        return false;

    case Application::BeingInstalled:
    case Application::BeingUpdated: {
        // The Application object has been updated right at the start of the installation/update.
        // Now's the time to update the InstallationReport that was written by the installer.
        QFile irfile(QDir(app->nonAliasedInfo()->manifestDir()).absoluteFilePath(qSL("installation-report.yaml")));
        QScopedPointer<InstallationReport> ir(new InstallationReport(app->id()));
        if (!irfile.open(QFile::ReadOnly) || !ir->deserialize(&irfile)) {
            qCCritical(LogInstaller) << "Could not read the new installation-report for application"
                                     << app->id() << "at" << irfile.fileName();
            return false;
        }
        app->nonAliasedInfo()->setInstallationReport(ir.take());
        registerMimeTypes();
        app->setState(Application::Installed);
        app->setProgress(0);

        try {
            if (d->database)
                d->database->write(d->apps);
        } catch (const Exception &e) {
            qCCritical(LogInstaller) << "ERROR: Application" << app->id() << "was installed, but writing the "
                                        "updated application database to disk failed:" << e.errorString();
            d->database->invalidate(); // make sure that the next AM start will re-read the DB
        }
        emitDataChanged(app);

        unblockApplication(id);
        emit app->bulkChange(); // not ideal, but icon and codeDir have changed
        break;
    }
    case Application::BeingRemoved: {
        int row = d->apps.indexOf(app);
        if (row >= 0) {
            emit applicationAboutToBeRemoved(app->id());
            beginRemoveRows(QModelIndex(), row, row);
            d->apps.removeAt(row);
            endRemoveRows();
        }
        delete app;
        registerMimeTypes();
        try {
            if (d->database)
                d->database->write(d->apps);
        } catch (const Exception &e) {
            qCCritical(LogInstaller) << "ERROR: Application" << app->id() << "was removed, but writing the "
                                        "updated application database to disk failed:" << e.errorString();
            d->database->invalidate(); // make sure that the next AM start will re-read the DB
            return false;
        }
        break;
    }
    }

    return true;
}

bool ApplicationManager::canceledApplicationInstall(const QString &id)
{
    AbstractApplication *absApp = fromId(id);
    if (!absApp)
        return false;

    Q_ASSERT(!absApp->isAlias());
    Application *app = static_cast<Application*>(absApp);

    switch (app->state()) {
    case Application::Installed:
        return false;

    case Application::BeingInstalled: {
        int row = d->apps.indexOf(app);
        if (row >= 0) {
            emit applicationAboutToBeRemoved(app->id());
            beginRemoveRows(QModelIndex(), row, row);
            d->apps.removeAt(row);
            endRemoveRows();
        }
        delete app;
        break;
    }
    case Application::BeingUpdated:
    case Application::BeingRemoved:
        app->setState(Application::Installed);
        app->setProgress(0);
        emitDataChanged(app, QVector<int> { IsUpdating });

        unblockApplication(id);
        break;
    }
    return true;
}

void ApplicationManager::preload()
{
    bool singleAppMode = d->database && d->database->isTemporary();

    for (AbstractApplication *app : qAsConst(d->apps)) {
        if (singleAppMode) {
            if (!startApplication(app)) {
                if (!singleAppMode)
                    qCWarning(LogSystem) << "WARNING: unable to start preload-enabled application" << app->id();
                else
                    QMetaObject::invokeMethod(qApp, "shutDown", Qt::DirectConnection, Q_ARG(int, 1));
            } else if (singleAppMode) {
                connect(this, &ApplicationManager::applicationRunStateChanged, [app](const QString &id, Application::RunState runState) {
                    if ((id == app->id()) && (runState == Application::NotRunning)) {
                        QMetaObject::invokeMethod(qApp, "shutDown", Qt::DirectConnection,
                                                  Q_ARG(int, app->lastExitCode()));
                    }
                });
            }
        }
    }
}

void ApplicationManager::shutDown()
{
    d->shuttingDown = true;
    emit shuttingDownChanged();

    auto shutdownHelper = [this]() {
        bool activeRuntime = false;
        for (AbstractApplication *app : qAsConst(d->apps)) {
            AbstractRuntime *rt = app->currentRuntime();
            if (rt) {
                activeRuntime = true;
                break;
            }
        }
        if (!activeRuntime)
            emit shutDownFinished();
    };

    for (AbstractApplication *app : qAsConst(d->apps)) {
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

void ApplicationManager::emitDataChanged(AbstractApplication *app, const QVector<int> &roles)
{
    int row = d->apps.indexOf(app);
    if (row >= 0) {
        emit dataChanged(index(row), index(row), roles);

        static const auto appChanged = QMetaMethod::fromSignal(&ApplicationManager::applicationChanged);
        if (isSignalConnected(appChanged)) {
            QStringList stringRoles;
            for (auto role : roles)
                stringRoles << qL1S(d->roleNames[role]);
            emit applicationChanged(app->id(), stringRoles);
        }
    }
}

void ApplicationManager::emitActivated(AbstractApplication *app)
{
    emit applicationWasActivated(app->isAlias() ? app->nonAliased()->id() : app->id(), app->id());
    emit app->activated();
}

// item model part

int ApplicationManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->apps.count();
}

QVariant ApplicationManager::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return QVariant();

    AbstractApplication *app = d->apps.at(index.row());

    switch (role) {
    case Id:
        return app->id();
    case Name: {
        QString name;
        if (!app->info()->names().isEmpty()) {
            name = app->info()->name(d->currentLocale);
            if (name.isEmpty())
                name = app->info()->name(qSL("en"));
            if (name.isEmpty())
                name = app->info()->name(qSL("en_US"));
            if (name.isEmpty())
                name = *app->info()->names().constBegin();
        } else {
            name = app->id();
        }
        return name;
    }
    case Icon:
        return app->icon();

    case IsRunning:
        return app->currentRuntime() ? (app->currentRuntime()->state() == AbstractRuntime::Active) : false;
    case IsStartingUp:
        return app->currentRuntime() ? (app->currentRuntime()->state() == AbstractRuntime::Startup) : false;
    case IsShuttingDown:
        return app->currentRuntime() ? (app->currentRuntime()->state() == AbstractRuntime::Shutdown) : false;
    case IsBlocked:
        return app->isBlocked();
    case IsUpdating:
        return app->state() != Application::Installed;
    case UpdateProgress:
        return app->progress();
    case IsRemovable:
        return !app->isBuiltIn();

    case CodeFilePath:
        return app->nonAliasedInfo()->absoluteCodeFilePath();
    case RuntimeName:
        return app->runtimeName();
    case RuntimeParameters:
        return app->runtimeParameters();
    case Capabilities:
        return app->capabilities();
    case Categories:
        return app->categories();
    case Version:
        return app->version();
    case ApplicationItem:
        return QVariant::fromValue(app);
    case LastExitCode:
        return app->lastExitCode();
    case LastExitStatus:
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
    \qmlmethod object ApplicationManager::get(int row)

    Retrieves the model data at \a row as a JavaScript object. See the
    \l {ApplicationManager Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a row is invalid.

    \note This is very inefficient if you only want to access a single property from QML; use
          application() instead to access the Application object's properties directly.
*/
QVariantMap ApplicationManager::get(int row) const
{
    if (row < 0 || row >= count()) {
        qCWarning(LogSystem) << "invalid index:" << row;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(qL1S(it.value()), data(index(row), it.key()));
    return map;
}

/*!
    \qmlmethod Application ApplicationManager::application(int index)

    Returns the Application object corresponding to the given \a index in the model,
    or \c null if the index is invalid.

    \note The object ownership of the returned Application object stays with the application-manager.
          If you want to store this pointer, you can use the ApplicationManager's QAbstractListModel
          signals or the applicationAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
AbstractApplication *ApplicationManager::application(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "invalid index:" << index;
        return nullptr;
    }
    return d->apps.at(index);
}

/*!
    \qmlmethod Application ApplicationManager::application(string id)

    Returns the Application object corresponding to the given application \a id,
    or \c null if the id does not exist.

    \note The object ownership of the returned Application object stays with the application-manager.
          If you want to store this pointer, you can use the ApplicationManager's QAbstractListModel
          signals or the applicationAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
AbstractApplication *ApplicationManager::application(const QString &id) const
{
    return application(indexOfApplication(id));
}

/*!
    \qmlmethod int ApplicationManager::indexOfApplication(string id)

    Maps the application \a id to its position within the model.

    Returns \c -1 if the specified \a id is invalid.
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
    auto map = get(indexOfApplication(id));
    return map;
}

Application::RunState ApplicationManager::applicationRunState(const QString &id) const
{
    int index = indexOfApplication(id);
    if (index < 0) {
        qCWarning(LogSystem) << "invalid index:" << index;
        return Application::NotRunning;
    }
    return d->apps.at(index)->runState();
}

QT_END_NAMESPACE_AM
