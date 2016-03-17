/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#pragma once

#include <QHash>
#include <QObject>

#include "global.h"

class Application;
class AbstractRuntime;
class AbstractRuntimeManager;
class AbstractContainer;


class AM_EXPORT RuntimeFactory : public QObject
{
    Q_OBJECT
public:
    static RuntimeFactory *instance();
    ~RuntimeFactory();

    QStringList runtimeIds() const;

    AbstractRuntimeManager *manager(const QString &id);
    AbstractRuntime *create(AbstractContainer *container, const Application *app);
    AbstractRuntime *createQuickLauncher(AbstractContainer *container, const QString &id);

    void setConfiguration(const QVariantMap &configuration);
    void setAdditionalConfiguration(const QVariantMap &additionalConfiguration);

    template<typename T> bool registerRuntime()
    {
        return registerRuntimeInternal(T::defaultIdentifier(), new T(T::defaultIdentifier(), this));
    }

    template<typename T> bool registerRuntime(const QString &id)
    {
        return registerRuntimeInternal(id, new T(id, this));
    }

private:
    bool registerRuntimeInternal(const QString &identifier, AbstractRuntimeManager *manager);

    RuntimeFactory(QObject *parent = 0);
    RuntimeFactory(const RuntimeFactory &);
    RuntimeFactory &operator=(const RuntimeFactory &);
    static RuntimeFactory *s_instance;

    QHash<QString, AbstractRuntimeManager *> m_runtimes;
};

