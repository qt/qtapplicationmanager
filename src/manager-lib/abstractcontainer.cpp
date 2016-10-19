/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

AbstractContainer::AbstractContainer(AbstractContainerManager *manager)
    : QObject(manager)
    , m_manager(manager)
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


ContainerDebugWrapper::ContainerDebugWrapper()
    : m_stdRedirections(3, -1)
{ }

ContainerDebugWrapper::ContainerDebugWrapper(const QVariantMap &map)
    : m_stdRedirections(3, -1)
{
    m_name = map.value(qSL("name")).toString();
    m_command = map.value(qSL("command")).toStringList();
    m_parameters = map.value(qSL("parameters")).toMap();
    m_supportedContainers = map.value(qSL("supportedContainers")).toStringList();
    m_supportedRuntimes = map.value(qSL("supportedRuntimes")).toStringList();

    m_valid = !m_name.isEmpty() && !m_command.isEmpty();
}

QString ContainerDebugWrapper::name() const
{
    return m_name;
}

bool ContainerDebugWrapper::isValid() const
{
    return m_valid;
}

QStringList ContainerDebugWrapper::command() const
{
    return m_command;
}

bool ContainerDebugWrapper::supportsContainer(const QString &containerId) const
{
    return m_supportedContainers.contains(containerId);
}

bool ContainerDebugWrapper::supportsRuntime(const QString &runtimeId) const
{
    return m_supportedRuntimes.contains(runtimeId);
}

void ContainerDebugWrapper::setStdRedirections(const QVector<int> &stdRedirections)
{
    for (int i = 0; i < 3; ++i)
        m_stdRedirections[i] = stdRedirections.value(i, -1);
}

QVector<int> ContainerDebugWrapper::stdRedirections() const
{
    return m_stdRedirections;
}

bool ContainerDebugWrapper::setParameter(const QString &key, const QVariant &value)
{
    auto it = m_parameters.find(key);
    if (it == m_parameters.end())
        return false;
    *it = value;
    return true;
}

void ContainerDebugWrapper::resolveParameters(const QString &program, const QStringList &arguments)
{
    for (auto it = m_parameters.cbegin(); it != m_parameters.cend(); ++it) {
        QString key = qL1C('%') + it.key() + qL1C('%');
        QString value = it.value().toString();

        // replace variable name with value in command line and redirects
        std::for_each(m_command.begin(), m_command.end(), [key, value](QString &str) {
            str.replace(key, value);
        });
    }

    QStringList args = arguments;

    bool foundArgumentMarker = false;
    for (int i = m_command.count() - 1; i >= 0; --i) {
        QString str = m_command.at(i);
        if (str == qL1S("%arguments%")) {
            foundArgumentMarker = true;
            continue;
        }
        str.replace(qL1S("%program%"), program);

        if (i == 0 || foundArgumentMarker)
            args.prepend(str);
        else
            args.append(str);
    }
    m_command = args;
}

QT_END_NAMESPACE_AM
