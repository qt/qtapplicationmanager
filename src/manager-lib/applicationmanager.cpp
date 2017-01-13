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
#include <QMimeDatabase>
#if defined(QT_GUI_LIB)
#  include <QDesktopServices>
#endif
#include <qplatformdefs.h>

#include "global.h"

#include "applicationdatabase.h"
#include "applicationmanager.h"
#include "application.h"
#include "runtimefactory.h"
#include "containerfactory.h"
#include "quicklauncher.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "dbus-policy.h"
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
    \class ApplicationManager
    \internal
*/

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
        \li The unique Id of an application, represented as a string in reverse-dns form (e.g.
            \c com.pelagicore.foo)
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

class ApplicationManagerPrivate
{
public:
    bool securityChecksEnabled = true;
    bool singleProcess;
    QVariantMap systemProperties;
    ApplicationDatabase *database = nullptr;

    QMap<QByteArray, DBusPolicy> dbusPolicy;

    QVector<const Application *> apps;

    QString currentLocale;
    QHash<int, QByteArray> roleNames;

    QVector<IpcProxyObject *> interfaceExtensions;

    QVector<ContainerDebugWrapper> debugWrappers;

    ContainerDebugWrapper parseDebugWrapperSpecification(const QString &spec);

    QList<QPair<QString, QString>> containerSelectionConfig;
    QJSValue containerSelectionFunction;

    ApplicationManagerPrivate();
    ~ApplicationManagerPrivate();
};


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
}

ApplicationManager *ApplicationManager::s_instance = 0;

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
        return 0;
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

bool ApplicationManager::securityChecksEnabled() const
{
    return d->securityChecksEnabled;
}

void ApplicationManager::setSecurityChecksEnabled(bool enabled)
{
    d->securityChecksEnabled = enabled;
}

QVariantMap ApplicationManager::additionalConfiguration() const
{
    return d->systemProperties;
}

void ApplicationManager::setAdditionalConfiguration(const QVariantMap &map)
{
    d->systemProperties = map;
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

    foreach (const QVariant &v, debugWrappers) {
        const QVariantMap &map = v.toMap();

        ContainerDebugWrapper dw(map);
        if (dw.name().isEmpty() || dw.command().isEmpty())
            continue;
        d->debugWrappers.append(dw);
    }

    ContainerDebugWrapper internalDw({
        { qSL("name"), qSL("--internal-redirect-only--") },
        { qSL("command"), QStringList { qSL("%program%"), qSL("%arguments%") } },
        { qSL("supportedRuntimes"), QStringList { qSL("native"), qSL("qml") } },
        { qSL("supportedContainers"), QStringList { qSL("process") } }
    });
    d->debugWrappers.append(internalDw);
}

ContainerDebugWrapper ApplicationManagerPrivate::parseDebugWrapperSpecification(const QString &spec)
{
    // Example:
    //    "gdbserver"
    //    "gdbserver: {port: 5555}"

    static ContainerDebugWrapper fail;

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

    ContainerDebugWrapper dw;
    for (const auto &it : qAsConst(debugWrappers)) {
        if (it.name() == name) {
            dw = it;
            break;
        }
    }

    if (!dw.isValid())
        return fail;

    for (auto it = userParams.cbegin(); it != userParams.cend(); ++it) {
        if (!dw.setParameter(it.key(), it.value()))
            return fail;
    }
    return dw;
}

