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

#include <QScopedPointer>
#include <QCoreApplication>

#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "runtimefactory.h"

AM_BEGIN_NAMESPACE

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
        arm = m_runtimes.value(id + qSL("-inprocess"));
    return arm;
}

AbstractRuntime *RuntimeFactory::create(AbstractContainer *container, const Application *app)
{
    QScopedPointer<AbstractContainer> ac(container);

    if (!app || app->runtimeName().isEmpty() || app->currentRuntime())
        return nullptr;

    AbstractRuntimeManager *arm = manager(app->runtimeName());
    if (!arm)
        return nullptr;

    AbstractRuntime *art = arm->create(ac.take(), app);

    if (art)
        app->setCurrentRuntime(art);
    return art;
}

AbstractRuntime *RuntimeFactory::createQuickLauncher(AbstractContainer *container, const QString &id)
{
    QScopedPointer<AbstractContainer> ac(container);

    AbstractRuntimeManager *arm = manager(id);
    if (!arm)
        return nullptr;

    return arm->create(ac.take(), nullptr);
}

void RuntimeFactory::setConfiguration(const QVariantMap &configuration)
{
    for (auto it = m_runtimes.cbegin(); it != m_runtimes.cend(); ++it) {
        it.value()->setConfiguration(configuration.value(it.key()).toMap());
    }
}

void RuntimeFactory::setAdditionalConfiguration(const QVariantMap &additionalConfiguration)
{
    for (auto it = m_runtimes.cbegin(); it != m_runtimes.cend(); ++it) {
        it.value()->setAdditionalConfiguration(additionalConfiguration);
    }
}

bool RuntimeFactory::registerRuntime(AbstractRuntimeManager *manager)
{
    return registerRuntime(manager, manager->identifier());
}

bool RuntimeFactory::registerRuntime(AbstractRuntimeManager *manager, const QString &identifier)
{
    if (!manager || identifier.isEmpty() || m_runtimes.contains(identifier))
        return false;
    m_runtimes.insert(identifier, manager);
    manager->setParent(this);
    return true;
}

AM_END_NAMESPACE
