// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <qplatformdefs.h>

#include "application.h"
#include "abstractcontainer.h"


/*!
    \qmltype Container
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief The handle for a container, that an application's \l Runtime is using.

    Instances of this class are available to the System UI via an application's \l Runtime object,
    while an application is running. Please see the \l{Containers}{Container documentation} for
    an in-depth description of containers within the application manager.

    \note Applications running in single-process mode (even ones using the \c qml-inprocess runtime
          while the application manager is running in multi-process mode) will have no Container
          object associated.
*/
/*!
    \qmlproperty string Container::controlGroup

    This property lets you get and set which control group the application's container will be in.
    The control group name handled by this property is not the low-level name used by the operating
    system, but an abstraction. See the \l{control group mapping}{container documentation} for more
    details on how to setup this mapping.

    \note Control groups are only supported on Linux through the kernel's \c cgroup system in the
          built-in \c process container. Custom container plugins might not implement the necessary
          interface.
*/


QT_BEGIN_NAMESPACE_AM

AbstractContainer::~AbstractContainer()
{ }

QString AbstractContainer::controlGroup() const
{
    return QString();
}

bool AbstractContainer::setControlGroup(const QString &groupName)
{
    Q_UNUSED(groupName)
    return false;
}

bool AbstractContainer::setProgram(const QString &program)
{
    if (!m_program.isEmpty())
        return false;
    m_program = program;
    return true;
}

void AbstractContainer::setBaseDirectory(const QString &baseDirectory)
{
    m_baseDirectory = baseDirectory;
}

QString AbstractContainer::mapContainerPathToHost(const QString &containerPath) const
{
    return containerPath;
}

QString AbstractContainer::mapHostPathToContainer(const QString &hostPath) const
{
    return hostPath;
}

AbstractContainerProcess *AbstractContainer::process() const
{
    return m_process;
}

void AbstractContainer::setApplication(Application *app)
{
    if (app != m_app) {
        m_app = app;
        emit applicationChanged(app);
    }
}

Application *AbstractContainer::application() const
{
    return m_app;
}

AbstractContainer::AbstractContainer(AbstractContainerManager *manager, Application *app)
    : QObject(manager)
    , m_manager(manager)
    , m_app(app)
{ }

QVariantMap AbstractContainer::configuration() const
{
    if (m_manager)
        return m_manager->configuration();
    return QVariantMap();
}



AbstractContainerManager::AbstractContainerManager(const QString &id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{ }

QString AbstractContainerManager::defaultIdentifier()
{
    return QString();
}

QString AbstractContainerManager::identifier() const
{
    return m_id;
}

bool AbstractContainerManager::supportsQuickLaunch() const
{
    return false;
}

QVariantMap AbstractContainerManager::configuration() const
{
    return m_configuration;
}

void AbstractContainerManager::setConfiguration(const QVariantMap &configuration)
{
    m_configuration = configuration;
}

QT_END_NAMESPACE_AM

#include "moc_abstractcontainer.cpp"
