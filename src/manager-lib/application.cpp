// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "application.h"
#include "abstractruntime.h"
#include "applicationinfo.h"
#include "package.h"
#include "exception.h"
#include "logging.h"
#include "utilities.h"

#include <QDebug>


/*!
    \qmltype ApplicationObject
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief The handle for an application known to the application manager.

    Each instance of this class represents a single application known to the application manager.

    Most of the read-only properties map directly to values read from the application package's
    \c info.yaml file - these are documented in the \l{Manifest Definition}.

    Items of this type are not creatable from QML code. Only functions and properties of
    ApplicationManager and ApplicationModel will return pointers to this class.

    Make sure to \b not save references to an ApplicationObject across function calls: packages (and
    with that, the applications contained within) can be deinstalled at any time, invalidating your
    reference. In case you do need a persistent handle, use the id string.
*/
/*!
    \qmlproperty string ApplicationObject::id
    \readonly

    This property returns the unique id of the application.
*/
/*!
    \qmlproperty string ApplicationObject::runtimeName
    \readonly

    This property holds the name of the runtime, necessary to run the application's code.
*/
/*!
    \qmlproperty object ApplicationObject::runtimeParameters
    \readonly

    This property holds a QVariantMap that is passed onto, and interpreted by the application's
    runtime.
*/
/*!
    \qmlproperty list<string> ApplicationObject::capabilities
    \readonly

    A list of special access rights for the application - these capabilities can later be queried
    and verified by the middleware via the application manager.
*/
/*!
    \qmlproperty string ApplicationObject::documentUrl
    \readonly

    This was used to distinguish between application aliases, which have been replaced by the
    intents mechanism.

    Always returns an empty string.
*/
/*!
    \qmlproperty list<string> ApplicationObject::supportedMimeTypes
    \readonly

    An array of MIME types the application can handle.
*/
/*!
    \qmlproperty object ApplicationObject::applicationProperties
    \readonly

    All user-defined properties of this application as listed in the private and protected sections
    of the \c applicationProperties field in the manifest file.
*/
/*!
    \qmlproperty Runtime ApplicationObject::runtime
    \readonly

    Will return a valid \l Runtime object, if the application is currently starting, running or
    shutting down. May return a \c null object, if the application was not yet started.
*/
/*!
    \qmlproperty int ApplicationObject::lastExitCode
    \readonly

    This property holds the last exit-code of the application's process in multi-process mode.
    On successful application shutdown, this value should normally be \c 0, but can be whatever
    the application returns from its \c main() function.
*/
/*!
    \qmlproperty enumeration ApplicationObject::lastExitStatus
    \readonly

    This property returns the last exit-status of the application's process in multi-process mode.

    \list
    \li Am.NormalExit - The application exited normally.
    \li Am.CrashExit - The application crashed.
    \li Am.ForcedExit - The application was killed by the application manager, since it
                        ignored the quit request originating from a call to
                        ApplicationManager::stopApplication.
    \endlist

    \sa ApplicationInterface::quit, ApplicationInterface::acknowledgeQuit
*/
/*!
    \qmlproperty string ApplicationObject::codeDir
    \readonly

    The absolute path to the application's installation directory.
*/
/*!
    \qmlproperty enumeration ApplicationObject::runState
    \readonly

    This property holds the current run state of the application. It can be one of:

    \list
    \li Am.NotRunning - the application has not been started yet.
    \li Am.StartingUp - the application has been started and is initializing.
    \li Am.Running - the application is running.
    \li Am.ShuttingDown - the application has been stopped and is cleaning up (in multi-process mode
                          this state is only reached, if the application is terminating gracefully).
    \endlist
*/
/*!
    \qmlproperty Package ApplicationObject::package
    \readonly

    Returns the Package object that contains this application.
*/
/*!
    \qmlsignal ApplicationObject::activated()

    This signal is emitted when the application is started or when it's already running but has
    been requested to be brought to foreground or raised.
*/
/*!
    \qmlmethod bool ApplicationObject::start(string document)

    Starts the application. The optional argument \a document will be supplied to the application as
    is - most commonly this is used to refer to a document to display.

    Returns a \c bool value indicating success. See the full documentation at
    ApplicationManager::startApplication for more information.
*/
/*!
    \qmlmethod bool ApplicationObject::debug(string debugWrapper, string document)

    Same as start() with the difference that it is started via the given \a debugWrapper. The
    optional argument \a document will be supplied to the application as is - most commonly this is
    used to refer to a document to display.

    Please see the \l{Debugging} page for more information on how to setup and use these debug-wrappers.

    Returns a \c bool value indicating success. See the full documentation at
    ApplicationManager::debugApplication for more information.
*/
/*!
    \qmlmethod ApplicationObject::stop(bool forceKill)

    Stops the application. The meaning of the \a forceKill parameter is runtime dependent, but in
    general you should always try to stop an application with \a forceKill set to \c false first
    in order to allow a clean shutdown. Use \a forceKill set to \c true only as a last resort to
    kill hanging applications.

    QML applications and native applications that \l {manifest supportsApplicationInterface}
    {support the ApplicationInterface} will be notified via ApplicationInterface::quit().
    All other applications will be sent the Unix \c TERM signal.

    \sa ApplicationManager::stopApplication
*/
/*!
    \qmlproperty bool ApplicationObject::builtIn
    \readonly
    \qmlobsolete

    Use PackageObject::builtIn
*/
/*!
    \qmlproperty bool ApplicationObject::alias
    \readonly
    \qmlobsolete

    This was used to distinguish between application aliases, which have been replaced by the
    intents mechanism.

    Always returns \c false.
*/
/*!
    \qmlproperty ApplicationObject ApplicationObject::nonAliased
    \readonly
    \qmlobsolete

    This was used to distinguish between application aliases, which have been replaced by the
    intents mechanism.

    Always returns the ApplicationObject itself.
*/
/*!
    \qmlproperty string ApplicationObject::version
    \readonly
    \qmlobsolete

    Use PackageObject::version.
*/
/*!
    \qmlproperty enumeration ApplicationObject::state
    \readonly
    \qmlobsolete

    Use PackageObject::state.
*/
/*!
    \qmlproperty bool ApplicationObject::blocked
    \readonly
    \qmlobsolete

    Use PackageObject::version.
*/
/*!
    \qmlproperty url ApplicationObject::icon
    \readonly

    The URL of the application's icon - can be used as the source property of an \l Image.
    If the application does not specify an \c icon, this will return the same as the containing
    PackageObject::icon.
*/
/*!
    \qmlproperty string ApplicationObject::name
    \readonly

    Returns the localized name of the application - as provided in the info.yaml file - in the currently
    active locale.

    This is a convenience property, that takes the mapping returned by the \l names property,
    and then tries to return the value for these keys if available: first the current locale's id,
    then \c en_US, then \c en and lastly the first available key.

    If no mapping is available, this will return the \l id.
*/
/*!
    \qmlproperty var ApplicationObject::names
    \readonly

    Returns an object with all the language code to localized name mappings as provided in the
    application's info.yaml file. If the application does not specify a \c names object, this will
    return the same as the containing PackageObject::names.
*/
/*!
    \qmlproperty string ApplicationObject::description
    \readonly

    Returns the localized description of the application - as provided in the info.yaml file - in the
    currently active locale.

    This property uses the same algorithm as the \l name property, but for the description.
*/
/*!
    \qmlproperty var ApplicationObject::descriptions
    \readonly

    Returns an object with all the language code to localized description mappings as provided in
    the applications's info.yaml file. If the application does not specify a \c descriptions object,
    this will return the same as the containing PackageObject::descriptions.
*/
/*!
    \qmlproperty list<string> ApplicationObject::categories
    \readonly

    A list of category names the application should be associated with. This is mainly for
    displaying the application within a fixed set of categories in the System UI.
    If the application does not specify a \c categories list, this will return the same as
    the containing PackageObject::categories.
*/