bool ApplicationManager::setDBusPolicy(const QVariantMap &yamlFragment)
{
    static const QVector<QByteArray> functions {
        QT_STRINGIFY(applicationIds),
        QT_STRINGIFY(get),
        QT_STRINGIFY(startApplication),
        QT_STRINGIFY(debugApplication),
        QT_STRINGIFY(stopApplication),
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
    foreach (const Application *app, d->apps) {
        if (app->id() == id)
            return app;
    }
    return 0;
}

const Application *ApplicationManager::fromProcessId(qint64 pid) const
{
    if (!pid)
        return 0;

    foreach (const Application *app, d->apps) {
        if (app->currentRuntime() && (app->currentRuntime()->applicationProcessId() == pid))
            return app;
    }

    // pid is not a direct child - try indirect children (e.g. when started via gdbserver)
    //TODO: optimize this by pre-computing a list of parent pids first (do the same in nativeruntime)
    qint64 appmanPid = qApp->applicationPid();

    foreach (const Application *app, d->apps) {
        if (app->currentRuntime()) {
            qint64 rtpid = app->currentRuntime()->applicationProcessId();
            qint64 ppid = pid;

            while (ppid > 1 && ppid != appmanPid) {
                ppid = getParentPid(ppid);

                if (rtpid == ppid)
                    return app;
            }
        }
    }
    return 0;
}

const Application *ApplicationManager::fromSecurityToken(const QByteArray &securityToken) const
{
    if (securityToken.size() != AbstractRuntime::SecurityTokenSize)
        return 0;

    foreach (const Application *app, d->apps) {
        if (app->currentRuntime() && app->currentRuntime()->securityToken() == securityToken)
            return app;
    }
    return 0;
}

const Application *ApplicationManager::schemeHandler(const QString &scheme) const
{
    foreach (const Application *app, d->apps) {
        foreach (const QString &mime, app->supportedMimeTypes()) {
            int pos = mime.indexOf(QLatin1Char('/'));

            if ((pos > 0)
                    && (mime.left(pos) == qL1S("x-scheme-handler"))
                    && (mime.mid(pos + 1) == scheme)) {
                return app;
            }
        }
    }
    return 0;
}

const Application *ApplicationManager::mimeTypeHandler(const QString &mimeType) const
{
    foreach (const Application *app, d->apps) {
        if (app->supportedMimeTypes().contains(mimeType))
            return app;
    }
    return 0;
}

void ApplicationManager::registerMimeTypes()
{
    QVector<QString> schemes;
    schemes << qSL("file") << qSL("http") << qSL("https");

    foreach (const Application *a, d->apps) {
        if (a->isAlias())
            continue;

        foreach (const QString &mime, a->supportedMimeTypes()) {
            int pos = mime.indexOf(QLatin1Char('/'));

            if ((pos > 0) && (mime.left(pos) == qL1S("x-scheme-handler")))
                schemes << mime.mid(pos + 1);
        }
    }
#if defined(QT_GUI_LIB)
    foreach (const QString &scheme, schemes)
        QDesktopServices::setUrlHandler(scheme, this, "openUrlRelay");
#endif
}

bool ApplicationManager::startApplication(const Application *app, const QString &documentUrl,
                                          const QString &debugWrapperSpecification,
                                          const QVector<int> &stdRedirections)
{
    if (!app) {
        qCWarning(LogSystem) << "Cannot start an invalid application";
        return false;
    }
    if (app->isLocked()) {
        qCWarning(LogSystem) << "Application" << app->id() << "is blocked - cannot start";
        return false;
    }
    AbstractRuntime *runtime = app->currentRuntime();

    ContainerDebugWrapper debugWrapper;
    if (!debugWrapperSpecification.isEmpty()) {
        debugWrapper = d->parseDebugWrapperSpecification(debugWrapperSpecification);
        if (!debugWrapper.isValid()) {
            qCWarning(LogSystem) << "Application" << app->id() << "cannot be started by this debug wrapper specification:" << debugWrapperSpecification;
            return false;
        }
        debugWrapper.setStdRedirections(stdRedirections);
    }

    if (runtime) {
        switch (runtime->state()) {
        case AbstractRuntime::Startup:
        case AbstractRuntime::Active:
            if (debugWrapper.isValid()) {
                qCDebug(LogSystem) << "Application" << app->id() << "is already running - cannot start with debug-wrapper" << debugWrapper.name();
                return false;
            }

            if (!documentUrl.isNull())
                runtime->openDocument(documentUrl);
            else if (!app->documentUrl().isNull())
                runtime->openDocument(app->documentUrl());

            emit applicationWasActivated(app->isAlias() ? app->nonAliased()->id() : app->id(), app->id());
            return true;

        case AbstractRuntime::Shutdown:
            return false;

        case AbstractRuntime::Inactive:
            break;
        }
    }

    auto runtimeManager = RuntimeFactory::instance()->manager(app->runtimeName());
    if (!runtimeManager) {
        qCWarning(LogSystem) << "No RuntimeManager found for runtime:" << app->runtimeName();
        return false;
    }

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

        if (!ContainerFactory::instance()->manager(containerId)) {
            qCWarning(LogSystem) << "No ContainerManager found for container:" << containerId;
            return false;
        }
    }
    bool attachRuntime = false;

    if (debugWrapper.isValid()) {
        if (!debugWrapper.supportsRuntime(app->runtimeName())) {
            qCDebug(LogSystem) << "Application" << app->id() << "is using the" << app->runtimeName()
                               << "runtime, which is not compatible with the requested debug-wrapper"
                               << debugWrapper.name();
            return false;
        }
        if (!debugWrapper.supportsContainer(containerId)) {
            qCDebug(LogSystem) << "Application" << app->id() << "is using the" << containerId
                               << "container, which is not compatible with the requested debug-wrapper"
                               << debugWrapper.name();
            return false;
        }
    }

    if (!runtime) {
        if (!inProcess) {
            if (!debugWrapper.isValid() && app->environmentVariables().isEmpty()) {
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
                if (debugWrapper.isValid())
                    container = ContainerFactory::instance()->create(containerId, app, debugWrapper);
                else
                    container = ContainerFactory::instance()->create(containerId, app);
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
        emit applicationRunStateChanged(app->isAlias() ? app->nonAliased()->id() : app->id(),
                                        runtimeToManagerState(newState));
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
        runtime->openDocument(documentUrl);
    else if (!app->documentUrl().isNull())
        runtime->openDocument(app->documentUrl());

    emit applicationWasActivated(app->isAlias() ? app->nonAliased()->id() : app->id(), app->id());

    qCDebug(LogSystem) << "app:" << app->id() << "; document:" << documentUrl << "; runtime: " << runtime;

    if (inProcess) {
        bool ok = runtime->start();
        if (!ok)
            runtime->deleteLater();
        return ok;
    } else {
        auto f = [=]() {
            qCDebug(LogSystem) << "Container ready for app (" << app->id() <<")!";
            bool successfullyStarted = attachRuntime ? runtime->attachApplicationToQuickLauncher(app)
                                                     : runtime->start();
            if (!successfullyStarted)
                runtime->deleteLater(); // ~Runtime() will clean app->m_runtime

            return successfullyStarted;
        };

        if (container->isReady()) {
            // Since the container is already ready, start the app immediately
            return f();
        }
        else {
            // We postpone the starting of the application to a later point in time since the container is not ready yet
            connect(container, &AbstractContainer::ready, f);
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

void ApplicationManager::killAll()
{
    for (const Application *app : qAsConst(d->apps)) {
        AbstractRuntime *rt = app->currentRuntime();
        if (rt)
            rt->stop(true);
    }
    QuickLauncher::instance()->killAll();
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

    return startApplication(fromId(id), documentUrl);
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

    return startApplication(fromId(id), documentUrl, debugWrapper);
}

#if defined(QT_DBUS_LIB)
bool ApplicationManager::startApplication(const QString &id, const QT_PREPEND_NAMESPACE_AM(UnixFdMap) &redirections, const QString &documentUrl)
{
    AM_AUTHENTICATE_DBUS(bool)

    QVector<int> redirectStd = { -1, -1, -1 };

#if defined(Q_OS_UNIX)
    for (auto it = redirections.cbegin(); it != redirections.cend(); ++it) {
        QDBusUnixFileDescriptor dfd = it.value();
        int fd = dfd.fileDescriptor();

        const QString &which = it.key();
        if (which == qL1S("in"))
            redirectStd[0] = dup(fd);
        else if (which == qL1S("out"))
            redirectStd[1] = dup(fd);
        else if (which == qL1S("err"))
            redirectStd[2] = dup(fd);
    }
#else
    Q_UNUSED(redirections)
#endif
    int result = startApplication(fromId(id), documentUrl, qSL("--internal-redirect-only--"), redirectStd);

    if (!result) {
        // we have to close the fds in this case, otherwise we block the tty where the fds are
        // originating from.
        //TODO: this really needs to fixed centrally (e.g. via the DebugWrapper), but this is the most
        //      common error case for now.
        for (int fd : qAsConst(redirectStd)) {
            if (fd >= 0)
                QT_CLOSE(fd);
        }
    }
    return result;
}

bool ApplicationManager::debugApplication(const QString &id, const QString &debugWrapper, const QT_PREPEND_NAMESPACE_AM(UnixFdMap) &redirections, const QString &documentUrl)
{
    AM_AUTHENTICATE_DBUS(bool)

    QVector<int> redirectStd = { -1, -1, -1 };

#if defined(Q_OS_UNIX)
    for (auto it = redirections.cbegin(); it != redirections.cend(); ++it) {
        QDBusUnixFileDescriptor dfd = it.value();
        int fd = dfd.fileDescriptor();

        const QString &which = it.key();
        if (which == qL1S("in"))
            redirectStd[0] = dup(fd);
        else if (which == qL1S("out"))
            redirectStd[1] = dup(fd);
        else if (which == qL1S("err"))
            redirectStd[2] = dup(fd);
    }
#else
    Q_UNUSED(redirections)
#endif
    int result = startApplication(fromId(id), documentUrl, debugWrapper, redirectStd);

    if (!result) {
        // we have to close the fds in this case, otherwise we block the tty where the fds are
        // originating from.
        //TODO: this really needs to fixed centrally (e.g. via the DebugWrapper), but this is the most
        //      common error case for now.
        for (int fd : qAsConst(redirectStd)) {
            if (fd >= 0)
                QT_CLOSE(fd);
        }
    }
    return result;
}
#endif

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
    \qmlmethod bool ApplicationManager::openUrl(string url)

    Tries to match the supplied \a url against the internal MIME database. If a match is found,
    the corresponding application is started via startApplication() and \a url is supplied to
    it as a document to open.

    Returns the result of startApplication(), or \c false if no application was registered for
    the type and/or scheme of \a url.
*/
bool ApplicationManager::openUrl(const QString &urlStr)
{
    AM_AUTHENTICATE_DBUS(bool)

    QUrl url(urlStr);
    const Application *app = 0;
    if (url.isValid()) {
        QString scheme = url.scheme();
        if (scheme != qL1S("file")) {
            app = schemeHandler(scheme);

            if (app) {
                if (app->isAlias())
                    app = app->nonAliased();

                // try to find a better matching alias, if available
                foreach (const Application *alias, d->apps) {
                    if (alias->isAlias() && alias->nonAliased() == app) {
                        if (url.toString(QUrl::PrettyDecoded | QUrl::RemoveScheme) == alias->documentUrl()) {
                            app = alias;
                            break;
                        }
                    }
                }
            }
        }

        if (!app) {
            QMimeDatabase mdb;
            QMimeType mt = mdb.mimeTypeForUrl(url);

            app = mimeTypeHandler(mt.name());
            if (app && app->isAlias())
                app = app->nonAliased();
        }
    }
    if (app)
        return startApplication(app, urlStr);
    return false;
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
    \qmlmethod string ApplicationManager::identifyApplication(int pid, string securityToken)

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

bool ApplicationManager::lockApplication(const QString &id)
{
    const Application *app = fromId(id);
    if (!app)
        return false;
    if (!app->lock())
        return false;
    emitDataChanged(app, QVector<int> { IsBlocked });
    stopApplication(app, true);
    emitDataChanged(app, QVector<int> { IsRunning });
    return true;
}

bool ApplicationManager::unlockApplication(const QString &id)
{
    const Application *app = fromId(id);
    if (!app)
        return false;
    if (!app->unlock())
        return false;
    emitDataChanged(app, QVector<int> { IsBlocked });
    return true;
}

bool ApplicationManager::startingApplicationInstallation(Application *installApp)
{
    if (!installApp || installApp->id().isEmpty())
        return false;
    const Application *app = fromId(installApp->id());
    if (!RuntimeFactory::instance()->manager(installApp->runtimeName()))
        return false;

    if (app) { // update
        if (!lockApplication(app->id()))
            return false;
        installApp->mergeInto(const_cast<Application *>(app));
        app->m_state = Application::BeingUpdated;
        app->m_progress = 0;
        emitDataChanged(app, QVector<int> { IsUpdating });
    } else { // installation
        installApp->setParent(this);
        installApp->m_locked.ref();
        installApp->m_state = Application::BeingInstalled;
        installApp->m_progress = 0;
        beginInsertRows(QModelIndex(), d->apps.count(), d->apps.count());
        d->apps << installApp;
        endInsertRows();
        emit applicationAdded(installApp->id());
        try {
            if (d->database)
                d->database->write(d->apps);
        } catch (const Exception &) {
            emit applicationAboutToBeRemoved(installApp->id());
            beginRemoveRows(QModelIndex(), d->apps.count() - 1, d->apps.count() - 1);
            d->apps.removeLast();
            endRemoveRows();
            delete installApp;
            return false;
        }
        emitDataChanged(app);
    }
    return true;
}

bool ApplicationManager::startingApplicationRemoval(const QString &id)
{
    const Application *app = fromId(id);
    if (!app || app->m_locked.load() || (app->m_state != Application::Installed))
        return false;

    if (app->isBuiltIn())
        return false;

    if (!lockApplication(id))
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
    case Application::BeingUpdated:
        app->m_state = Application::Installed;
        app->m_progress = 0;
        emitDataChanged(app, QVector<int> { IsUpdating });

        unlockApplication(id);
        break;

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
            qCCritical(LogSystem) << "ERROR: Application was removed successfully, but we could not save the app database:" << e.what();
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

        unlockApplication(id);
        break;
    }
    return true;
}

void ApplicationManager::preload()
{
    bool forcePreload = d->database && d->database->isTemporary();

    foreach (const Application *app, d->apps) {
        if (forcePreload || app->isPreloaded()) {
            if (!startApplication(app)) {
                qCWarning(LogSystem) << "WARNING: unable to start preload-enabled application" << app->id();
            }
        }
    }
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
        return app->isLocked();
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

    int index = indexOfApplication(id);
    if (index >= 0) {
        auto map = get(index);
        map.remove(qSL("application")); // cannot marshall QObject *
        return map;
    }
    return QVariantMap();
}

ApplicationManager::RunState ApplicationManager::applicationRunState(const QString &id) const
{
    AM_AUTHENTICATE_DBUS(ApplicationManager::RunState)

    int index = indexOfApplication(id);
    if (index < 0)
        return NotRunning;
    const Application *app = d->apps.at(index);
    if (!app->currentRuntime())
        return NotRunning;
    return runtimeToManagerState(app->currentRuntime()->state());
}

QT_END_NAMESPACE_AM
