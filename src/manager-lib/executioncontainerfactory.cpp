/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

#include <QScopedPointer>
#include <QCoreApplication>

#include "application.h"
#include "executioncontainerfactory.h"
#include "executioncontainer.h"

#include <assert.h>
#include <QThreadPool>

ExecutionContainerFactory *ExecutionContainerFactory::s_instance = nullptr;

ExecutionContainerFactory *ExecutionContainerFactory::instance()
{
    if (!s_instance)
        s_instance = new ExecutionContainerFactory(QCoreApplication::instance());
    return s_instance;
}

ExecutionContainerFactory::ExecutionContainerFactory(QObject *parent)
    : QObject(parent)
{
}

ExecutionContainerFactory::~ExecutionContainerFactory()
{
}

ExecutionContainer *ExecutionContainerFactory::create(const Application *app)
{
    Q_UNUSED(app)

    // We use the secure container if it has been registered
    if (m_secureRuntime.id != nullptr)
        return m_secureRuntime.m_pool.getInstance();
    else
        return m_defaultRuntime.m_pool.getInstance();
}

void ExecutionContainerFactory::registerExecutionContainer(const char* id, bool secure, ContainerFactoryFunction factoryFunction) {
    auto& entry = secure ? m_secureRuntime : m_defaultRuntime;
    entry.id = id;
    entry.m_pool.setFactoryFunction(factoryFunction);
    entry.secure = secure;
    entry.m_pool.triggerInstancesCreation();
}
