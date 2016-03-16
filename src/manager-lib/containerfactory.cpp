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

#include <QScopedPointer>
#include <QCoreApplication>
#include <QThreadPool>

#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "containerfactory.h"


ContainerFactory *ContainerFactory::s_instance = nullptr;

ContainerFactory *ContainerFactory::instance()
{
    if (!s_instance)
        s_instance = new ContainerFactory(QCoreApplication::instance());
    return s_instance;
}

ContainerFactory::ContainerFactory(QObject *parent)
    : QObject(parent)
{ }

ContainerFactory::~ContainerFactory()
{ }

QStringList ContainerFactory::containerIds() const
{
    return m_containers.keys();
}

AbstractContainerManager *ContainerFactory::manager(const QString &id)
{
    if (id.isEmpty())
        return nullptr;
    return m_containers.value(id);
}

AbstractContainer *ContainerFactory::create(const QString &id)
{
    AbstractContainerManager *acm = manager(id);
    if (!acm)
        return nullptr;
    return acm->create();
}

void ContainerFactory::setConfiguration(const QVariantMap &configuration)
{
    for (auto it = m_containers.cbegin(); it != m_containers.cend(); ++it) {
        it.value()->setConfiguration(configuration.value(it.key()).toMap());
    }
}

bool ContainerFactory::registerContainerInternal(const QString &identifier, AbstractContainerManager *manager)
{
    if (!manager || identifier.isEmpty() || m_containers.contains(identifier))
        return false;
    m_containers.insert(identifier, manager);
    return true;
}