QT_BEGIN_NAMESPACE_AM


///////////////////////////////////////////////////////////////////////////////////////////////////
// Application
///////////////////////////////////////////////////////////////////////////////////////////////////

Application::Application(ApplicationInfo *info, Package *package)
    : m_info(info)
    , m_package(package)
{
    Q_ASSERT(info);
    Q_ASSERT(package);

    // handle package blocking: all apps have to be stopped and the stop state has to be reported
    // back to the package

    connect(package, &Package::blockedChanged, this, [this](bool blocked) {
        emit blockedChanged(blocked);
        if (blocked && (runState() == Am::NotRunning))
            this->package()->applicationStoppedDueToBlock(id());
        else if (blocked)
            stop(true);
    });
    connect(this, &Application::runStateChanged, this, [this](Am::RunState runState) {
        if (isBlocked() && (runState == Am::NotRunning))
            this->package()->applicationStoppedDueToBlock(id());
    });

    connect(package, &Package::stateChanged, this, [this]() {
        emit stateChanged(state());
    });
    connect(package, &Package::bulkChange, this, &Application::bulkChange);
}

bool Application::start(const QString &documentUrl)
{
    if (requests.startRequested)
        return requests.startRequested(documentUrl);
    else
        return false;
}

bool Application::debug(const QString &debugWrapper, const QString &documentUrl)
{
    if (requests.debugRequested)
        return requests.debugRequested(debugWrapper, documentUrl);
    else
        return false;
}

