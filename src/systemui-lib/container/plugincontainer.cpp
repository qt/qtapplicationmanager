// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <functional>
#include <utilities.h>
#include <application.h>
#include "plugincontainer.h"
#include "debugwrapper.h"
#include "exception.h"
#include "sudo.h"
#include "unixsignalhandler.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

PluginContainerManager::PluginContainerManager(ContainerManagerInterface *managerInterface, QObject *parent)
    : AbstractContainerManager(managerInterface->identifier(), parent)
    , m_interface(managerInterface)
    , m_helpers(new PluginContainerHelperFunctions)
{
}

PluginContainerManager::~PluginContainerManager()
{
    delete m_interface;
}

bool PluginContainerManager::initialize()
{
    return m_interface->initialize(m_helpers.get());
}

QString PluginContainerManager::defaultIdentifier()
{
    return u"invalid-plugin"_s;
}

bool PluginContainerManager::supportsQuickLaunch() const
{
    return m_interface->supportsQuickLaunch();
}

AbstractContainer *PluginContainerManager::create(Application *app, QVector<int> &&stdioRedirections,
                                                  const QMap<QString, QString> &debugWrapperEnvironment,
                                                  const QStringList &debugWrapperCommand)
{
    // stdioRedirections should really be changed to 'move', but we would break the plugin API.
    auto containerInterface = m_interface->create(app == nullptr, std::move(stdioRedirections),
                                                  debugWrapperEnvironment, debugWrapperCommand);
    if (!containerInterface) {
        closeAndClearFileDescriptors(stdioRedirections);
        return nullptr;
    }
    return new PluginContainer(this, app, containerInterface, !debugWrapperCommand.isEmpty());
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

bool PluginContainer::hasDebugWrapper() const
{
    return m_hasDebugWrapper;
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

PluginContainer::PluginContainer(AbstractContainerManager *manager, Application *app,
                                 ContainerInterface *containerInterface, bool hasDebugWrapper)
    : AbstractContainer(manager, app)
    , m_interface(containerInterface)
    , m_hasDebugWrapper(hasDebugWrapper)
{
    m_process = new PluginContainerProcess(this);
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

int PluginContainerProcess::processFd() const
{
    return m_container->m_interface->processFd();
}

Am::RunState PluginContainerProcess::state() const
{
    return static_cast<Am::RunState>(m_container->m_interface->state());
}

void PluginContainerProcess::stop(Am::ExitStatus exitStatus)
{
    m_container->m_interface->stop(static_cast<ContainerInterface::ExitStatus>(exitStatus));
}

void PluginContainerHelperFunctions::closeAndClearFileDescriptors(QVector<int> &fdList)
{
    ::QtAM::closeAndClearFileDescriptors(fdList);
}

QStringList PluginContainerHelperFunctions::substituteCommand(const QStringList &debugWrapperCommand,
                                                              const QString &program,
                                                              const QStringList &arguments)
{
    return DebugWrapper::substituteCommand(debugWrapperCommand, program, arguments);
}

bool PluginContainerHelperFunctions::hasRootPrivileges()
{
    return !SudoClient::instance()->isFallbackImplementation();
}

void PluginContainerHelperFunctions::bindMountFileSystem(const QString &from, const QString &to,
                                                         bool readOnly, int namespacePidFd)
{
    auto sudo = SudoClient::instance();
    if (sudo->isFallbackImplementation())
        throw std::runtime_error("Cannot call bindMountFileSystem without root privileges: make sure the sudo-helper is enabled.");

    try {
        sudo->bindMountFileSystem(from, to, readOnly, namespacePidFd);
    } catch (const Exception &e) {
        throw std::runtime_error(qPrintable(e.errorString()));
    }
}

int PluginContainerHelperFunctions::watchdogSignal()
{
    return UnixSignalHandler::watchdogSignal();
}

QString PluginContainerHelperFunctions::checkDBusSocketPath(const QString &dbusAddress, const QByteArray &typeHint)
{
    // parse the actual socket file name from the DBus address specification
    QString socketPath = dbusAddress;
    socketPath = socketPath.mid(socketPath.indexOf(u'=') + 1);
    socketPath = socketPath.left(socketPath.indexOf(u','));
    QFileInfo socketInfo(socketPath);
    if (!socketInfo.exists()) {
        QByteArray err = typeHint + " DBus socket doesn't exist: " + socketPath.toLocal8Bit();
        throw std::runtime_error(err.constData());
    }
    return socketInfo.absoluteFilePath();
}

QString PluginContainerHelperFunctions::checkWaylandSocketPath(const QString &xdgRuntimeDir, const QString &waylandDisplay)
{
    // parse the wayland socket name from wayland env variables
    QString socketPath = xdgRuntimeDir + u'/' + waylandDisplay;
    QFileInfo socketInfo(socketPath);
    if (!socketInfo.exists()) {
        QByteArray err = "Wayland socket doesn't exist: " + socketPath.toLocal8Bit();
        throw std::runtime_error(err.constData());
    }
    return socketInfo.absoluteFilePath();
}

QT_END_NAMESPACE_AM

#include "moc_plugincontainer.cpp"
