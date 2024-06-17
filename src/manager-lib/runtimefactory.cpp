// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>

#include "logging.h"
#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "runtimefactory.h"

#include <memory>

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

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
{
    qDeleteAll(m_runtimes);
    s_instance = nullptr;
}

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
        arm = m_runtimes.value(id + u"-inprocess"_s);
    return arm;
}

AbstractRuntime *RuntimeFactory::create(AbstractContainer *container, Application *app)
{
    std::unique_ptr<AbstractContainer> ac(container);

    if (!app || app->runtimeName().isEmpty() || app->currentRuntime())
        return nullptr;

    AbstractRuntimeManager *arm = manager(app->runtimeName());
    if (!arm)
        return nullptr;

    AbstractRuntime *art = arm->create(ac.release(), app);

    if (art) {
        art->setSlowAnimations(m_slowAnimations);
        app->setCurrentRuntime(art);
    }
    return art;
}

AbstractRuntime *RuntimeFactory::createQuickLauncher(AbstractContainer *container, const QString &id)
{
    std::unique_ptr<AbstractContainer> ac(container);

    AbstractRuntimeManager *arm = manager(id);
    if (!arm)
        return nullptr;

    auto runtime = arm->create(ac.release(), nullptr);
    if (runtime)
        runtime->setSlowAnimations(m_slowAnimations);
    return runtime;
}

void RuntimeFactory::setConfiguration(const QVariantMap &configuration)
{
    for (auto it = m_runtimes.cbegin(); it != m_runtimes.cend(); ++it)
        it.value()->setConfiguration(configuration.value(it.key()).toMap());
}

void RuntimeFactory::setSlowAnimations(bool value)
{
    m_slowAnimations = value;
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
    static bool once = false;
    if (!once) {
        qCDebug(LogSystem) << "Registering runtimes:";
        once = true;
    }
    qCDebug(LogSystem).noquote() << " *" << identifier;
    return true;
}

QT_END_NAMESPACE_AM

#include "moc_runtimefactory.cpp"
