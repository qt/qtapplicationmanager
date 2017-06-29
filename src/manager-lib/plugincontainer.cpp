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

#include <functional>
#include <application.h>
#include "plugincontainer.h"

QT_BEGIN_NAMESPACE_AM

PluginContainerManager::PluginContainerManager(ContainerManagerInterface *managerInterface, QObject *parent)
    : AbstractContainerManager(managerInterface->identifier(), parent)
    , m_interface(managerInterface)
{ }

QString PluginContainerManager::defaultIdentifier()
{
    return qSL("invalid-plugin");
}

bool PluginContainerManager::supportsQuickLaunch() const
{
    return m_interface->supportsQuickLaunch();
}

AbstractContainer *PluginContainerManager::create(const Application *app, const QVector<int> &stdioRedirections,
                                                  const QMap<QString, QString> &debugWrapperEnvironment,
                                                  const QStringList &debugWrapperCommand)
{
    auto containerInterface = m_interface->create(app == nullptr, stdioRedirections,
                                                  debugWrapperEnvironment, debugWrapperCommand);
    if (!containerInterface)
        return nullptr;
    return new PluginContainer(this, app, containerInterface);
}

void PluginContainerManager::setConfiguration(const QVariantMap &configuration)
{
    AbstractContainerManager::setConfiguration(configuration);
    m_interface->setConfiguration(configuration);
}


PluginContainer::~PluginContainer()
{ }

QString PluginContainer::controlGroup() const
{
    return m_interface->controlGroup();
}

bool PluginContainer::setControlGroup(const QString &groupName)
{
    return m_interface->setControlGroup(groupName);
}

bool PluginContainer::setProgram(const QString &program)
{
    if (AbstractContainer::setProgram(program))
        return m_interface->setProgram(program);
    return false;
}

void PluginContainer::setBaseDirectory(const QString &baseDirectory)
{
    AbstractContainer::setBaseDirectory(baseDirectory);
    m_interface->setBaseDirectory(baseDirectory);
}

bool PluginContainer::isReady()
{
    return m_interface->isReady();
}

QString PluginContainer::mapContainerPathToHost(const QString &containerPath) const
{
    return m_interface->mapContainerPathToHost(containerPath);
}

QString PluginContainer::mapHostPathToContainer(const QString &hostPath) const
{
    return m_interface->mapHostPathToContainer(hostPath);
}

AbstractContainerProcess *PluginContainer::start(const QStringList &arguments, const QMap<QString, QString> &env)
{
    if (m_startCalled)
        return nullptr;

    if (m_interface->start(arguments, env)) {
        m_startCalled = true;
        return m_process;
    }
    return nullptr;
}

PluginContainer::PluginContainer(AbstractContainerManager *manager, const Application *app, ContainerInterface *containerInterface)
    : AbstractContainer(manager, app)
    , m_interface(containerInterface)
    , m_process(new PluginContainerProcess(this))
{
    m_process->setParent(this);

    connect(containerInterface, &ContainerInterface::ready, this, &PluginContainer::ready);
    connect(containerInterface, &ContainerInterface::started, m_process, &PluginContainerProcess::started);
    connect(containerInterface, &ContainerInterface::errorOccured, m_process, &PluginContainerProcess::errorOccured);
    connect(containerInterface, &ContainerInterface::finished, m_process, &PluginContainerProcess::finished);
    connect(containerInterface, &ContainerInterface::stateChanged, m_process, &PluginContainerProcess::stateChanged);

    connect(this, &AbstractContainer::applicationChanged, this, [this]() {
        m_interface->attachApplication(application()->toVariantMap());
    });
    if (application())
        containerInterface->attachApplication(application()->toVariantMap());
}

PluginContainerProcess::PluginContainerProcess(PluginContainer *container)
    : AbstractContainerProcess()
    , m_container(container)
{ }

qint64 PluginContainerProcess::processId() const
{
    return m_container->m_interface->processId();
}

QProcess::ProcessState PluginContainerProcess::state() const
{
    return m_container->m_interface->state();
}

void PluginContainerProcess::kill()
{
    m_container->m_interface->kill();
}

void PluginContainerProcess::terminate()
{
    m_container->m_interface->terminate();
}

QT_END_NAMESPACE_AM
