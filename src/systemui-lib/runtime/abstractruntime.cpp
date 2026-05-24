// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QHash>
#include <QUuid>

#if defined(Q_OS_LINUX)
#  include <sys/stat.h>
#  include <sys/syscall.h>
#  include <unistd.h>
#endif

#include "logging.h"
#include "application.h"
#include "abstractruntime.h"
#include "abstractcontainer.h"
#include "globalruntimeconfiguration.h"
#include "exception.h"
#include "utilities.h"

/*!
    \qmltype Runtime
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief The handle for a runtime that is executing an application.

    While an \l{ApplicationObject}{Application} is running, the associated Runtime object will be
    valid and yield access to runtime related information.
*/
/*!
    \qmlproperty Container Runtime::container
    \readonly

    This property returns the \l Container object of a running application. Please see the \l{Containers}
    {general Container} and the \l Container class documentation for more information on containers
    within the application manager.
*/

QT_BEGIN_NAMESPACE_AM

AbstractRuntime::AbstractRuntime(AbstractContainer *container, Application *app, AbstractRuntimeManager *manager)
    : QObject(manager)
    , m_container(container)
    , m_app(app)
    , m_manager(manager)
{
    m_securityToken = QUuid::createUuid().toRfc4122().toHex();
    Q_STATIC_ASSERT(SecurityTokenSize == 2 * sizeof(QUuid)); // hex encoding doubles the size
    Q_ASSERT(m_securityToken.size() == SecurityTokenSize);

    AbstractRuntimeManager::s_allRuntimes.append(this);
}

QVariantMap AbstractRuntime::configuration() const
{
    if (m_manager)
        return m_manager->configuration();
    return { };
}

QVariantMap AbstractRuntime::systemProperties() const
{
    if (m_app) {
        const auto &grc = GlobalRuntimeConfiguration::instance();
        return m_app->isBuiltIn() ? grc.systemPropertiesForBuiltInApps
                                  : grc.systemPropertiesForThirdPartyApps;
    }
    return { };
}

RuntimeSignaler *AbstractRuntime::signaler()
{
    static RuntimeSignaler rs;
    return &rs;
}

QByteArray AbstractRuntime::securityToken() const
{
    return m_securityToken;
}

void AbstractRuntime::openDocument(const QString &document, const QString &mimeType)
{
    Q_UNUSED(document)
    Q_UNUSED(mimeType)
}

void AbstractRuntime::setSlowAnimations(bool slow)
{
    // not every runtime needs this information
    Q_UNUSED(slow)
}

void AbstractRuntime::setApplicationExtraDirs(const QMap<QString, QString> &extraPaths)
{
    Q_UNUSED(extraPaths);
}

Application *AbstractRuntime::application() const
{
    return m_app.data();
}

AbstractRuntime::~AbstractRuntime()
{
    delete m_container;
    AbstractRuntimeManager::s_allRuntimes.removeOne(this);
}

AbstractRuntimeManager *AbstractRuntime::manager() const
{
    return m_manager;
}


/*!
    \qmlproperty string Runtime::runtimeId
    \readonly
    \since 6.10

    This property returns the \c id of the runtime that is executing the application. The \c id
    is a unique identifier for the runtime integration and can be used to reference it in other
    parts of the System UI or in configuration files.
*/
QString AbstractRuntime::runtimeId() const
{
    return m_manager->identifier();
}

bool AbstractRuntime::isQuickLauncher() const
{
    return false;
}

bool AbstractRuntime::attachApplicationToQuickLauncher(Application *app)
{
    Q_UNUSED(app)
    return false;
}

Am::RunState AbstractRuntime::state() const
{
    return m_state;
}

