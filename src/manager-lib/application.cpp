/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include "application.h"
#include "abstractruntime.h"
#include "applicationinfo.h"
#include "exception.h"
#include "logging.h"

#include <QDebug>

#if defined(Q_OS_UNIX)
#  include <signal.h>
#endif

/*!
    \qmltype ApplicationObject
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief The handle for an application known to the ApplicationManager.

    Most of the read-only properties map directly to values read from the application's
    \c info.yaml file - these are documented in the \l{Manifest Definition}.
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
    \qmlproperty url ApplicationObject::icon
    \readonly

    The URL of the application's icon - can be used as the source property of an \l Image.
*/
/*!
    \qmlproperty string ApplicationObject::documentUrl
    \readonly

    This property always returns the default \c documentUrl specified in the manifest file, even if
    a different URL was used to start the application.
*/
/*!
    \qmlproperty bool ApplicationObject::builtIn
    \readonly

    This property describes, if this application is part of the built-in set of applications of the
    current System-UI.
*/
/*!
    \qmlproperty bool ApplicationObject::alias
    \readonly

    Will return \c true if this ApplicationObject object is an alias to another one.

    \sa nonAliased
*/
/*!
    \qmlproperty ApplicationObject ApplicationObject::nonAliased
    \readonly

    If this ApplicationObject is an alias, then you can access the non-alias, base ApplicationObject
    via this property, otherwise it contains a reference to itself. This means that if you're interested
    in accessing the base application regardless of whether the object at hand is just an alias, you
    can always safely refer to this property.

    If you want to know whether this object is an alias or a base ApplicationObject, use
    ApplicationObject::alias instead.
*/
/*!
    \qmlproperty list<string> ApplicationObject::capabilities
    \readonly

    A list of special access rights for the application - these capabilities can later be queried
    and verified by the middleware via the application-manager.
*/
/*!
    \qmlproperty list<string> ApplicationObject::supportedMimeTypes
    \readonly

    An array of MIME types the application can handle.
*/
/*!
    \qmlproperty list<string> ApplicationObject::categories
    \readonly

    A list of category names the application should be associated with. This is mainly for the
    automated app-store uploads as well as displaying the application within a fixed set of
    categories in the System-UI.
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
    \li Am.ForcedExit - The application was killed by the application-manager, since it
                        ignored the quit request originating from a call to
                        ApplicationManager::stopApplication.
    \endlist

    \sa ApplicationInterface::quit, ApplicationInterface::acknowledgeQuit
*/
/*!
    \qmlproperty string ApplicationObject::version
    \readonly

    Holds the version of the application as a string.
*/
/*!
    \qmlproperty string ApplicationObject::codeDir
    \readonly

    The absolute path to the application's installation directory. Please note this directory might
    not always be available for applications that were installed onto removable media.

    \sa {Installation Locations}
*/
/*!
    \qmlproperty enumeration ApplicationObject::state
    \readonly

    This property holds the current installation state of the application. It can be one of:

    \list
    \li ApplicationObject.Installed - The application is completely installed and ready to be used.
    \li ApplicationObject.BeingInstalled - The application is currently in the process of being installed.
    \li ApplicationObject.BeingUpdated - The application is currently in the process of being updated.
    \li ApplicationObject.BeingDowngraded - The application is currently in the process of being downgraded.
                                            That can only happen for a built-in application that was previously
                                            upgraded. It will then be brought back to its original, built-in,
                                            version and its state will go back to ApplicationObject.Installed.
    \li ApplicationObject.BeingRemoved - The application is currently in the process of being removed.
    \endlist
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
    \qmlsignal ApplicationObject::activated()

    This signal is emitted when the application is started or when it's already running but has
    been requested to be brought to foreground or raised.
*/
/*!
    \qmlmethod string ApplicationObject::name(string language)

    Returns the name of the application in the given \a language, as provided in the the info.yaml
    file.
*/
/*!
    \qmlmethod bool ApplicationObject::start(string document)

    Starts the application. The optional argument \a document will be supplied to the application as
    is - most commonly this is used to refer to a document to display.

    \sa ApplicationManager::startApplication
*/
/*!
    \qmlmethod bool ApplicationObject::debug(string debugWrapper, string document)

    Same as start() with the difference that it is started via the given \a debugWrapper.
    Please see the \l{Debugging} page for more information on how to setup and use these debug-wrappers.

    \sa ApplicationManager::debugApplication
*/
/*!
    \qmlmethod ApplicationObject::stop(bool forceKill)

    Stops the application. The meaning of the \a forceKill parameter is runtime dependent, but in
    general you should always try to stop an application with \a forceKill set to \c false first
    in order to allow a clean shutdown. Use \a forceKill set to \c true only as a last resort to
    kill hanging applications.

    \sa ApplicationManager::stopApplication
*/

QT_BEGIN_NAMESPACE_AM

///////////////////////////////////////////////////////////////////////////////////////////////////
// AbstractApplication
///////////////////////////////////////////////////////////////////////////////////////////////////

