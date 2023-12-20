// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QProcess>
#include <QProcessEnvironment>

#include "global.h"
#include "logging.h"
#include "utilities.h"
#include "containerfactory.h"
#include "application.h"
#include "processcontainer.h"
#include "systemreader.h"
#include "debugwrapper.h"

#if defined(Q_OS_UNIX)
#  include <csignal>
#  include <unistd.h>
#  include <fcntl.h>
#endif

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM


HostProcess::HostProcess()
    : m_process(new QProcess)
{
    m_process->setProcessChannelMode(QProcess::ForwardedChannels);
    m_process->setInputChannelMode(QProcess::ForwardedInputChannel);
#if defined(Q_OS_UNIX)
    m_process->setChildProcessModifier([this]() {
        if (m_stopBeforeExec) {
            fprintf(stderr, "\n*** a 'process' container was started in stopped state ***\nthe process is suspended via SIGSTOP and you can attach a debugger to it via\n\n   gdb -p %d\n\n", getpid());
            raise(SIGSTOP);
        }
        // duplicate any requested redirections to the respective stdin/out/err fd. Also make sure to
        // close the original fd: otherwise we would block the tty where the fds originated from.
        for (int i = 0; i < 3; ++i) {
            int fd = m_stdioRedirections.value(i, -1);
            if (fd >= 0) {
                dup2(fd, i);
                ::close(fd);
            }
        }
    });
#endif
}

HostProcess::~HostProcess()
{
    closeAndClearFileDescriptors(m_stdioRedirections);
    m_process->disconnect(this);
    delete m_process;
}

void HostProcess::start(const QString &program, const QStringList &arguments)
{
    connect(m_process, &QProcess::started, this, [this]() {
         // we to cache the pid in order to have it available after the process crashed
        m_pid = m_process->processId();
        emit started();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        emit errorOccured(static_cast<Am::ProcessError>(error));
    });
    connect(m_process, static_cast<void (QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        emit finished(exitCode, static_cast<Am::ExitStatus>(exitStatus));
    });
    connect(m_process, &QProcess::stateChanged,
            this, [this](QProcess::ProcessState newState) {
        emit stateChanged(static_cast<Am::RunState>(newState));
    });
    m_process->start(program, arguments);

    // we are forked now and the child process has received a copy of all redirected fds
    // now it's time to close our fds, since we don't need them anymore (plus we would block
    // the tty where they originated from)
    closeAndClearFileDescriptors(m_stdioRedirections);
}

void HostProcess::setWorkingDirectory(const QString &dir)
{
    m_process->setWorkingDirectory(dir);
}

void HostProcess::setProcessEnvironment(const QProcessEnvironment &environment)
{
    m_process->setProcessEnvironment(environment);
}

void HostProcess::kill()
{
    m_process->kill();
}

void HostProcess::terminate()
{
    m_process->terminate();
}

qint64 HostProcess::processId() const
{
    return m_pid;
}

Am::RunState HostProcess::state() const
{
    return static_cast<Am::RunState>(m_process->state());
}

void HostProcess::setStdioRedirections(QVector<int> &&stdioRedirections)
{
    // we own the file descriptors now
    closeAndClearFileDescriptors(m_stdioRedirections);
    m_stdioRedirections = stdioRedirections;

#if defined(Q_OS_UNIX)
    // make sure that the redirection fds do not have a close-on-exec flag, since we need them
    // in the child process.
    for (int fd : std::as_const(m_stdioRedirections)) {
        if (fd < 0)
            continue;
        int flags = fcntl(fd, F_GETFD);
        if (flags & FD_CLOEXEC)
            fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
    }
#endif
}

void HostProcess::setStopBeforeExec(bool stopBeforeExec)
{
    m_stopBeforeExec = stopBeforeExec;
}


ProcessContainer::ProcessContainer(ProcessContainerManager *manager, Application *app,
                                   QVector<int> &&stdioRedirections,
                                   const QMap<QString, QString> &debugWrapperEnvironment,
                                   const QStringList &debugWrapperCommand)
    : AbstractContainer(manager, app)
    , m_stdioRedirections(stdioRedirections)
    , m_debugWrapperEnvironment(debugWrapperEnvironment)
    , m_debugWrapperCommand(debugWrapperCommand)
{ }

ProcessContainer::~ProcessContainer()
{
    closeAndClearFileDescriptors(m_stdioRedirections);
}

QString ProcessContainer::controlGroup() const
{
    return m_currentControlGroup;
}