void Application::stop(bool forceKill)
{
    if (requests.stopRequested)
        requests.stopRequested(forceKill);
}

ApplicationInfo *Application::info() const
{
    return m_info;
}

PackageInfo *Application::packageInfo() const
{
    return m_info->packageInfo();
}

Package *Application::package() const
{
    return m_package;
}

QString Application::id() const
{
    return m_info->id();
}

bool Application::isBuiltIn() const
{
    return packageInfo()->isBuiltIn();
}

QString Application::runtimeName() const
{
    return m_info->runtimeName();
}

QVariantMap Application::runtimeParameters() const
{
    return m_info->runtimeParameters();
}

QStringList Application::capabilities() const
{
    return m_info->capabilities();
}

QStringList Application::categories() const
{
    return m_info->categories();
}

QUrl Application::icon() const
{
    if (info()->icon().isEmpty())
        return QUrl();

    return QUrl::fromLocalFile(packageInfo()->baseDir().absoluteFilePath(info()->icon()));
}

QStringList Application::supportedMimeTypes() const
{
    return info()->supportedMimeTypes();
}

QString Application::documentUrl() const
{
    return info()->documentUrl();
}

QString Application::name() const
{
    return translateFromMap(m_info->names(), id());
}

QVariantMap Application::names() const
{
    QVariantMap vm;
    const auto map = m_info->names();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        vm.insert(it.key(), it.value());
    return vm;
}

QString Application::description() const
{
    return translateFromMap(m_info->descriptions());
}

QVariantMap Application::descriptions() const
{
    QVariantMap vm;
    const auto map = m_info->descriptions();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        vm.insert(it.key(), it.value());
    return vm;
}

bool Application::isBlocked() const
{
    return package()->isBlocked();
}

QVariantMap Application::applicationProperties() const
{
    return info()->applicationProperties();
}

bool Application::supportsApplicationInterface() const
{
    return info()->supportsApplicationInterface();
}

QString Application::codeDir() const
{
    switch (package()->state()) {
    default:
    case Package::Installed:
        return packageInfo()->baseDir().absolutePath();
    case Package::BeingInstalled:
    case Package::BeingUpdated:
    case Package::BeingDowngraded:
        return packageInfo()->baseDir().absolutePath() + u'+';
    case Package::BeingRemoved:
        return packageInfo()->baseDir().absolutePath() + u'-';
    }
}

QString Application::version() const
{
    return packageInfo()->version();
}

Application::State Application::state() const
{
    return static_cast<State>(package()->state());
}

qreal Application::progress() const
{
    return package()->progress();
}

void Application::setRunState(Am::RunState runState)
{
    if (runState != m_runState) {
        m_runState = runState;
        emit runStateChanged(m_runState);
    }
}

void Application::setCurrentRuntime(AbstractRuntime *runtime)
{
    if (m_runtime == runtime)
        return;

    if (m_runtime)
        disconnect(m_runtime, nullptr, this, nullptr);

    m_runtime = runtime;
    emit runtimeChanged();

    if (m_runtime) {
        connect(m_runtime, &AbstractRuntime::finished, this, &Application::setLastExitCodeAndStatus);
        connect(m_runtime, &QObject::destroyed, this, [this]() {
            this->setCurrentRuntime(nullptr);
        });
    } else {
        setRunState(Am::NotRunning);
    }
}

void Application::setLastExitCodeAndStatus(int exitCode, Am::ExitStatus exitStatus)
{
    if (m_lastExitCode != exitCode) {
        m_lastExitCode = exitCode;
        emit lastExitCodeChanged();
    }

    if (m_lastExitStatus != exitStatus) {
        m_lastExitStatus = exitStatus;
        emit lastExitStatusChanged();
    }
}


QDebug operator<<(QDebug debug, const Application *app)
{
    debug << "Application Object:";
    if (app)
        debug << app->info()->toVariantMap();
    else
        debug << "(null)";
    return debug;
}

QT_END_NAMESPACE_AM

#include "moc_application.cpp"