AbstractApplication::AbstractApplication(AbstractApplicationInfo *info)
    : m_info(info)
{
}

QString AbstractApplication::id() const
{
    return m_info->id();
}

QUrl AbstractApplication::icon() const
{
    if (info()->icon().isEmpty())
        return QUrl();

    auto appInfo = this->nonAliasedInfo();
    QDir dir;
    switch (state()) {
    default:
    case Installed:
        dir.setPath(appInfo->manifestDir());
        break;
    case BeingInstalled:
    case BeingUpdated:
        dir.setPath(appInfo->codeDir().absolutePath() + QLatin1Char('+'));
        break;
    case BeingRemoved:
        dir.setPath(appInfo->codeDir().absolutePath() + QLatin1Char('-'));
        break;
    }
    return QUrl::fromLocalFile(dir.absoluteFilePath(info()->icon()));
}

QString AbstractApplication::documentUrl() const
{
    return info()->documentUrl();
}

bool AbstractApplication::isBuiltIn() const
{
    return nonAliasedInfo()->isBuiltIn();
}

bool AbstractApplication::isAlias() const
{
    return info()->isAlias();
}

QStringList AbstractApplication::capabilities() const
{
    return nonAliasedInfo()->capabilities();
}

QStringList AbstractApplication::supportedMimeTypes() const
{
    return nonAliasedInfo()->supportedMimeTypes();
}

QStringList AbstractApplication::categories() const
{
    return nonAliasedInfo()->categories();
}

QVariantMap AbstractApplication::applicationProperties() const
{
    return info()->applicationProperties();
}

bool AbstractApplication::supportsApplicationInterface() const
{
    return nonAliasedInfo()->supportsApplicationInterface();
}

QString AbstractApplication::version() const
{
    return nonAliasedInfo()->version();
}

QString AbstractApplication::codeDir() const
{
    switch (state()) {
    default:
    case Installed:
        return nonAliasedInfo()->codeDir().absolutePath();
    case BeingInstalled:
    case BeingUpdated:
        return nonAliasedInfo()->codeDir().absolutePath() + QLatin1Char('+');
    case BeingRemoved:
        return nonAliasedInfo()->codeDir().absolutePath() + QLatin1Char('-');
    }
}

QString AbstractApplication::name(const QString &language) const
{
    return info()->name(language);
}

void AbstractApplication::start(const QString &documentUrl)
{
    emit requests.startRequested(documentUrl);
}

void AbstractApplication::debug(const QString &debugWrapper, const QString &documentUrl)
{
    emit requests.debugRequested(debugWrapper, documentUrl);
}

void AbstractApplication::stop(bool forceKill)
{
    emit requests.stopRequested(forceKill);
}