bool ProcessContainer::setControlGroup(const QString &groupName)
{
    if (groupName == m_currentControlGroup)
        return true;

    QVariantMap map = m_manager->configuration().value(u"controlGroups"_s).toMap();
    auto git = map.constFind(groupName);
    if (git != map.constEnd()) {
        QVariantMap mapping = (*git).toMap();
        QByteArray pidString = QByteArray::number(m_process->processId());
        pidString.append('\n');

        for (auto it = mapping.cbegin(); it != mapping.cend(); ++it) {
            const QString &resource = it.key();
            const QString &userclass = it.value().toString();

            //qWarning() << "Setting cgroup for" << m_program << ", pid" << m_process->processId() << ":" << resource << "->" << userclass;

            QString file = QString(u"/sys/fs/cgroup/%1/%2/cgroup.procs"_s).arg(resource, userclass);
            QFile f(file);
            bool ok = f.open(QFile::WriteOnly);
            ok = ok && (f.write(pidString) == pidString.size());

            if (!ok) {
                qWarning() << "Failed setting cgroup for" << m_program << ", pid" << m_process->processId() << ":" << resource << "->" << userclass;
                return false;
            }

            if (resource == u"memory") {
                if (!m_memWatcher) {
                    m_memWatcher = new MemoryWatcher(this);
                    connect(m_memWatcher, &MemoryWatcher::memoryLow,
                            this, &ProcessContainer::memoryLowWarning);
                    connect(m_memWatcher, &MemoryWatcher::memoryCritical,
                            this, &ProcessContainer::memoryCriticalWarning);
                }
                m_memWatcher->startWatching(userclass);
            }
        }
        m_currentControlGroup = groupName;
        emit controlGroupChanged(groupName);
        return true;
    }
    return false;
}

bool ProcessContainer::isReady()
{
    return true;
}

AbstractContainerProcess *ProcessContainer::start(const QStringList &arguments,
                                                  const QMap<QString, QString> &runtimeEnvironment,
                                                  const QVariantMap &amConfig)
{
    Q_UNUSED(amConfig)

    if (m_process) {
        qWarning() << "Process" << m_program << "is already started and cannot be started again";
        return nullptr;
    }
    if (!QFile::exists(m_program)) {
        qCWarning(LogSystem) << "Program" << m_program << "not found";
        return nullptr;
    }

    QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();

    for (auto it = runtimeEnvironment.cbegin(); it != runtimeEnvironment.cend(); ++it) {
        if (it.value().isEmpty())
            penv.remove(it.key());
        else
            penv.insert(it.key(), it.value());
    }
    for (auto it = m_debugWrapperEnvironment.cbegin(); it != m_debugWrapperEnvironment.cend(); ++it) {
        if (it.value().isEmpty())
            penv.remove(it.key());
        else
            penv.insert(it.key(), it.value());
    }

    HostProcess *process = new HostProcess();
    process->setWorkingDirectory(m_baseDirectory);
    process->setProcessEnvironment(penv);
    process->setStopBeforeExec(configuration().value(u"stopBeforeExec"_s).toBool());
    process->setStdioRedirections(std::move(m_stdioRedirections));

    QString command = m_program;
    QStringList args = arguments;

    if (!m_debugWrapperCommand.isEmpty()) {
        auto cmd = DebugWrapper::substituteCommand(m_debugWrapperCommand, m_program, arguments);

        command = cmd.takeFirst();
        args = cmd;
    }
    qCDebug(LogSystem) << "Running command:" << command << "arguments:" << args;

    process->start(command, args);
    m_process = process;

    setControlGroup(configuration().value(u"defaultControlGroup"_s).toString());
    return process;
}

ProcessContainerManager::ProcessContainerManager(QObject *parent)
    : AbstractContainerManager(defaultIdentifier(), parent)
{ }

ProcessContainerManager::ProcessContainerManager(const QString &id, QObject *parent)
    : AbstractContainerManager(id, parent)
{ }

QString ProcessContainerManager::defaultIdentifier()
{
    return u"process"_s;
}

bool ProcessContainerManager::supportsQuickLaunch() const
{
    return true;
}

AbstractContainer *ProcessContainerManager::create(Application *app, QVector<int> &&stdioRedirections,
                                                   const QMap<QString, QString> &debugWrapperEnvironment,
                                                   const QStringList &debugWrapperCommand)
{
    return new ProcessContainer(this, app, std::move(stdioRedirections), debugWrapperEnvironment,
                                debugWrapperCommand);
}

QT_END_NAMESPACE_AM


#include "moc_processcontainer.cpp"
