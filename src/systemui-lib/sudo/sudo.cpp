// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:privilege-management

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QSet>
#include <qplatformdefs.h>

#include "logging.h"
#include "sudo.h"
#include "utilities.h"
#include "unix-utilities.h"
#include "exception.h"

#include <cerrno>
#include <memory>

using namespace Qt::StringLiterals;

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#  include <QtCore/private/qcore_unix_p.h>

#  include "processtitle.h"
#  include "sudo/socketipc.h"

#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/errno.h>
#  include <sys/ioctl.h>
#  include <sys/stat.h>
#  include <sys/prctl.h>
#  include <sys/mount.h>
#  include <sys/syscall.h>
#  include <sys/wait.h>
#  include <sys/xattr.h>
#  include <sched.h>
#  include <thread>


// Declared as weak symbol here, so we can check at runtime if we were compiled against libgcov
extern "C" void __gcov_init() __attribute__((weak)); // NOLINT(reserved-identifier)


#ifndef OPEN_TREE_CLONE
#  define OPEN_TREE_CLONE 1
#endif
#ifndef OPEN_TREE_CLOEXEC
#  define OPEN_TREE_CLOEXEC O_CLOEXEC
#endif
#ifndef SYS_open_tree
#  define SYS_open_tree 428
#endif
#ifndef MOVE_MOUNT_F_EMPTY_PATH
#  define MOVE_MOUNT_F_EMPTY_PATH 0x00000004
#endif
#ifndef SYS_move_mount
#  define SYS_move_mount 429
#endif
#ifndef MOUNT_ATTR_RDONLY
#  define MOUNT_ATTR_RDONLY 0x00000001
#endif
#ifndef SYS_mount_setattr
#  define SYS_mount_setattr 442
#endif
#ifndef AT_RECURSIVE
#  define AT_RECURSIVE 0x8000
#endif
#ifndef AT_EMPTY_PATH
#  define AT_EMPTY_PATH 0x1000
#endif
#ifndef SYS_pidfd_open
#  define SYS_pidfd_open 434
#endif

#ifndef MOUNT_ATTR_SIZE_VER0
#  define MOUNT_ATTR_SIZE_VER0 32
struct mount_attr {
    __u64 attr_set;
    __u64 attr_clr;
    __u64 propagation;
    __u64 userns_fd;
};
#endif

QT_BEGIN_NAMESPACE_AM

static const char *setuidArg = nullptr;

static void checkSetuidArg(int argc, char *argv[], char *envp[])
{
    Q_UNUSED(envp)
    for (int i = 1; i < argc; ++i) {
        if (qstrncmp(argv[i], "--setuid", 8) == 0) {
            if (argv[i][8] == '=')
                setuidArg = argv[i] + 9;
            else if (!argv[i][8])
                setuidArg = ((i + 1) < argc) ? argv[++i] : "";
            break;
        }
    }
}

// register a .init function that is automatically run before main()
decltype(checkSetuidArg) *init_checkSetuidArg
    __attribute__((section(".init_array"), used)) = checkSetuidArg;

QT_END_NAMESPACE_AM

#endif // Q_OS_LINUX && !Q_OS_ANDROID


QT_BEGIN_NAMESPACE_AM

// Process-lifetime owners. The SocketIpc keeps the helper process alive on the client side; the
// fallback SudoServer is held here when no helper was forked. SudoClient::s_instance is a
// raw pointer that just borrows into one of these.
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
static std::unique_ptr<SocketIpc> s_sudoIpc;
#endif
static std::unique_ptr<SudoServer> s_sudoFallbackServer;


void Sudo::fallbackServer()
{
    if (SudoClient::instance()) {
        if (!SudoClient::instance()->isFallbackImplementation())
            throw Exception("Sudo::fallbackServer was called after Sudo::forkServer");
        return;
    }

    s_sudoFallbackServer = std::make_unique<SudoServer>();
    SudoClient::s_instance = new SudoClient(s_sudoFallbackServer.get());
}

