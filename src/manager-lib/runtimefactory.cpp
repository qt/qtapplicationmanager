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

#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "runtimefactory.h"


RuntimeFactory *RuntimeFactory::s_instance = nullptr;

RuntimeFactory *RuntimeFactory::instance()
{
    if (!s_instance)
        s_instance = new RuntimeFactory(QCoreApplication::instance());
    return s_instance;
}

RuntimeFactory::RuntimeFactory(QObject *parent)
    : QObject(parent)
{ }

RuntimeFactory::~RuntimeFactory()
{ }

QStringList RuntimeFactory::runtimeIds() const
{
    return m_runtimes.keys();
}

AbstractRuntimeManager *RuntimeFactory::manager(const QString &id)
{
    if (id.isEmpty())
        return nullptr;
    AbstractRuntimeManager *arm = m_runtimes.value(id);
    if (!arm)
        arm = m_runtimes.value(id + "-inprocess");
    return arm;
}

AbstractRuntime *RuntimeFactory::create(AbstractContainer *container, const Application *app)
{
    if (!app || app->runtimeName().isEmpty() || app->currentRuntime())
        return nullptr;

    AbstractRuntimeManager *arm = manager(app->runtimeName());
    if (!arm)
        return nullptr;

    AbstractRuntime *art = arm->create(container, app);

    if (art)
        app->setCurrentRuntime(art);
    return art;
}

AbstractRuntime *RuntimeFactory::createQuickLauncher(AbstractContainer *container, const QString &id)
{
    AbstractRuntimeManager *arm = manager(id);
    if (!arm)
        return nullptr;
    return arm->create(container, nullptr);
}

void RuntimeFactory::setConfiguration(const QVariantMap &configuration)
{
    for (auto it = m_runtimes.cbegin(); it != m_runtimes.cend(); ++it) {
        it.value()->setConfiguration(configuration.value(it.key()).toMap());
    }
}

bool RuntimeFactory::registerRuntimeInternal(const QString &identifier, AbstractRuntimeManager *manager)
{
    if (!manager || identifier.isEmpty() || m_runtimes.contains(identifier))
        return false;
    m_runtimes.insert(identifier, manager);
    return true;
}


