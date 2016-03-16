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

#pragma once

//#include <functional>
//#include <QVector>
//#include <QMutex>
//#include <QThread>

#include <QMap>
#include <QObject>

#include "global.h"

class Application;
class AbstractContainer;
class AbstractContainerManager;

//typedef std::function<ExecutionContainer*()> ContainerFactoryFunction;

//class ObjectPool : public QThread {

//    Q_OBJECT

//    typedef std::function<ExecutionContainer*()> FactoryFunction;

//public:
//    ObjectPool()
//    { }

//    void setFactoryFunction(FactoryFunction factoryFunction)
//    {
//        m_factoryFunction = factoryFunction;
//    }

//    Q_INVOKABLE void createInstances()
//    {
//        bool bCreateInstance = true;
//        do {
//            {
//                QMutexLocker locker(&m_mutex);
//                if (m_instances.size() >= m_size)
//                    bCreateInstance = false;
//            }

//            if (bCreateInstance) {
//                auto instance = m_factoryFunction();
//                QMutexLocker locker(&m_mutex);
//                m_instances.push_back(instance);
//            }

//        } while (bCreateInstance);
//    }

//    void triggerInstancesCreation()
//    {
//        if (!QThread::isRunning()) {
//            this->start();
//            moveToThread(this);
//        }

//        QMetaObject::invokeMethod(this, "createInstances", Qt::QueuedConnection);
//    }

//    ExecutionContainer* getInstance()
//    {
//        QMutexLocker locker(&m_mutex);
//        ExecutionContainer* instance = nullptr;
//        if (m_instances.size() > 0) {
//            instance = m_instances[m_instances.size()-1];
//            m_instances.pop_back();
//        } else {
//            instance = m_factoryFunction();
//        }
//        triggerInstancesCreation();
//        return instance;
//    }

//    size_t m_size = 3;
//    std::vector<ExecutionContainer*> m_instances;
//    FactoryFunction m_factoryFunction;
//    QMutex m_mutex;
//};

class ContainerFactory : public QObject
{
    Q_OBJECT

public:
    static ContainerFactory *instance();
    ~ContainerFactory();

    QStringList containerIds() const;

    AbstractContainerManager *manager(const QString &id);
    AbstractContainer *create(const QString &id);

    void setConfiguration(const QVariantMap &configuration);

    template<typename T> bool registerContainer()
    {
        return registerContainerInternal(T::defaultIdentifier(), new T(T::defaultIdentifier(), this));
    }

    template<typename T> bool registerContainer(const QString &id)
    {
        return registerContainerInternal(id, new T(id, this));
    }

private:
    bool registerContainerInternal(const QString &identifier, AbstractContainerManager *manager);

    ContainerFactory(QObject *parent = 0);
    ContainerFactory(const ContainerFactory &);
    ContainerFactory &operator=(const ContainerFactory &);
    static ContainerFactory *s_instance;

    QMap<QString, AbstractContainerManager *> m_containers;
};

