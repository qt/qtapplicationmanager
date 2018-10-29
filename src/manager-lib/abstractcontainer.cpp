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

#include <qplatformdefs.h>

#include "application.h"
#include "abstractcontainer.h"


/*!
    \qmltype Container
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief The handle for a container, that an application's \l Runtime is using.

    Instances of this class are available to the System-UI via an application's \l Runtime object,
    while an application is running. Please see the \l{Containers}{Container documentation} for
    an in-depth description of containers within the application-manager.

    \note Applications running in single-process mode (even ones using the \c qml-inprocess runtime
          while the application-manager is running in multi-process mode) will have no Container
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

void AbstractContainer::setApplication(AbstractApplication *app)
{
    if (app != m_app) {
        m_app = app;
        emit applicationChanged(app);
    }
}

AbstractApplication *AbstractContainer::application() const
{
    return m_app;
}

AbstractContainer::AbstractContainer(AbstractContainerManager *manager, AbstractApplication *app)
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
