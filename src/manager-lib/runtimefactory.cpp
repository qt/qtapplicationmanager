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
#include "runtimefactory.h"

RuntimeFactory *RuntimeFactory::s_instance = 0;

RuntimeFactory *RuntimeFactory::instance()
{
    if (!s_instance)
        s_instance = new RuntimeFactory(QCoreApplication::instance());
    return s_instance;
}

RuntimeFactory::RuntimeFactory(QObject *parent)
    : QObject(parent)
{
}

RuntimeFactory::~RuntimeFactory()
{
}

QStringList RuntimeFactory::runtimeIds() const
{
    return m_runtimes.keys();
}

AbstractRuntime *RuntimeFactory::create(const Application *app)
{
    if (!app || app->runtimeName().isEmpty() || app->currentRuntime())
        return 0;

    const QMetaObject *metaObject = m_runtimes.value(app->runtimeName());
    if (!metaObject)
        metaObject =  m_runtimes.value(app->runtimeName() + "-inprocess");
    if (!metaObject)
        return 0;

    QScopedPointer<QObject> o(metaObject->newInstance(Q_ARG(QObject *, qApp)));

    if (AbstractRuntime *rt = qobject_cast<AbstractRuntime *>(o.data())) {
        if (rt->create(app)) {
            app->setCurrentRuntime(rt);
            o.take();
            return rt;
        }
    }
    return 0;
}

bool RuntimeFactory::registerRuntimeInternal(const QString &identifier, const QMetaObject *metaObject)
{
    if (!metaObject || identifier.isEmpty() || m_runtimes.contains(identifier))
        return false;
    m_runtimes.insert(identifier, metaObject);
    return true;
}


