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

#include "application.h"
#include "abstractruntime.h"
#include "applicationinfo.h"

#include <QDebug>

#if defined(Q_OS_UNIX)
#  include <signal.h>
#endif

/*!
    \qmltype Application
    \inqmlmodule QtApplicationManager
    \ingroup system-ui-non-instantiable
    \brief The handle for an application known to the ApplicationManager.

    Most of the read-only properties map directly to values read from the application's
    \c info.yaml file - these are documented in the \l{Manifest Definition}.

    \note There is an unfortunate naming conflict with the (undocumented) \c Application object
          returned from \l{QtQml::Qt::application}{Qt.application}. If you want to use the enums
          defined in the application-manager's \c Application class, you need to use an alias import:

    \badcode
    import QtApplicationManager 1.0 as AppMan

    ...

    if (app.lastExitStatus == AppMan.Application.NormalExit)
        ...
    \endcode
*/

/*!
    \qmlproperty string Application::id
    \readonly

    This property returns the unique id of the application.
*/
/*!
    \qmlproperty string Application::runtimeName
    \readonly

    This property holds the name of the runtime, necessary to run the application's code.
*/
/*!
    \qmlproperty object Application::runtimeParameters
    \readonly

    This property holds a QVariantMap that is passed onto, and interpreted by the application's
    runtime.
*/
/*!
    \qmlproperty url Application::icon
    \readonly

    The URL of the application's icon - can be used as the source property of an \l Image.
*/
/*!
    \qmlproperty string Application::documentUrl
    \readonly

    This property always returns the default \c documentUrl specified in the manifest file, even if
    a different URL was used to start the application.
*/
/*!
    \qmlproperty bool Application::builtIn
    \readonly

    This property describes, if this application is part of the built-in set of applications of the
    current System-UI.
*/
/*!
    \qmlproperty bool Application::alias
    \readonly

    Will return \c true if this Application object is an alias to another one.

    \sa Application::nonAliased
*/
/*!
    \qmlproperty Application Application::nonAliased
    \readonly

    If this Application object is an alias, then you can access the non-alias, base Application
    object via this property, otherwise it contains a reference to itself. This means that if
    you're interested in accessing the base application regardless of whether the object at hand
    is just an alias, you can always safely refer to this property.

    If you want to know whether this object is an alias or a base application, use
    Application::alias instead.
*/
/*!
    \qmlproperty list<string> Application::capabilities
    \readonly

    A list of special access rights for the application - these capabilities can later be queried
    and verified by the middleware via the application-manager.
*/
/*!
    \qmlproperty list<string> Application::supportedMimeTypes
    \readonly

    An array of MIME types the application can handle.
*/
/*!
    \qmlproperty list<string> Application::categories
    \readonly

    A list of category names the application should be associated with. This is mainly for the
    automated app-store uploads as well as displaying the application within a fixed set of
    categories in the System-UI.
*/
/*!
    \qmlproperty object Application::applicationProperties
    \readonly

    All user-defined properties of this application as listed in the private and protected sections
    of the \c applicationProperties field in the manifest file.
*/
/*!
    \qmlproperty Runtime Application::runtime
    \readonly

    Will return a valid \l Runtime object, if the application is currently starting, running or
    shutting down. May return a \c null object, if the application was not yet started.
*/
/*!
    \qmlproperty int Application::lastExitCode
    \readonly

    This property holds the last exit-code of the application's process in multi-process mode.
    On successful application shutdown, this value should normally be \c 0, but can be whatever
    the application returns from its \c main() function.
*/
/*!
    \qmlproperty enumeration Application::lastExitStatus
    \readonly

    This property returns the last exit-status of the application's process in multi-process mode.

    \list
    \li Application.NormalExit - The application exited normally.
    \li Application.CrashExit - The application crashed.
    \li Application.ForcedExit - The application was killed by the application-manager, since it
                                 ignored the quit request originating from a call to
                                 ApplicationManager::stopApplication.
    \endlist

    \sa ApplicationInterface::quit, ApplicationInterface::acknowledgeQuit
*/
/*!
    \qmlproperty string Application::version
    \readonly

    Holds the version of the application as a string.
*/
/*!
    \qmlproperty string Application::codeDir
    \readonly

    The absolute path to the application's installation directory. Please note this directory might
    not always be available for applications that were installed onto removable media.

    \sa {Installation Locations}
*/
/*!
    \qmlproperty enumeration Application::state
    \readonly

    This property holds the current installation state of the application. It can be one of:

    \list
    \li Application.Installed - The application is completely installed and ready to be used.
    \li Application.BeingInstalled - The application is currently in the process of being installed.
    \li Application.BeingUpdated - The application is currently in the process of being updated.
    \li Application.BeingRemoved - The application is currently in the process of being removed.
    \endlist
*/
/*!
    \qmlproperty enumeration Application::runState
    \readonly

    This property holds the current run state of the application. It can be one of:

    \list
    \li Application.NotRunning - the application has not been started yet
    \li Application.StartingUp - the application has been started and is initializing
    \li Application.Running - the application is running
    \li Application.ShuttingDown - the application has been stopped and is cleaning up (in
                                   multi-process mode this signal is only emitted if the
                                   application terminates gracefully)
    \endlist
*/
/*!
    \qmlsignal Application::activated()

    This signal is emitted when the application is started or when it's already running but has
    been requested to be brought to foreground or raised.
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
    if (m_info->icon().isEmpty())
        return QUrl();

    auto appInfo = this->nonAliasedInfo();
    QDir dir;
    switch (state()) {
    default:
    case Installed:
        dir = appInfo->manifestDir();
        break;
    case BeingInstalled:
    case BeingUpdated:
        dir = QDir(appInfo->codeDir().absolutePath() + QLatin1Char('+'));
        break;
    case BeingRemoved:
        dir = QDir(appInfo->codeDir().absolutePath() + QLatin1Char('-'));
        break;
    }
    return QUrl::fromLocalFile(dir.absoluteFilePath(m_info->icon()));
}

QString AbstractApplication::documentUrl() const
{
    return m_info->documentUrl();
}

bool AbstractApplication::isBuiltIn() const
{
    return nonAliasedInfo()->isBuiltIn();
}

bool AbstractApplication::isAlias() const
{
    return m_info->isAlias();
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
    return m_info->applicationProperties();
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
    return m_info->name(language);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Application
///////////////////////////////////////////////////////////////////////////////////////////////////

Application::Application(ApplicationInfo *info)
    : AbstractApplication(info)
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
        setRunState(NotRunning);
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

void Application::setRunState(AbstractApplication::RunState runState)
{
    if (runState != m_runState) {
        m_runState = runState;
        emit runStateChanged(m_runState);
    }
}

QString Application::runtimeName() const
{
    return static_cast<ApplicationInfo*>(m_info.data())->runtimeName();
}

QVariantMap Application::runtimeParameters() const
{
    return static_cast<ApplicationInfo*>(m_info.data())->runtimeParameters();
}

ApplicationInfo *Application::nonAliasedInfo() const
{
    return static_cast<ApplicationInfo*>(m_info.data());
}

void Application::setLastExitCodeAndStatus(int code, QProcess::ExitStatus processStatus)
{
    if (m_lastExitCode != code) {
        m_lastExitCode = code;
        emit lastExitCodeChanged();
    }

    ExitStatus newStatus;
    if (processStatus == QProcess::CrashExit) {
#if defined(Q_OS_UNIX)
        newStatus = (code == SIGTERM || code == SIGKILL) ? ForcedExit : CrashExit;
#else
        newStatus = CrashExit;
#endif
    } else {
        newStatus = NormalExit;
    }

    if (m_lastExitStatus != newStatus) {
        m_lastExitStatus = newStatus;
        emit lastExitStatusChanged();
    }
}

void Application::setInfo(ApplicationInfo *info)
{
    m_info.reset(info);
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
