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

#include <QCoreApplication>
#include <QUrl>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QTimer>
#include <QUuid>
#include <QMimeDatabase>
#if defined(QT_GUI_LIB)
#  include <QDesktopServices>
#endif
#include <qplatformdefs.h>

#include "global.h"
#include "logging.h"
#include "exception.h"
#include "applicationdatabase.h"
#include "applicationmanager.h"
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

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <signal.h>
#endif

#define AM_AUTHENTICATE_DBUS(RETURN_TYPE) \
    do { \
        if (!checkDBusPolicy(this, d->dbusPolicy, __FUNCTION__, [](qint64 pid) -> QStringList { return ApplicationManager::instance()->capabilities(ApplicationManager::instance()->identifyApplication(pid)); })) \
            return RETURN_TYPE(); \
    } while (false);


/*!
    \qmltype ApplicationManager
    \inqmlmodule QtApplicationManager
    \brief The ApplicationManager singleton.

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

    \list
    \li ApplicationManager.NotRunning - the application has not been started yet
    \li ApplicationManager.StartingUp - the application has been started and is initializing
    \li ApplicationManager.Running - the application is running
    \li ApplicationManager.ShuttingDown - the application has been stopped and is cleaning up (in
                                          multi-process mode this signal is only emitted if the
                                          application terminates gracefully)
    \endlist

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

    Emitted whenever one or more data roles, denoted by \a changedRole, changed on the application
    identified by \a id.

    \note In addition to the normal "low-level" QAbstractListModel signals, the application-manager
          will also emit these "high-level" signals for System-UIs that cannot work directly on the
          ApplicationManager model: applicationAdded, applicationAboutToBeRemoved and applicationChanged.
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
    BackgroundMode,
    Capabilities,
    Categories,
    Importance,
    Preload,
    Version,
    ApplicationItem,
    LastExitCode,
    LastExitStatus
};

QT_BEGIN_NAMESPACE_AM

static ApplicationManager::RunState runtimeToManagerState(AbstractRuntime::State rtState)
{
    switch (rtState) {
    case AbstractRuntime::Startup: return ApplicationManager::StartingUp;
    case AbstractRuntime::Active: return ApplicationManager::Running;
    case AbstractRuntime::Shutdown: return ApplicationManager::ShuttingDown;
    default: return ApplicationManager::NotRunning;
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
    roleNames.insert(IsBlocked, "isLocked");
    roleNames.insert(IsUpdating, "isUpdating");
    roleNames.insert(IsRemovable, "isRemovable");
    roleNames.insert(UpdateProgress, "updateProgress");
    roleNames.insert(CodeFilePath, "codeFilePath");
    roleNames.insert(RuntimeName, "runtimeName");
    roleNames.insert(RuntimeParameters, "runtimeParameters");
    roleNames.insert(BackgroundMode, "backgroundMode");
    roleNames.insert(Capabilities, "capabilities");
    roleNames.insert(Categories, "categories");
    roleNames.insert(Importance, "importance");
    roleNames.insert(Preload, "preload");
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
                const_cast<Application *>(app)->setParent(am.data());
        }
        am->registerMimeTypes();
    } catch (const Exception &e) {
        if (error)
            *error = e.errorString();
        return nullptr;
    }

    qmlRegisterSingletonType<ApplicationManager>("QtApplicationManager", 1, 0, "ApplicationManager",
                                                 &ApplicationManager::instanceForQml);
    qmlRegisterUncreatableType<const Application>("QtApplicationManager", 1, 0, "Application",
                                                  qSL("Cannot create objects of type Application"));
    qRegisterMetaType<const Application*>("const Application*");
    qmlRegisterUncreatableType<AbstractRuntime>("QtApplicationManager", 1, 0, "Runtime",
                                                qSL("Cannot create objects of type Runtime"));
    qRegisterMetaType<AbstractRuntime*>("AbstractRuntime*");
    qmlRegisterUncreatableType<AbstractContainer>("QtApplicationManager", 1, 0, "Container",
                                                  qSL("Cannot create objects of type Container"));
    qRegisterMetaType<AbstractContainer*>("AbstractContainer*");

    return s_instance = am.take();
}

ApplicationManager *ApplicationManager::instance()
{
    if (!s_instance)
        qFatal("ApplicationManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *ApplicationManager::instanceForQml(QQmlEngine *qmlEngine, QJSEngine *)
{
    if (qmlEngine)
        retakeSingletonOwnershipFromQmlEngine(qmlEngine, instance());
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

void ApplicationManager::setDebugWrapperConfiguration(const QVariantList &debugWrappers)
{
    // Example:
    //    debugWrappers:
    //    - name: gdbserver
    //      # %program% and %arguments% are internal variables
    //      command: [ /usr/bin/gdbserver, ':%port%', '%program%', '%arguments%' ]
    //      parameters:  # <name>: <default value>
    //        port: 5555
    //      supportedRuntimes: [ native, qml ]
    //      supportedContainers: [ process ]
    //    - name: valgrind
    //      command: [ /usr/bin/valgrind, '--', '%program%', '%arguments%' ]
    //      parameters:  # <name>: <default value>

    for (const QVariant &v : debugWrappers) {
        const QVariantMap &map = v.toMap();

        ApplicationManagerPrivate::DebugWrapper dw;
        dw.name = map.value(qSL("name")).toString();
        dw.command = map.value(qSL("command")).toStringList();
        dw.parameters = map.value(qSL("parameters")).toMap();
        dw.supportedContainers = map.value(qSL("supportedContainers")).toStringList();
        dw.supportedRuntimes = map.value(qSL("supportedRuntimes")).toStringList();

        if (dw.name.isEmpty() || dw.command.isEmpty()) {
            qCWarning(LogSystem) << "Ignoring invalid debug wrapper: " << dw.name << "/" << dw.command;
            continue;
        }
        d->debugWrappers.append(dw);
    }
}

ApplicationManagerPrivate::DebugWrapper ApplicationManagerPrivate::parseDebugWrapperSpecification(const QString &spec)
{
    // Example:
    //    "gdbserver"
    //    "gdbserver: {port: 5555}"

    static ApplicationManagerPrivate::DebugWrapper fail;

    if (spec.isEmpty())
        return fail;

    auto docs = QtYaml::variantDocumentsFromYaml(spec.toUtf8());
    if (docs.size() != 1)
        return fail;
    const QVariant v = docs.at(0);

    QString name;
    QVariantMap userParams;

    switch (v.type()) {
    case QVariant::String:
        name = v.toString();
        break;
    case QVariant::Map: {
        const QVariantMap map = v.toMap();
        name = map.firstKey();
        userParams = map.first().toMap();
        break;
    }
    default:
        return fail;
    }

    for (const auto &it : qAsConst(debugWrappers)) {
        if (it.name == name) {
            ApplicationManagerPrivate::DebugWrapper dw = it;
            for (auto it = userParams.cbegin(); it != userParams.cend(); ++it) {
                auto pit = dw.parameters.find(it.key());
                if (pit == dw.parameters.end())
                    return fail;
                *pit = it.value();
            }

            for (auto pit = dw.parameters.cbegin(); pit != dw.parameters.cend(); ++pit) {
                QString key = qL1C('%') + pit.key() + qL1C('%');
                QString value = pit.value().toString();

                // replace variable name with value in command line
                std::for_each(dw.command.begin(), dw.command.end(), [key, value](QString &str) {
                    str.replace(key, value);
                });
            }
            return dw;
        }
    }
    return fail;
}

bool ApplicationManager::setDBusPolicy(const QVariantMap &yamlFragment)
{
    static const QVector<QByteArray> functions {
        QT_STRINGIFY(applicationIds),
        QT_STRINGIFY(get),
        QT_STRINGIFY(startApplication),
        QT_STRINGIFY(debugApplication),
        QT_STRINGIFY(stopApplication),
        QT_STRINGIFY(stopAllApplications),
        QT_STRINGIFY(openUrl),
        QT_STRINGIFY(capabilities),
        QT_STRINGIFY(identifyApplication),
        QT_STRINGIFY(applicationState)
    };

    d->dbusPolicy = parseDBusPolicy(yamlFragment);

    for (auto it = d->dbusPolicy.cbegin(); it != d->dbusPolicy.cend(); ++it) {
       if (!functions.contains(it.key()))
           return false;
    }
    return true;
}

QVector<const Application *> ApplicationManager::applications() const
{
    return d->apps;
}

const Application *ApplicationManager::fromId(const QString &id) const
{
    for (const Application *app : d->apps) {
        if (app->id() == id)
            return app;
    }
    return 0;
}

const Application *ApplicationManager::fromProcessId(qint64 pid) const
{
    // pid could be an indirect child (e.g. when started via gdbserver)
    qint64 appmanPid = QCoreApplication::applicationPid();

    while ((pid > 1) && (pid != appmanPid)) {
        for (const Application *app : d->apps) {
            if (app->currentRuntime() && (app->currentRuntime()->applicationProcessId() == pid))
                return app;
        }
        pid = getParentPid(pid);
    }
    return nullptr;
}

const Application *ApplicationManager::fromSecurityToken(const QByteArray &securityToken) const
{
    if (securityToken.size() != AbstractRuntime::SecurityTokenSize)
        return 0;

    for (const Application *app : d->apps) {
        if (app->currentRuntime() && app->currentRuntime()->securityToken() == securityToken)
            return app;
    }
    return 0;
}

QVector<const Application *> ApplicationManager::schemeHandlers(const QString &scheme) const
{
    QVector<const Application *> handlers;

    for (const Application *app : d->apps) {
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

QVector<const Application *> ApplicationManager::mimeTypeHandlers(const QString &mimeType) const
{
    QVector<const Application *> handlers;

    for (const Application *app : d->apps) {
        if (app->isAlias())
            continue;

        if (app->supportedMimeTypes().contains(mimeType))
            handlers << app;
    }
    return handlers;
}

void ApplicationManager::registerMimeTypes()
{
    QVector<QString> schemes;
    schemes << qSL("file") << qSL("http") << qSL("https");

    for (const Application *app : qAsConst(d->apps)) {
        if (app->isAlias())
            continue;

        const auto mimeTypes = app->supportedMimeTypes();
        for (const QString &mime : mimeTypes) {
            int pos = mime.indexOf(QLatin1Char('/'));

            if ((pos > 0) && (mime.left(pos) == qL1S("x-scheme-handler")))
                schemes << mime.mid(pos + 1);
        }
    }
#if defined(QT_GUI_LIB)
    for (const QString &scheme : qAsConst(schemes))
        QDesktopServices::setUrlHandler(scheme, this, "openUrlRelay");
#endif
}

bool ApplicationManager::startApplication(const Application *app, const QString &documentUrl,
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

    AbstractRuntime *runtime = app->currentRuntime();

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
    ApplicationManagerPrivate::DebugWrapper debugWrapper;
    if (!debugWrapperSpecification.isEmpty()) {
        debugWrapper = d->parseDebugWrapperSpecification(debugWrapperSpecification);
        if (!debugWrapper.isValid()) {
            throw Exception("Tried to start application %1 using an invalid debug-wrapper specification: %2")
                    .arg(app->id(), debugWrapperSpecification);
        }
    }

    if (runtime) {
        switch (runtime->state()) {
        case AbstractRuntime::Startup:
        case AbstractRuntime::Active:
            if (debugWrapper.isValid()) {
                throw Exception("Application %1 is already running - cannot start with debug-wrapper: %2")
                        .arg(app->id(), debugWrapper.name);
            }

            if (!documentUrl.isNull())
                runtime->openDocument(documentUrl, documentMimeType);
            else if (!app->documentUrl().isNull())
                runtime->openDocument(app->documentUrl(), documentMimeType);

            emit applicationWasActivated(app->isAlias() ? app->nonAliased()->id() : app->id(), app->id());
            return true;

        case AbstractRuntime::Shutdown:
            return false;

        case AbstractRuntime::Inactive:
            break;
        }
    }

    auto runtimeManager = RuntimeFactory::instance()->manager(app->runtimeName());
    if (!runtimeManager)
        throw Exception("No RuntimeManager found for runtime: %1").arg(app->runtimeName());

    bool inProcess = runtimeManager->inProcess();
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

    if (debugWrapper.isValid()) {
        if (!debugWrapper.supportedRuntimes.contains(app->runtimeName())) {
            throw Exception("Application %1 is using the %2 runtime, which is not compatible with the requested debug-wrapper: %3")
                    .arg(app->id(), app->runtimeName(), debugWrapper.name);
        }
        if (!debugWrapper.supportedContainers.contains(containerId)) {
            throw Exception("Application %1 is using the %2 container, which is not compatible with the requested debug-wrapper: %3")
                    .arg(app->id(), containerId, debugWrapper.name);
        }
    }

    if (!runtime) {
        if (!inProcess) {
            // we cannot use the quicklaunch pool, if
            //  (a) a debug-wrapper is being used,
            //  (b) stdio is redirected or
            //  (c) the app requests special environment variables
            bool cannotUseQuickLaunch = debugWrapper.isValid()
                    || hasStdioRedirections
                    || !app->environmentVariables().isEmpty();

            if (!cannotUseQuickLaunch) {
                // check quicklaunch pool
                QPair<AbstractContainer *, AbstractRuntime *> quickLaunch =
                        QuickLauncher::instance()->take(containerId, app->m_runtimeName);
                container = quickLaunch.first;
                runtime = quickLaunch.second;

                qCDebug(LogSystem) << "Found a quick-launch entry for container" << containerId
                                   << "and runtime" << app->m_runtimeName << "->" << container << runtime;

                if (!container && runtime) {
                    runtime->deleteLater();
                    qCCritical(LogSystem) << "ERROR: QuickLauncher provided a runtime without a container.";
                    return false;
                }
            }

            if (!container) {
                container = ContainerFactory::instance()->create(containerId, app, stdioRedirections,
                                                                 debugWrapper.command);
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
            runtime = RuntimeFactory::instance()->create(container, app);

        if (runtime && inProcess)  {
            // evil hook to get in-process mode working transparently
            emit inProcessRuntimeCreated(runtime);
        }
    }

    if (!runtime) {
        qCCritical(LogSystem) << "ERROR: Couldn't create Runtime for Application (" << app->id() <<")!";
        return false;
    }

    connect(runtime, &AbstractRuntime::stateChanged, this, [this, app](AbstractRuntime::State newState) {
        QVector<const Application *> apps;
        //Always emit the actual starting app/alias first
        apps.append(app);

        //Add the original app and all aliases
        const Application *nonAliasedApp = app;
        if (app->isAlias())
            nonAliasedApp = app->nonAliased();

        if (!apps.contains(nonAliasedApp))
            apps.append(nonAliasedApp);

        for (const Application *alias : qAsConst(d->apps)) {
            if (!apps.contains(alias) && alias->isAlias() && alias->nonAliased() == nonAliasedApp)
                apps.append(alias);
        }

        for (const Application *app : qAsConst(apps))
            emit applicationRunStateChanged(app->id(), runtimeToManagerState(newState));
    });

    connect(runtime, static_cast<void(AbstractRuntime::*)(int, QProcess::ExitStatus)>
            (&AbstractRuntime::finished), this, [app](int code, QProcess::ExitStatus status) {
        if (code != app->m_lastExitCode) {
            app->m_lastExitCode = code;
            emit app->lastExitCodeChanged();
        }

        Application::ExitStatus newStatus;
        if (status == QProcess::CrashExit) {
#if defined(Q_OS_UNIX)
            newStatus = (code == SIGTERM || code == SIGKILL) ? Application::ForcedExit : Application::CrashExit;
#else
            newStatus = Application::CrashExit;
#endif
        } else {
            newStatus = Application::NormalExit;
        }
        if (newStatus != app->m_lastExitStatus) {
            app->m_lastExitStatus = newStatus;
            emit app->lastExitStatusChanged();
        }
    });

    if (!documentUrl.isNull())
        runtime->openDocument(documentUrl, documentMimeType);
    else if (!app->documentUrl().isNull())
        runtime->openDocument(app->documentUrl(), documentMimeType);

    emit applicationWasActivated(app->isAlias() ? app->nonAliased()->id() : app->id(), app->id());

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
        auto startInContainer = [app, attachRuntime, runtime]() {
            bool successfullyStarted = attachRuntime ? runtime->attachApplicationToQuickLauncher(app)
                                                     : runtime->start();
            if (!successfullyStarted)
                runtime->deleteLater(); // ~Runtime() will clean app->m_runtime

            return successfullyStarted;
        };

        if (container->isReady()) {
            // Since the container is already ready, start the app immediately
            return startInContainer();
        }
        else {
            // We postpone the starting of the application to a later point in time since the container is not ready yet
            connect(container, &AbstractContainer::ready, startInContainer);
            return true;       // we return true for now, since we don't know at this point in time whether the container will be able to start the application. TODO : fix
        }
    }
}

void ApplicationManager::stopApplication(const Application *app, bool forceKill)
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
    AM_AUTHENTICATE_DBUS(bool)

    try {
        return startApplication(fromId(id), documentUrl);
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        if (calledFromDBus())
            sendErrorReply(qL1S("org.freedesktop.DBus.Error.Failed"), e.errorString());
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
    AM_AUTHENTICATE_DBUS(bool)

    try {
        return startApplication(fromId(id), documentUrl, QString(), debugWrapper);
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        if (calledFromDBus())
            sendErrorReply(qL1S("org.freedesktop.DBus.Error.Failed"), e.errorString());
        return false;
    }
}

#if defined(QT_DBUS_LIB)
bool ApplicationManager::startApplication(const QString &id, const QT_PREPEND_NAMESPACE_AM(UnixFdMap) &redirections, const QString &documentUrl)
{
    AM_AUTHENTICATE_DBUS(bool)

    QVector<int> stdioRedirections = { -1, -1, -1 };

#  if defined(Q_OS_UNIX)
    for (auto it = redirections.cbegin(); it != redirections.cend(); ++it) {
        QDBusUnixFileDescriptor dfd = it.value();
        int fd = dfd.fileDescriptor();

        const QString &which = it.key();
        if (which == qL1S("in"))
            stdioRedirections[0] = dup(fd);
        else if (which == qL1S("out"))
            stdioRedirections[1] = dup(fd);
        else if (which == qL1S("err"))
            stdioRedirections[2] = dup(fd);
    }
#  else
    Q_UNUSED(redirections)
#  endif // defined(Q_OS_UNIX)
    int result = false;
    try {
        result = startApplication(fromId(id), documentUrl, QString(), QString(), stdioRedirections);
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        if (calledFromDBus())
            sendErrorReply(qL1S("org.freedesktop.DBus.Error.Failed"), e.errorString());
        result = false;
    }

    if (!result) {
        // we have to close the fds in this case, otherwise we block the tty where the fds are
        // originating from.
        //TODO: this really needs to fixed centrally (e.g. via the DebugWrapper), but this is the most
        //      common error case for now.
        for (int fd : qAsConst(stdioRedirections)) {
            if (fd >= 0)
                QT_CLOSE(fd);
        }
    }
    return result;
}

bool ApplicationManager::debugApplication(const QString &id, const QString &debugWrapper, const QT_PREPEND_NAMESPACE_AM(UnixFdMap) &redirections, const QString &documentUrl)
{
    AM_AUTHENTICATE_DBUS(bool)

    QVector<int> stdioRedirections = { -1, -1, -1 };

#  if defined(Q_OS_UNIX)
    for (auto it = redirections.cbegin(); it != redirections.cend(); ++it) {
        QDBusUnixFileDescriptor dfd = it.value();
        int fd = dfd.fileDescriptor();

        const QString &which = it.key();
        if (which == qL1S("in"))
            stdioRedirections[0] = dup(fd);
        else if (which == qL1S("out"))
            stdioRedirections[1] = dup(fd);
        else if (which == qL1S("err"))
            stdioRedirections[2] = dup(fd);
    }
#  else
    Q_UNUSED(redirections)
#  endif // defined(Q_OS_UNIX)
    int result = false;
    try {
        result = startApplication(fromId(id), documentUrl, QString(), debugWrapper, stdioRedirections);
    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        if (calledFromDBus())
            sendErrorReply(qL1S("org.freedesktop.DBus.Error.Failed"), e.errorString());
        result = false;
    }

    if (!result) {
        // we have to close the fds in this case, otherwise we block the tty where the fds are
        // originating from.
        //TODO: this really needs to fixed centrally (e.g. via the DebugWrapper), but this is the most
        //      common error case for now.
        for (int fd : qAsConst(stdioRedirections)) {
            if (fd >= 0)
                QT_CLOSE(fd);
        }
    }
    return result;
}
#endif // defined(QT_DBUS_LIB)

/*!
    \qmlmethod ApplicationManager::stopApplication(string id, bool forceKill)

    Tells the application manager to stop an application identified by its unique \a id. The
    meaning of the \a forceKill parameter is runtime dependent, but in general you should always try
    to stop an application with \a forceKill set to \c false first in order to allow a clean
    shutdown. Use \a forceKill set to \c true only as a last resort to kill hanging applications.
*/
void ApplicationManager::stopApplication(const QString &id, bool forceKill)
{
    AM_AUTHENTICATE_DBUS(void)

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
    AM_AUTHENTICATE_DBUS(void)

    for (const Application *app : qAsConst(d->apps)) {
        AbstractRuntime *rt = app->currentRuntime();
        if (rt)
            rt->stop(forceKill);
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
    AM_AUTHENTICATE_DBUS(bool)

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
    QVector<const Application *> apps;

    if (url.isValid()) {
        QString scheme = url.scheme();
        if (scheme != qL1S("file")) {
            apps = schemeHandlers(scheme);

            for (auto it = apps.begin(); it != apps.end(); ++it) {
                const Application *&app = *it;

                // try to find a better matching alias, if available
                for (const Application *alias : d->apps) {
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
            // If the system-ui does not react to the signal, then just use the first match.
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
    AM_AUTHENTICATE_DBUS(QStringList)

    const Application *app = fromId(id);
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
    AM_AUTHENTICATE_DBUS(QString)

    const Application *app = fromProcessId(pid);
    return app ? app->id() : QString();
}

bool ApplicationManager::blockApplication(const QString &id)
{
    const Application *app = fromId(id);
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
    const Application *app = fromId(id);
    if (!app)
        return false;
    if (!app->unblock())
        return false;
    emitDataChanged(app, QVector<int> { IsBlocked });
    return true;
}

bool ApplicationManager::startingApplicationInstallation(Application *installApp)
{
    // ownership of installApp is transferred to ApplicationManager
    QScopedPointer<Application> newapp(installApp);

    if (!newapp || newapp->id().isEmpty())
        return false;
    const Application *app = fromId(newapp->id());
    if (!RuntimeFactory::instance()->manager(newapp->runtimeName()))
        return false;

    if (app) { // update
        if (!blockApplication(app->id()))
            return false;
        newapp->mergeInto(const_cast<Application *>(app));
        app->m_state = Application::BeingUpdated;
        app->m_progress = 0;
    } else { // installation
        newapp->setParent(this);
        newapp->block();
        newapp->m_state = Application::BeingInstalled;
        newapp->m_progress = 0;
        app = newapp.take();
        beginInsertRows(QModelIndex(), d->apps.count(), d->apps.count());
        d->apps << app;
        endInsertRows();
        emit applicationAdded(app->id());
    }
    emitDataChanged(app);
    return true;
}

bool ApplicationManager::startingApplicationRemoval(const QString &id)
{
    const Application *app = fromId(id);
    if (!app || app->isBlocked() || (app->m_state != Application::Installed))
        return false;

    if (app->isBuiltIn())
        return false;

    if (!blockApplication(id))
        return false;

    app->m_state = Application::BeingRemoved;
    app->m_progress = 0;
    emitDataChanged(app, QVector<int> { IsUpdating });
    return true;
}

void ApplicationManager::progressingApplicationInstall(const QString &id, qreal progress)
{
    const Application *app = fromId(id);
    if (!app || app->m_state == Application::Installed)
        return;
    app->m_progress = progress;
    emitDataChanged(app, QVector<int> { UpdateProgress });
}

bool ApplicationManager::finishedApplicationInstall(const QString &id)
{
    const Application *app = fromId(id);
    if (!app)
        return false;

    switch (app->m_state) {
    case Application::Installed:
        return false;

    case Application::BeingInstalled:
    case Application::BeingUpdated: {
        // The Application object has been updated right at the start of the installation/update.
        // Now's the time to update the InstallationReport that was written by the installer.
        QFile irfile(QDir(app->manifestDir()).absoluteFilePath(qSL("installation-report.yaml")));
        QScopedPointer<InstallationReport> ir(new InstallationReport(app->id()));
        if (!irfile.open(QFile::ReadOnly) || !ir->deserialize(&irfile)) {
            qCCritical(LogInstaller) << "Could not read the new installation-report for application"
                                     << app->id() << "at" << irfile.fileName();
            return false;
        }
        const_cast<Application *>(app)->setInstallationReport(ir.take());
        app->m_state = Application::Installed;
        app->m_progress = 0;

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
    const Application *app = fromId(id);
    if (!app)
        return false;

    switch (app->m_state) {
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
        app->m_state = Application::Installed;
        app->m_progress = 0;
        emitDataChanged(app, QVector<int> { IsUpdating });

        unblockApplication(id);
        break;
    }
    return true;
}

void ApplicationManager::preload()
{
    bool forcePreload = d->database && d->database->isTemporary();

    for (const Application *app : qAsConst(d->apps)) {
        if (forcePreload || app->isPreloaded()) {
            if (!startApplication(app)) {
                qCWarning(LogSystem) << "WARNING: unable to start preload-enabled application" << app->id();
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
        for (const Application *app : qAsConst(d->apps)) {
            AbstractRuntime *rt = app->currentRuntime();
            if (rt) {
                activeRuntime = true;
                break;
            }
        }
        if (!activeRuntime)
            emit shutDownFinished();
    };

    for (const Application *app : qAsConst(d->apps)) {
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
    openUrl(url.toString());
}

void ApplicationManager::emitDataChanged(const Application *app, const QVector<int> &roles)
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

    const Application *app = d->apps.at(index.row());

    switch (role) {
    case Id:
        return app->id();
    case Name: {
        QString name;
        if (!app->names().isEmpty()) {
            name = app->name(d->currentLocale);
            if (name.isEmpty())
                name = app->name(qSL("en"));
            if (name.isEmpty())
                name = app->name(qSL("en_US"));
            if (name.isEmpty())
                name = *app->names().constBegin();
        } else {
            name = app->id();
        }
        return name;
    }
    case Icon:
        return QUrl::fromLocalFile(app->icon());

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
        return app->absoluteCodeFilePath();
    case RuntimeName:
        return app->runtimeName();
    case RuntimeParameters:
        return app->runtimeParameters();
    case BackgroundMode: {
        switch (app->backgroundMode()) {
        case Application::Auto:
            return qL1S("Auto");
        case Application::Never:
            return qL1S("Never");
        case Application::ProvidesVoIP:
            return qL1S("ProvidesVoIP");
        case Application::PlaysAudio:
            return qL1S("PlaysAudio");
        case Application::TracksLocation:
            return qL1S("TracksLocation");
        }
        return QString();
    }
    case Capabilities:
        return app->capabilities();
    case Categories:
        return app->categories();
    case Importance:
        return app->importance();
    case Preload:
        return app->isPreloaded();
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
const Application *ApplicationManager::application(int index) const
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
const Application *ApplicationManager::application(const QString &id) const
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
    AM_AUTHENTICATE_DBUS(QStringList)

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
    AM_AUTHENTICATE_DBUS(QVariantMap)

    auto map = get(indexOfApplication(id));
    if (calledFromDBus())
        map.remove(qSL("application")); // cannot marshall QObject *
    return map;
}

ApplicationManager::RunState ApplicationManager::applicationRunState(const QString &id) const
{
    AM_AUTHENTICATE_DBUS(ApplicationManager::RunState)

    int index = indexOfApplication(id);
    if (index < 0) {
        qCWarning(LogSystem) << "invalid index:" << index;
        return NotRunning;
    }
    const Application *app = d->apps.at(index);
    if (!app->currentRuntime())
        return NotRunning;
    return runtimeToManagerState(app->currentRuntime()->state());
}

QT_END_NAMESPACE_AM