void Sudo::forkServer(DropPrivileges dropPrivileges)
{
    if (SudoClient::instance()) {
        if (SudoClient::instance()->isFallbackImplementation())
            throw Exception("Sudo::forkServer was called after Sudo::fallbackServer");
        return;
    }

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    uid_t realUid = Unix::User::currentId();
    uid_t effectiveUid = Unix::User::currentEffectiveId();

    if (realUid != effectiveUid)
        throw Exception("Running as suid executable is not supported anymore");

    if (realUid != 0)
        return fallbackServer();

    if (!setuidArg)
        setuidArg = ::getenv("AM_SETUID");

    if ((realUid != 0) && setuidArg)
        throw Exception("Cannot use the --setuid argument or $AM_SETUID when not running as root");

    Unix::User setUser;
    Unix::Group setGroup;
    QSet<gid_t> setSupGids;

    // setuidArg is initialized in checkSetuidArg, before main()
    if (!setuidArg) {
        // If we are running under sudo, we can also use SUDO_UID and SUDO_GID. This is especially
        // important for auto-tests, as the testrunner does not like extra command line arguments.
        const QByteArray sudoUid = ::getenv("SUDO_UID");
        const QByteArray sudoGid = ::getenv("SUDO_GID");
        if (!sudoUid.isEmpty() && !sudoGid.isEmpty()) {
            try {
                setUser = Unix::User::parse(sudoUid);
                setGroup = Unix::Group::parse(sudoGid);

                if ((setUser.uid() == 0) || (setGroup.gid() == 0)) {
                    throw Exception("the user and group invoking sudo needs to be unprivileged (got: %1:%2)")
                        .arg(setUser.name()).arg(setGroup.name());
                }

                for (const auto *env : { "SUDO_UID", "SUDO_GID", "SUDO_USER", "SUDO_COMMAND", "SUDO_HOME", "SUDO_TTY" })
                    ::unsetenv(env);
            } catch (const Exception &e) {
                throw Exception("SUDO_UID/SUDO_GID: %1").arg(e.errorString());
            }
        } else {
            qCCritical(LogSystem) << "Running as root is not recommended! Please use "
                                     "--setuid=<user>[:<group>]*, set $AM_SETUID or use "
                                     "sudo to run as an unprivileged user";
        }
    } else {
        try {
            const auto list = QByteArray(setuidArg).trimmed().split(':');

            setUser = Unix::User::parse(list.at(0));
            setGroup = (list.size() >= 2) ? Unix::Group::parse(list.at(1))
                                          : Unix::Group::fromUser(setUser);
            const auto supGroups = list.mid(2);
            for (const auto &supGroup : supGroups)
                setSupGids << Unix::Group::parse(supGroup).gid();

            if (setSupGids.size() > Unix::Group::MaxSupplementary)
                throw Exception("too many supplementary groups, the maximum is %1").arg(Unix::Group::MaxSupplementary);

            if ((setUser.uid() == 0) || (setGroup.gid() == 0) || setSupGids.contains(0)) {
                throw Exception("user and group(s) need to be unprivileged (got: %1:%2, supplementary: %3)")
                    .arg(setUser.name()).arg(setGroup.name()).arg(setSupGids);
            }
        } catch (const Exception &e) {
            throw Exception("Error parsing --setuid / $AM_SETUID: %1").arg(e.errorString());
        }
    }

    auto ipcConfig = SocketIpcConfiguration::createSocketPair();

    // We need to make the gcda files generated by the root process writable by the normal user.
    // There is no way to detect a compilation with -ftest-coverage, but we can check for gcov
    // symbols at runtime. GCov will open all gcda files at fork() time, so we can get away with
    // switching umasks around the fork() call.

    mode_t realUmask = 0;
    if (__gcov_init)
        realUmask = ::umask(0);

    pid_t pid = ::fork();
    if (pid < 0) {
        throw Exception(errno, "Could not fork process");
    } else if (pid == 0) {
        // child process, this is now the sudo-helper
        qt_safe_close(0);
        ::setsid();

        // reset umask
        if (realUmask)
            ::umask(realUmask);

        // This call is Linux only, but it makes it so easy to detect a dying parent process.
        // We would have a big problem otherwise, since the main process drops its privileges,
        // which prevents it from sending SIGHUP to the child process, which still runs with
        // root privileges.
        ::prctl(PR_SET_PDEATHSIG, SIGHUP);
        ::signal(SIGHUP, [](int sig) { if (sig == SIGHUP) ::_exit(0); });

        try {
            // Drop as many capabilities as possible, just to be on the safe side
            using Cap = Unix::Capability::Cap;
            Unix::Capability::reduceTo({Cap::SysAdmin,
                                        Cap::SysChroot,
                                        Cap::SysPtrace,
                                        Cap::Chown,
                                        Cap::Fowner,
                                        Cap::DacOverride});

            // Ipc needs an event loop, so we need at least a QCoreApplication
            static char dummyArgv0[] = "sudo-helper";
            static char *dummyArgv[] = {dummyArgv0, nullptr};
            int dummyArgc = 1;
            qInstallMessageHandler(nullptr);
            QCoreApplication app(dummyArgc, dummyArgv);
            ProcessTitle::setTitle("sudo helper");

            SocketIpc server(std::move(ipcConfig), SocketIpc::Role::Server);
            server.registerSingleton(std::make_unique<SudoServer>());
            server.start();

            ::_exit(app.exec());
        } catch (const Exception &e) {
            qCCritical(LogSystem) << "Failed to start sudo helper:" << e.what();
            ::_exit(1);
        }
    }

    // parent process, this is the main process
    try {
        // reset umask
        if (realUmask)
            ::umask(realUmask);

        if (setUser.isValid() && setGroup.isValid()) {
            // combine the user's supplementary groups with the additonal groups given to --setuid:
            auto supGids = setUser.supplementaryGroupIds(setGroup);
            setSupGids.unite(supGids);
            setSupGids.remove(setGroup.gid());
            if (setSupGids.size() > Unix::Group::MaxSupplementary) {
                throw Exception("Too many supplementary groups when combining the groups of user "
                                "%1 with the ones specified for --setuid / $AM_SETUID")
                    .arg(setUser.name());
            }
            Unix::User::setCurrentSupplementaryGroupIds(setSupGids);

            // drop all root privileges
            const bool dropPermanently = (dropPrivileges == DropPrivilegesPermanently);
            setGroup.setCurrent(dropPermanently);
            setUser.setCurrent(dropPermanently);

            if (!dropPermanently) {
                qCCritical(LogSystem) << "\nSudo was instructed to NOT drop root privileges permanently.\n"
                                         "This is dangerous and should only be used in auto-tests!\n";
            }

            // Fix env variables
            // ::system("env"); // for testing
            ::setenv("HOME", setUser.dir(), 1);
            ::setenv("USER", setUser.name(), 1);
            ::setenv("LOGNAME", setUser.name(), 1);
            ::setenv("SHELL", setUser.shell(), 1);
            QByteArray xdgRTD = ::getenv("XDG_RUNTIME_DIR");
            if (xdgRTD.endsWith("/0")) {
                xdgRTD.chop(1);
                xdgRTD.append(QByteArray::number(setUser.uid()));
                ::setenv("XDG_RUNTIME_DIR", xdgRTD.constData(), 1);
            }
            // We are NOT changing to the user's home dir on purpose to avoid overriding a systemd setting

            qCInfo(LogSystem).nospace() << "The sudo-helper process is active and the main process is now running as "
                                        << setUser.name() << ":" << setGroup.name();
        }

        s_sudoIpc = std::make_unique<SocketIpc>(std::move(ipcConfig), SocketIpc::Role::Client);

        // Every sudo call has a 60 s soft deadline; three back-to-back expirations are treated
        // as "sudo-helper has become unresponsive" and qFatal the main process
        s_sudoIpc->setRequestTimeout(std::chrono::seconds(60));
        s_sudoIpc->setMaxConsecutiveTimeouts(3);

        // SocketIpc::start() needs to be called after the QCoreApplication constructor, because it
        // instantiates a QEventLoop, so this needs to be delayed to startServer() below.

    } catch (const Exception &e) {
        ::kill(pid, SIGKILL);
        throw;
    }

    // Make sure the main process dies when the helper process dies.
    // The other way around is handled by PR_SET_PDEATHSIG above.
    std::thread watcher([pid]() {
        int status = 0;
        if (qt_safe_waitpid(pid, &status, 0) > 0) {  // blocks until child exits
            qCCritical(LogSystem, "The sudo-helper process died with %s %d, so the main process needs to follow suit",
                WIFSIGNALED(status) ? "signal" : "exit code",
                WIFSIGNALED(status) ? WTERMSIG(status) : WEXITSTATUS(status));
            ::_exit(3);
        }
    });
    watcher.detach();
#else
    Q_UNUSED(dropPrivileges)
    return fallbackServer();
#endif // Q_OS_LINUX && !Q_OS_ANDROID
}

