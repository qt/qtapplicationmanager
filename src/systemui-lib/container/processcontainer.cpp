// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:execute-external-code

#include <QProcess>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QUuid>

#include "logging.h"
#include "utilities.h"
#include "containerfactory.h"
#include "application.h"
#include "processcontainer.h"
#include "debugwrapper.h"
#include "unixsignalhandler.h"
#include <mutex>

#if defined(Q_OS_UNIX)
#  include <csignal>
#  include <iostream>
#  include <unistd.h>
#  include <fcntl.h>
#endif
#if defined(Q_OS_LINUX)
#  include <sys/syscall.h>
#endif

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM

HostProcess::HostProcess(const QByteArray &cgroupProcsPath)
    : m_process(new QProcess)
{
    m_process->setProcessChannelMode(QProcess::ForwardedChannels);
    m_process->setInputChannelMode(QProcess::ForwardedInputChannel);
#if defined(Q_OS_UNIX)
    m_process->setChildProcessModifier([this, cgroupProcsPath]() {
        if (m_stopBeforeExec) {
            std::cerr << "\n*** a 'process' container was started in stopped state ***\n"
                         "The process is suspended via SIGSTOP and you can attach a debugger to it via\n"
                         "\n   gdb -p " << ::getpid() << "\n\n";
            ::raise(SIGSTOP);
        }

        if (!cgroupProcsPath.isEmpty()) {
            Unix::Fd fd { ::open(cgroupProcsPath.constData(), O_WRONLY | O_CLOEXEC) };
            if (!fd)
                m_process->failChildProcessModifier("open cgroup.procs", errno);
            // Writing "0" moves the calling (child) process into the cgroup
            static const char zero[] = { '0', '\n' };
            if (::write(*fd, zero, sizeof(zero)) != sizeof(zero))
                m_process->failChildProcessModifier("write cgroup.procs", errno);
        }

        // duplicate any requested redirections to the respective stdin/out/err fd. Also make sure to
        // close the original fd: otherwise we would block the tty where the fds originated from.
        for (int i = 0; i < 3; ++i) {
            int fd = m_stdioRedirections.value(i, -1);
            if (fd >= 0) {
                ::dup2(fd, i);
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
         // we need to cache the pid in order to have it available after the process crashed
        m_pid = m_process->processId();
#if defined(Q_OS_LINUX)
        // open a pidfd in the parent so we can reliably identify the child later
        m_pidFd.reset(int(::syscall(SYS_pidfd_open, m_pid, 0)));
#endif
        emit started();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            qCWarning(LogSystem, "Failed to start process: %s", qPrintable(m_process->errorString()));
        emit errorOccured(static_cast<Am::ProcessError>(error));
    });
    connect(m_process, &QProcess::finished,
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        Am::ExitStatus status = Am::NormalExit;
        if (exitStatus == QProcess::CrashExit) {
            if ((exitCode == SIGTERM || exitCode == SIGKILL))
                status = Am::ForcedExit;
            else if (exitCode == UnixSignalHandler::watchdogSignal())
                status = Am::WatchdogExit;
            else
                status = Am::CrashExit;
        }
        emit finished(exitCode, status);
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

void HostProcess::stop(Am::ExitStatus exitStatus)
{
    switch (exitStatus) {
    case Am::NormalExit:
        m_process->terminate();
        break;
    case Am::ForcedExit:
        m_process->kill();
        break;
    case Am::CrashExit:
        if (auto pid = m_process->processId())
            ::kill((pid_t) pid, SIGSEGV);
        break;
    case Am::WatchdogExit:
        if (auto pid = m_process->processId()) {
            if (int sig = UnixSignalHandler::watchdogSignal())
                ::kill((pid_t) pid, sig);
        }
        break;
    }
}

qint64 HostProcess::processId() const
{
    return m_pid;
}

int HostProcess::processFd() const
{
    return m_pidFd.get();
}

Am::RunState HostProcess::state() const
{
    return static_cast<Am::RunState>(m_process->state());
}

void HostProcess::setStdioRedirections(QVector<int> &&stdioRedirections)
{
    // we own the file descriptors now
    closeAndClearFileDescriptors(m_stdioRedirections);
    m_stdioRedirections = std::move(stdioRedirections);

#if defined(Q_OS_UNIX)
    // make sure that the redirection fds do not have a close-on-exec flag, since we need them
    // in the child process.
    for (int fd : std::as_const(m_stdioRedirections)) {
        if (fd < 0)
            continue;
        int flags = ::fcntl(fd, F_GETFD);
        if (flags & FD_CLOEXEC)
            ::fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
    }
#endif
}

void HostProcess::setStopBeforeExec(bool stopBeforeExec)
{
    m_stopBeforeExec = stopBeforeExec;
}


bool ProcessContainer::s_hasCGroupV2 = false;

ProcessContainer::ProcessContainer(ProcessContainerManager *manager, Application *app,
                                   QVector<int> &&stdioRedirections,
                                   const QMap<QString, QString> &debugWrapperEnvironment,
                                   const QStringList &debugWrapperCommand)
    : AbstractContainer(manager, app)
    , m_stdioRedirections(std::move(stdioRedirections))
    , m_debugWrapperEnvironment(debugWrapperEnvironment)
    , m_debugWrapperCommand(debugWrapperCommand)
{
#if defined(Q_OS_LINUX)
    static std::once_flag once;
    std::call_once(once, [] {
        s_hasCGroupV2 = QFile::exists(testRootPathPrefix() + u"/sys/fs/cgroup/cgroup.controllers"_s);
    });
#endif
}

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
    if (!s_hasCGroupV2)
        return false;

    if (groupName == m_currentControlGroup)
        return true;

    // cgroup-v2 nested groups use '/', but '..' or a leading '/' would escape /sys/fs/cgroup/
    if (groupName.startsWith(u'/') || groupName.split(u'/').contains(u".."_s)) {
        qCWarning(LogSystem) << "Refusing to set cgroup with invalid name:" << groupName;
        return false;
    }

    if (m_process->state() == Am::Running) {
        const QByteArray pidString = QByteArray::number(m_process->processId()) + '\n';

        QString procsFile = testRootPathPrefix() + u"/sys/fs/cgroup/"_s + groupName + u"/cgroup.procs"_s;
        QFile f(procsFile);
        bool ok = f.open(QFile::WriteOnly);
        ok = ok && (f.write(pidString) == pidString.size());

        if (!ok) {
            qCWarning(LogSystem) << "Failed setting cgroup for" << m_program << ", pid"
                                 << m_process->processId() << "to" << groupName;
            return false;
        }
    }

    m_currentControlGroup = groupName;
    emit controlGroupChanged(groupName);
    return true;
}

bool ProcessContainer::isReady()
{
    return true;
}

bool ProcessContainer::hasDebugWrapper() const
{
    return !m_debugWrapperCommand.isEmpty();
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

    QString controlGroup = configuration().value(u"defaultControlGroup"_s).toString();
    bool createControlGroupPerProcess = configuration().value(u"createControlGroupPerProcess"_s).toBool();
    if (createControlGroupPerProcess) {
        // Do we really want to use the app id if possible only only fall back to uuid when quick-launch is used ?
        if (auto app = application())
            controlGroup += u"/"_s + app->id();
        else
            controlGroup += u"/app-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    QString cgroupDir;
    QByteArray cgroupProcsPath;
    if (!controlGroup.isEmpty()) {
        cgroupDir = testRootPathPrefix() + u"/sys/fs/cgroup/"_s + controlGroup;
        // mkpath succeeds if the cgroup already exists, so this only fails if we really cannot use
        // it. Starting the process is still attempted: it will fail when joining the cgroup, but
        // this warning tells the user what the actual problem was.
        if (!QDir().mkpath(cgroupDir))
            qCWarning(LogSystem) << "Failed to create cgroup" << cgroupDir;
        cgroupProcsPath = QString(cgroupDir + u"/cgroup.procs"_s).toLocal8Bit();

        // Under test (testRootPathPrefix set), the faked cgroupfs has no kernel-created
        // cgroup.procs; wait for the test to create it before the child tries to join, as the
        // child opens it O_WRONLY without O_CREAT. No-op in production (prefix empty) and when the
        // file already exists (a pre-existing, shared group).
        if (!testRootPathPrefix().isEmpty()) {
            QElapsedTimer timer;
            timer.start();
            while (!QFile::exists(QString::fromLocal8Bit(cgroupProcsPath))
                   && (timer.elapsed() < (5000 * timeoutFactor()))) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            }
        }
    }

    auto *process = new HostProcess(cgroupProcsPath);
    process->setWorkingDirectory(m_baseDirectory);
    process->setProcessEnvironment(penv);
    process->setStopBeforeExec(configuration().value(u"stopBeforeExec"_s).toBool());
    process->setStdioRedirections(std::move(m_stdioRedirections));

    connect(process, &HostProcess::finished, this, [createControlGroupPerProcess, cgroupDir]() {
        // Only delete the cgroup if it is not shared with other apps
        if (createControlGroupPerProcess && !cgroupDir.isEmpty()) {
            if (::rmdir(cgroupDir.toLocal8Bit().constData()) != 0) {
                qCWarning(LogSystem) << "Failed to remove cgroup" << cgroupDir << ":"
                                     << qt_error_string(errno);
            }
        }
    });

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
    setControlGroup(controlGroup);


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
