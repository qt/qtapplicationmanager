// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QThreadPool>

#include "logging.h"
#include "utilities.h"
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

AbstractContainer *ContainerFactory::create(const QString &id, Application *app,
                                            QVector<int> &&stdioRedirections,
                                            const QMap<QString, QString> &debugWrapperEnvironment,
                                            const QStringList &debugWrapperCommand)
{
    AbstractContainerManager *acm = manager(id);
    if (!acm) {
        closeAndClearFileDescriptors(stdioRedirections);
        return nullptr;
    }
    return acm->create(app, std::move(stdioRedirections), debugWrapperEnvironment, debugWrapperCommand);
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
    qCDebug(LogSystem).noquote() << " *" << identifier;
    return true;
}

bool ContainerFactory::disableContainer(const QString &identifier)
{
    return m_containers.take(identifier);
}


QT_END_NAMESPACE_AM

#include "moc_containerfactory.cpp"