void Sudo::startServer()
{
    // The sudo-helper process dies implicitly through PR_SET_PDEATHSIG when this process dies
    // (which it should do after throwing exceptions here)

    if (SudoClient::instance())
        return;  // already started, or fallback already in place

    if (!QCoreApplication::instance())
        throw Exception("Sudo::startServer must be called after QCoreApplication is constructed");

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (s_sudoIpc) {
        s_sudoIpc->start();

        auto client = s_sudoIpc->bindSingleton<SudoClient>();
        if (!client)
            throw Exception("Could not bind to the SudoServer singleton");
        SudoClient::s_instance = client.release(); // owned for process lifetime
        return;
    }
#endif
    throw Exception("Sudo::startServer was called before fallbackServer or forkServer");
}


SudoInterface::SudoInterface(QObject *parent)
    : QObject(parent)
{ }


SudoClient *SudoClient::s_instance = nullptr;

SudoClient *SudoClient::instance()
{
    return s_instance;
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
SudoClient::SudoClient(SocketIpc *ipc)
    : m_ipc(ipc)
{ }
#endif

SudoClient::SudoClient(SudoServer *fallback)
    : m_fallback(fallback)
{ }

bool SudoClient::isFallbackImplementation() const
{
    return (m_fallback);
}

void SudoClient::removeRecursive(const QString &fileOrDir)
{
    if (m_fallback) {
        m_fallback->removeRecursive(fileOrDir);
        return;
    }
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (!m_ipc)
        throw Exception("Sudo IPC connection is no longer available");
    m_ipc->invokeMethod<void>(this, __func__, fileOrDir);
#else
    throw Exception("Sudo IPC is only available on Linux");
#endif
}

void SudoClient::bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                                     quint64 namespacePid, quint64 namespacePidInode)
{
    if (m_fallback) {
        m_fallback->bindMountFileSystem(from, to, readOnly, namespacePid, namespacePidInode);
        return;
    }
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (!m_ipc)
        throw Exception("Sudo IPC connection is no longer available");
    m_ipc->invokeMethod<void>(this, __func__, from, to, readOnly, namespacePid, namespacePidInode);
#else
    throw Exception("Sudo IPC is only available on Linux");
#endif
}

