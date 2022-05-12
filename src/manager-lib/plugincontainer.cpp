/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#include <functional>
#include <application.h>
#include "plugincontainer.h"

QT_BEGIN_NAMESPACE_AM

PluginContainerManager::PluginContainerManager(ContainerManagerInterface *managerInterface, QObject *parent)
    : AbstractContainerManager(managerInterface->identifier(), parent)
    , m_interface(managerInterface)
{ }

QString PluginContainerManager::defaultIdentifier()
{
    return qSL("invalid-plugin");
}

bool PluginContainerManager::supportsQuickLaunch() const
{
    return m_interface->supportsQuickLaunch();
}

AbstractContainer *PluginContainerManager::create(Application *app, const QVector<int> &stdioRedirections,
                                                  const QMap<QString, QString> &debugWrapperEnvironment,
                                                  const QStringList &debugWrapperCommand)
{
    auto containerInterface = m_interface->create(app == nullptr, stdioRedirections,
                                                  debugWrapperEnvironment, debugWrapperCommand);
    if (!containerInterface)
        return nullptr;
    return new PluginContainer(this, app, containerInterface);
}

void PluginContainerManager::setConfiguration(const QVariantMap &configuration)
{
    AbstractContainerManager::setConfiguration(configuration);
    m_interface->setConfiguration(configuration);
}


PluginContainer::~PluginContainer()
{ }

QString PluginContainer::controlGroup() const
{
    return m_interface->controlGroup();
}

bool PluginContainer::setControlGroup(const QString &groupName)
{
    return m_interface->setControlGroup(groupName);
}

bool PluginContainer::setProgram(const QString &program)
{
    if (AbstractContainer::setProgram(program))
        return m_interface->setProgram(program);
    return false;
}

void PluginContainer::setBaseDirectory(const QString &baseDirectory)
{
    AbstractContainer::setBaseDirectory(baseDirectory);
    m_interface->setBaseDirectory(baseDirectory);
}

bool PluginContainer::isReady()
{
    return m_interface->isReady();
}

QString PluginContainer::mapContainerPathToHost(const QString &containerPath) const
{
    return m_interface->mapContainerPathToHost(containerPath);
}

QString PluginContainer::mapHostPathToContainer(const QString &hostPath) const
{
    return m_interface->mapHostPathToContainer(hostPath);
}

AbstractContainerProcess *PluginContainer::start(const QStringList &arguments, const QMap<QString, QString> &env,
                                                 const QVariantMap &amConfig)
{
    if (m_startCalled)
        return nullptr;

    if (m_interface->start(arguments, env, amConfig)) {
        m_startCalled = true;
        return m_process;
    }
    return nullptr;
}

PluginContainer::PluginContainer(AbstractContainerManager *manager, Application *app, ContainerInterface *containerInterface)
    : AbstractContainer(manager, app)
    , m_interface(containerInterface)
    , m_process(new PluginContainerProcess(this))
{
    m_process->setParent(this);

    connect(containerInterface, &ContainerInterface::ready, this, &PluginContainer::ready);
    connect(containerInterface, &ContainerInterface::started, m_process, &PluginContainerProcess::started);
    connect(containerInterface, &ContainerInterface::errorOccured,
            m_process, [this](ContainerInterface::ProcessError processError) {
        emit m_process->errorOccured(static_cast<Am::ProcessError>(processError));
    });
    connect(containerInterface, &ContainerInterface::finished,
            m_process, [this](int exitCode, ContainerInterface::ExitStatus exitStatus) {
        emit m_process->finished(exitCode, static_cast<Am::ExitStatus>(exitStatus));
    });
    connect(containerInterface, &ContainerInterface::stateChanged,
            m_process, [this](ContainerInterface::RunState newState) {
        emit m_process->stateChanged(static_cast<Am::RunState>(newState));
    });
    connect(this, &AbstractContainer::applicationChanged, this, [this]() {
        m_interface->attachApplication(application()->info()->toVariantMap());
    });
    if (application())
        containerInterface->attachApplication(application()->info()->toVariantMap());
}

PluginContainerProcess::PluginContainerProcess(PluginContainer *container)
    : AbstractContainerProcess()
    , m_container(container)
{ }

qint64 PluginContainerProcess::processId() const
{
    return m_container->m_interface->processId();
}

Am::RunState PluginContainerProcess::state() const
{
    return static_cast<Am::RunState>(m_container->m_interface->state());
}

void PluginContainerProcess::kill()
{
    m_container->m_interface->kill();
}

void PluginContainerProcess::terminate()
{
    m_container->m_interface->terminate();
}

QT_END_NAMESPACE_AM

#include "moc_plugincontainer.cpp"
