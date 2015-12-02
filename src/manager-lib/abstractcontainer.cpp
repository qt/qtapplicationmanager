/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include "application.h"
#include "abstractcontainer.h"


AbstractContainer::~AbstractContainer()
{ }

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

AbstractContainer::AbstractContainer(AbstractContainerManager *manager)
    : QObject(manager)
{ }


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