void SudoClient::setExtendedAttribute(const QString &file, const QByteArray &attrName, const QByteArray &attrValue)
{
    if (m_fallback) {
        m_fallback->setExtendedAttribute(file, attrName, attrValue);
        return;
    }
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (!m_ipc)
        throw Exception("Sudo IPC connection is no longer available");
    m_ipc->invokeMethod<void>(this, __func__, file, attrName, attrValue);
#else
    throw Exception("Sudo IPC is only available on Linux");
#endif
}


SudoServer::SudoServer(QObject *parent)
    : SudoInterface(parent)
{ }

void SudoServer::removeRecursive(const QString &fileOrDir)
{
    if (!recursiveOperation(fileOrDir, safeRemove))
        throw Exception(errno, "could not recursively remove %1").arg(fileOrDir);
}

void SudoServer::bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                                     quint64 namespacePid, quint64 namespacePidInode)
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    int oldNsFd = -1;
    auto nsRestore = qScopeGuard([&] {
        if ((oldNsFd >= 0) && namespacePid) {
            if (::setns(oldNsFd, CLONE_NEWNS) < 0)
                qFatal() << "SudoHelper process is halted: could not reset the mount namespace:" << strerror(errno);
        }
    });

    // Create a detached mount point for our source location
    int fromFd = int(::syscall(SYS_open_tree, -EBADF, qPrintable(from), OPEN_TREE_CLOEXEC | OPEN_TREE_CLONE));
    if (fromFd < 0)
        throw Exception(errno, "could not create a detached mount point for %1").arg(from);

    if (readOnly) {
        ::mount_attr mountAttr { MOUNT_ATTR_RDONLY, 0, 0, 0 };
        if (::syscall(SYS_mount_setattr, fromFd, "", AT_EMPTY_PATH | AT_RECURSIVE, &mountAttr, sizeof(mountAttr)) < 0)
            throw Exception(errno, "could not set the detached mount point for %1 to read-only").arg(from);
    }

    if (namespacePid) {
        // Save our current mount namespace to be able to restore it later
        oldNsFd = open("/proc/self/ns/mnt", O_RDONLY);
        if (oldNsFd < 0)
            throw Exception(errno, "could not open our own mount namespace");

        Unix::Fd pidFd { int(::syscall(SYS_pidfd_open, pid_t(namespacePid), 0)) };
        if (!pidFd)
            throw Exception(errno, "process %1 is not available").arg(namespacePid);

        if (isPidFileSystemSupported()) {
            // pidfs (Linux 6.9+) available: verify the pid still refers to the caller-captured
            // generation by comparing the pidfs inode.
            struct ::stat st;
            if (::fstat(pidFd.get(), &st) != 0)
                throw Exception(errno, "could not fstat the pidfd for process %1").arg(namespacePid);
            if (quint64(st.st_ino) != namespacePidInode) {
                throw Exception("process %1 generation mismatch (expected pidfd inode %2, got %3) "
                                "- pid recycle detected")
                    .arg(namespacePid).arg(namespacePidInode).arg(quint64(st.st_ino));
            }
        } else {
            // Legacy kernel: pidfds have no stable inode, so no recycle protection is possible.
            // The caller should have left the inode at 0 in this case.
            if (namespacePidInode != 0) {
                throw Exception("pidfs is not supported on this kernel but caller supplied a "
                                "non-zero pidfd inode (%1) - caller/helper mismatch")
                    .arg(namespacePidInode);
            }
        }

        if (::setns(pidFd.get(), CLONE_NEWNS) < 0)
            throw Exception(errno, "could not enter the mount namespace of process %1").arg(namespacePid);
    }

    // Mount the detached mount point to the final location within the mount namespace
    if (::syscall(SYS_move_mount, fromFd, "", -EBADF, qPrintable(to), MOVE_MOUNT_F_EMPTY_PATH) < 0)
        throw Exception(errno, "could not move the detached mount point to %1").arg(to);
#else
    Q_UNUSED(from)
    Q_UNUSED(to)
    Q_UNUSED(readOnly)
    Q_UNUSED(namespacePid)
    Q_UNUSED(namespacePidInode)
    throw Exception("bindMountFileSystem is only available on Linux");
#endif // Q_OS_LINUX && !Q_OS_ANDROID
}

void SudoServer::setExtendedAttribute(const QString &file, const QByteArray &attrName, const QByteArray &attrValue)
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (::setxattr(qPrintable(file), attrName.constData(), attrValue.constData(), attrValue.size(), 0) != 0)
        throw Exception(errno, "could not set extended attribute '%1' on file '%2'").arg(attrName).arg(file);
#else
    Q_UNUSED(file)
    Q_UNUSED(attrName)
    Q_UNUSED(attrValue)
    throw Exception("setExtendedAttribute is only available on Linux");
#endif
}

QT_END_NAMESPACE_AM