void AbstractRuntime::setState(Am::RunState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

void AbstractRuntime::setInProcessQmlEngine(QQmlEngine *engine)
{
    m_inProcessQmlEngine = engine;
}

QQmlEngine *AbstractRuntime::inProcessQmlEngine() const
{
    return m_inProcessQmlEngine;
}

AbstractContainer *AbstractRuntime::container() const
{
    return m_container;
}

QList<AbstractRuntime *> AbstractRuntimeManager::s_allRuntimes;

AbstractRuntimeManager::AbstractRuntimeManager(const QString &id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{ }

QString AbstractRuntimeManager::identifier() const
{
    return m_id;
}

bool AbstractRuntimeManager::inProcess() const
{
    return false;
}

bool AbstractRuntimeManager::supportsQuickLaunch() const
{
    return false;
}

QVariantMap AbstractRuntimeManager::configuration() const
{
    return m_configuration;
}

void AbstractRuntimeManager::setConfiguration(const QVariantMap &configuration)
{
    m_configuration = configuration;
}

/* \internal

    Returns the runtimes whose launcher process matches \a pid -- or any of its ancestors up
    to 5 levels deep, so indirect children (e.g. an app started under gdbserver) are matched too.

    On Linux 6.9+ with pidfs, identity is established by pidfd inode comparison, which is
    race-free across the pidfd's lifetime. If \a pidfd is -1 the function falls back to
    pidfd_open(pid) internally -- callers that can obtain a peer-pidfd at the kernel boundary
    (SO_PEERPIDFD on a connected socket) should pass it directly to also close the entry-window
    TOCTOU.

    On older kernels or non-Linux platforms the legacy pid+/proc walk is used, which has the
    documented pid-reuse TOCTOU on /proc reads.
*/
QList<AbstractRuntime *> AbstractRuntimeManager::fromProcessId(qint64 pid, int pidfd)
{
    QList<AbstractRuntime *> result;
    if (pid <= 0)
        return result;

    const qint64 appmanPid = QCoreApplication::applicationPid();

    if (isPidFileSystemSupported()) {
#if defined(Q_OS_LINUX)
        // Returns the inode number of a pidfd, uniquely identifying the process.
        static auto pidfdInode = [](int fd) -> quint64 {
            struct ::stat st { };
            return (::fstat(fd, &st) == 0) ? quint64(st.st_ino) : 0;
        };

        unique_fd ownedPidfd;
        if (pidfd < 0) {
            ownedPidfd.reset(int(::syscall(SYS_pidfd_open, pid, 0)));
            pidfd = ownedPidfd.get();
        }
        if (pidfd < 0)
            return result;

        // Snapshot the candidate runtimes' pidfd inodes once.
        QHash<quint64, AbstractRuntime *> candidates;
        for (AbstractRuntime *runtime : std::as_const(s_allRuntimes)) {
            AbstractContainer *container = runtime->container();
            AbstractContainerProcess *process = container ? container->process() : nullptr;
            const quint64 ino = pidfdInode(process ? process->processFd() : -1);
            if (ino != 0)
                candidates.insert(ino, runtime);
        }

        // Walk up to 5 ancestor levels, inode-comparing at each step.
        int level = 0;
        while ((pid > 1) && (pid != appmanPid) && (level < 5)) {
            const quint64 ino = pidfdInode(pidfd);
            if (ino != 0) {
                if (AbstractRuntime *runtime = candidates.value(ino, nullptr)) {
                    if (!result.contains(runtime))
                        result << runtime;
                }
            }
            pid = getParentPid(pid);
            ownedPidfd.reset(int(::syscall(SYS_pidfd_open, pid, 0)));
            pidfd = ownedPidfd.get();
            if (pidfd < 0)
                break;
            ++level;
        }
        return result;
#else
        Q_UNUSED(pidfd)
#endif
    } else {
        // Legacy path (pre-6.9 kernel, or no pidfd available)

        int level = 0;
        while ((pid > 1) && (pid != appmanPid) && (level < 5)) {
            for (AbstractRuntime *runtime : std::as_const(s_allRuntimes)) {
                if (result.contains(runtime))
                    continue;
                if (runtime->applicationProcessId() == pid)
                    result << runtime;
            }
            pid = getParentPid(pid);
            ++level;
        }
    }
    return result;
}

QT_END_NAMESPACE_AM

#include "moc_abstractruntime.cpp"