QVector<AbstractApplication *> AbstractApplication::fromApplicationInfoVector(
        QVector<AbstractApplicationInfo *> &appInfoVector)
{
    QVector<AbstractApplication *> apps;

    auto findAppWithId = [&apps] (const QString &id) -> AbstractApplication*
    {
        for (AbstractApplication *app : apps) {
            if (app->id() == id)
                return app;
        }
        return nullptr;
    };

    auto extractBaseId = [] (const QString &id) -> QString
    {
        return id.section(qL1C('@'), 0, 0);
    };

    for (auto *appInfo : appInfoVector) {
        QScopedPointer<AbstractApplication> app;
        if (appInfo->isAlias()) {
            auto *originalApp = findAppWithId(extractBaseId(appInfo->id()));
            if (!originalApp)
                throw Exception(Error::Parse, "Could not find base app for alias id %2").arg(appInfo->id());
            Q_ASSERT(!originalApp->isAlias());
            app.reset(new ApplicationAlias(static_cast<Application*>(originalApp),
                                           static_cast<ApplicationAliasInfo*>(appInfo)));
        } else {
            AbstractApplication *otherAbsApp = findAppWithId(appInfo->id());
            if (otherAbsApp) {
                // There's already another ApplicationInfo with the same id. It's probably an update for a
                // built-in app, in which case we use the same Application instance to hold both
                // ApplicationInfo instances.
                bool merged = false;

                if (!otherAbsApp->isAlias()) {
                    auto otherApp = static_cast<Application*>(otherAbsApp);
                    auto fullAppInfo = static_cast<ApplicationInfo*>(appInfo);
                    if (otherApp->isBuiltIn() && !fullAppInfo->isBuiltIn() && !otherApp->updatedInfo()) {
                        otherApp->setUpdatedInfo(static_cast<ApplicationInfo*>(appInfo));
                        merged = true;
                    } else if (!otherApp->isBuiltIn() && fullAppInfo->isBuiltIn() && !otherApp->updatedInfo()) {
                        auto currentBaseInfo = otherApp->takeBaseInfo();
                        otherApp->setBaseInfo(static_cast<ApplicationInfo*>(appInfo));
                        otherApp->setUpdatedInfo(currentBaseInfo);
                        merged = true;
                    }
                }

                if (!merged)
                    qCWarning(LogSystem).nospace() << "Found a second application with id "
                        << appInfo->id() << " which is not an update for a built-in one. Ignoring it.";
            } else {
                app.reset(new Application(static_cast<ApplicationInfo*>(appInfo)));
            }
        }

        if (!app.isNull())
            apps << app.take();
    }

    return apps;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Application
///////////////////////////////////////////////////////////////////////////////////////////////////

Application::Application(ApplicationInfo *info, State initialState)
    : AbstractApplication(info)
    , m_state(initialState)
{
}

void Application::setCurrentRuntime(AbstractRuntime *rt)
{
    if (m_runtime == rt)
        return;

    if (m_runtime)
        disconnect(m_runtime, nullptr, this, nullptr);

    m_runtime = rt;
    emit runtimeChanged();

    if (m_runtime) {
        connect(m_runtime, &AbstractRuntime::finished, this, &Application::setLastExitCodeAndStatus);
        connect(m_runtime, &QObject::destroyed, this, [this]() {
            this->setCurrentRuntime(nullptr);
        });
    } else
        setRunState(Am::NotRunning);
}

bool Application::isBlocked() const
{
    return m_blocked.load() == 1;
}

bool Application::block()
{
    return m_blocked.testAndSetOrdered(0, 1);
}

bool Application::unblock()
{
    return m_blocked.testAndSetOrdered(1, 0);
}

void Application::setRunState(Am::RunState runState)
{
    if (runState != m_runState) {
        m_runState = runState;
        emit runStateChanged(m_runState);
    }
}

QString Application::runtimeName() const
{
    return Application::nonAliasedInfo()->runtimeName();
}

QVariantMap Application::runtimeParameters() const
{
    return Application::nonAliasedInfo()->runtimeParameters();
}

AbstractApplicationInfo *Application::info() const
{
    return m_updatedInfo ? m_updatedInfo.data() : m_info.data();
}

ApplicationInfo *Application::nonAliasedInfo() const
{
    return static_cast<ApplicationInfo*>(Application::info());
}

void Application::setLastExitCodeAndStatus(int exitCode, Am::ExitStatus exitStatus)
{
    if (m_lastExitCode != exitCode) {
        m_lastExitCode = exitCode;
        emit lastExitCodeChanged();
    }

    Am::ExitStatus newStatus;
    if (exitStatus == Am::CrashExit) {
#if defined(Q_OS_UNIX)
        newStatus = (exitCode == SIGTERM || exitCode == SIGKILL) ? Am::ForcedExit : Am::CrashExit;
#else
        newStatus = Am::CrashExit;
#endif
    } else {
        newStatus = Am::NormalExit;
    }

    if (m_lastExitStatus != newStatus) {
        m_lastExitStatus = newStatus;
        emit lastExitStatusChanged();
    }
}

void Application::setBaseInfo(ApplicationInfo *info)
{
    m_info.reset(info);
    emit bulkChange();
}

ApplicationInfo *Application::takeBaseInfo()
{
    return static_cast<ApplicationInfo*>(m_info.take());
}

void Application::setUpdatedInfo(ApplicationInfo* info)
{
    Q_ASSERT(!info || (m_info && info->id() == m_info->id()));

    m_updatedInfo.reset(info);
    emit bulkChange();
}

void Application::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(m_state);
    }
}

void Application::setProgress(qreal value)
{
    m_progress = value;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationAlias
///////////////////////////////////////////////////////////////////////////////////////////////////

ApplicationAlias::ApplicationAlias(Application* app, ApplicationAliasInfo* info)
    : AbstractApplication(info)
    , m_application(app)
{
    connect(m_application, &AbstractApplication::runtimeChanged, this, &AbstractApplication::runtimeChanged);
    connect(m_application, &AbstractApplication::lastExitCodeChanged, this, &AbstractApplication::lastExitCodeChanged);
    connect(m_application, &AbstractApplication::lastExitStatusChanged, this, &AbstractApplication::lastExitStatusChanged);
    connect(m_application, &AbstractApplication::stateChanged, this, &AbstractApplication::stateChanged);
    connect(m_application, &AbstractApplication::runStateChanged, this, &AbstractApplication::runStateChanged);
}

QString ApplicationAlias::runtimeName() const
{
    return m_application->runtimeName();
}

QVariantMap ApplicationAlias::runtimeParameters() const
{
    return m_application->runtimeParameters();
}

ApplicationInfo *ApplicationAlias::nonAliasedInfo() const
{
    return m_application->nonAliasedInfo();
}

QT_END_NAMESPACE_AM

QDebug operator<<(QDebug debug, const QT_PREPEND_NAMESPACE_AM(AbstractApplication) *app)
{
    debug << "Application Object:";
    if (app)
        debug << app->info()->toVariantMap();
    else
        debug << "(null)";
    return debug;
}
