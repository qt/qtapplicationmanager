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

#include <QScopedPointer>
#include <QCoreApplication>
#include <QThreadPool>

#include "logging.h"
#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "containerfactory.h"

QT_BEGIN_NAMESPACE_AM

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
{
    qDeleteAll(m_containers);
    s_instance = nullptr;
}

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

AbstractContainer *ContainerFactory::create(const QString &id, AbstractApplication *app,
                                            const QVector<int> &stdioRedirections,
                                            const QMap<QString, QString> &debugWrapperEnvironment,
                                            const QStringList &debugWrapperCommand)
{
    AbstractContainerManager *acm = manager(id);
    if (!acm)
        return nullptr;
    return acm->create(app, stdioRedirections, debugWrapperEnvironment, debugWrapperCommand);
}

void ContainerFactory::setConfiguration(const QVariantMap &configuration)
{
    for (auto it = m_containers.cbegin(); it != m_containers.cend(); ++it) {
        it.value()->setConfiguration(configuration.value(it.key()).toMap());
    }
}

bool ContainerFactory::registerContainer(AbstractContainerManager *manager)
{
    return registerContainer(manager, manager->identifier());
}

bool ContainerFactory::registerContainer(AbstractContainerManager *manager, const QString &identifier)
{
    if (!manager || identifier.isEmpty() || m_containers.contains(identifier))
        return false;
    m_containers.insert(identifier, manager);
    manager->setParent(this);
    static bool once = false;
    if (!once) {
        qCDebug(LogSystem) << "Registering containers:";
        once = true;
    }
    qCDebug(LogSystem).noquote() << " *" << identifier << (manager->supportsQuickLaunch() ? "[quicklaunch supported]" : "");
    return true;
}


QT_END_NAMESPACE_AM
