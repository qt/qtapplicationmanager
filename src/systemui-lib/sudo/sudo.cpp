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
#include <QSocketNotifier>
#include <QUuid>
#include <qplatformdefs.h>

#include "logging.h"
#include "sudo.h"
#include "sudo_p.h"
#include "utilities.h"
#include "unix-utilities.h"
#include "exception.h"

#include <cerrno>
#include <memory>

using namespace Qt::StringLiterals;

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#  include <QtCore/private/qcore_unix_p.h>

#  include "processtitle.h"

#  include <sys/xattr.h>

#  if QT_CONFIG(am_multi_process)
#    include <QtCore/QTimer>
#    include <QtDBus/QDBusServer>
#    include <QtDBus/QDBusConnection>
#    include <QtDBus/QDBusError>
#    include <QtDBus/QDBusPendingReply>
#    include <QtDBus/QDBusUnixFileDescriptor>
#    include "dbus-utilities.h"
#    include "sudo_adaptor.h"
#    include "sudo_interface.h"
#  endif

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


#  if defined(QT_AM_COVERAGE)
extern "C" {
#    include <gcov.h>
}
#  endif

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

#if QT_CONFIG(am_multi_process)
// The read end of the inherited pipe over which the forked helper hands its P2P D-Bus address back
// to the main process. forkServer() fills it in; startServer() drains and closes it.
static Unix::Fd s_sudoAddressPipe;
#endif


void Sudo::fallbackServer()
{
    if (SudoClient::instance()) {
        if (!SudoClient::instance()->isFallbackImplementation())
            throw Exception("Sudo::fallbackServer was called after Sudo::forkServer");
        return;
    }

    auto *d = new SudoClientPrivate;
    d->isFallback = true;
    SudoClient::s_instance = new SudoClient(d);
}

/*! \internal
    Must be called before QCoreApplication is constructed.
*/
void Sudo::forkServer(DropPrivileges dropPrivileges)
{
    if (SudoClient::instance()) {
        if (SudoClient::instance()->isFallbackImplementation())
            throw Exception("Sudo::forkServer was called after Sudo::fallbackServer");
        return;
    }

#if defined(Q_OS_UNIX)
    uid_t realUid = Unix::User::currentId();
    uid_t effectiveUid = Unix::User::currentEffectiveId();

    if (realUid != effectiveUid)
        throw Exception("Running as suid executable is not supported anymore");

    if (realUid != 0)
        return fallbackServer();
#endif

#if !QT_CONFIG(am_multi_process)
    Q_UNUSED(dropPrivileges)
#  if defined(Q_OS_UNIX)
    if (realUid == 0)
        qCCritical(LogSystem) << "Running as root is not recommended!";
#  endif
    return fallbackServer();

#else
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

    // Topology: the privileged helper is the P2P D-Bus *server*; the main process connects in. The
    // helper picks a random server address and hands it back to the main process over this inherited
    // pipe. The main process's blocking read on it also doubles as a "helper is ready" barrier.
    int addressPipe[2] = { -1, -1 };
    if (qt_safe_pipe(addressPipe) != 0)
        throw Exception(errno, "Could not create the sudo-helper address pipe");

#  if defined(QT_AM_COVERAGE)
    // We need to make the gcda files generated by the root process writable by the normal user.
    // GCov will open all gcda files at fork() time, so we can get away with switching umasks
    // around the fork() call.
    mode_t realUmask = ::umask(0);
#  endif

    // Open a pidfd on ourselves before forking, so the helper can identify us
    const pid_t mainPid = ::getpid();
    Unix::Fd mainPidFd { int(::syscall(SYS_pidfd_open, mainPid, 0)) };

    pid_t helperPid = ::fork();
    if (helperPid < 0) {
        throw Exception(errno, "Could not fork process");
    } else if (helperPid == 0) {
        // child process, this is now the sudo-helper
        qt_safe_close(0);
        qt_safe_close(addressPipe[0]); // the helper only writes its address back
        ::setsid();

#  if defined(QT_AM_COVERAGE)
        ::umask(realUmask);  // reset umask, see above for explanation
#  endif

        // This call is Linux only, but it makes it so easy to detect a dying parent process.
        // We would have a big problem otherwise, since the main process drops its privileges,
        // which prevents it from sending SIGHUP to the child process, which still runs with
        // root privileges.
        ::prctl(PR_SET_PDEATHSIG, SIGHUP);
        ::signal(SIGHUP, [](int sig) {
            if (sig == SIGHUP) {
#  if defined(QT_AM_COVERAGE)
                __gcov_dump();
#  endif
                ::_exit(0);
            }
        });

        try {
            // Drop as many capabilities as possible, just to be on the safe side
            using Cap = Unix::Capability::Cap;
            Unix::Capability::reduceTo({Cap::SysAdmin,
                                        Cap::SysChroot,
                                        Cap::SysPtrace,
                                        Cap::Chown,
                                        Cap::Fowner,
                                        Cap::DacOverride});

            // QtDBus needs an event loop, so we need at least a QCoreApplication
            static char dummyArgv0[] = "sudo-helper";
            static char *dummyArgv[] = { dummyArgv0, nullptr };
            int dummyArgc = 1;
            qInstallMessageHandler(nullptr);
            QCoreApplication app(dummyArgc, dummyArgv);
            ProcessTitle::setTitle("sudo helper");

            auto sudoServer = new SudoServer(&app);
            SudoAdaptor *sudoAdaptor = nullptr;

            // Listen on a random abstract-namespace socket: it leaves no filesystem artifact and
            // the address is handed to the main process over addressPipe. The name is not secret
            // (it shows up in /proc/net/unix), so access control is the peer-identity check in the
            // newConnection handler below.
            const QString listenAddress = u"unix:abstract=qtapplicationmanager-sudo-%1"_s
                                              .arg(QUuid::createUuid().toString(QUuid::Id128));
            QDBusServer dbusServer(listenAddress);
            dbusServer.setAnonymousAuthenticationAllowed(true);
            if (!dbusServer.isConnected()) {
                throw Exception("could not create the sudo-helper P2P D-Bus server: %1")
                    .arg(dbusServer.lastError().message());
            }

            QObject::connect(&dbusServer, &QDBusServer::newConnection, &app,
                             [sudoServer, &sudoAdaptor, mainPid, &mainPidFd](const QDBusConnection &connection) {
                // We accept exactly one verified connection and refuse every later one: once the real
                // main process is connected there is never a second legitimate peer.
                if (sudoAdaptor) {
                    qCCritical(LogSystem) << "sudo-helper refused an additional P2P connection";
                    ::_exit(4);
                }

                sudoAdaptor = new SudoAdaptor(sudoServer);

                bool verified = false;
                auto [peerPid, peerPidFd] = getDBusPeerPidAndFd(connection);

                if (isPidFileSystemSupported()) {
                    struct ::stat peerSt { }, mainSt { };
                    verified = (peerPidFd)
                               && (::fstat(peerPidFd.get(), &peerSt) == 0)
                               && (::fstat(mainPidFd.get(), &mainSt) == 0)
                               && (quint64(peerSt.st_ino) == quint64(mainSt.st_ino));
                } else {
                    // Legacy path (pre-6.9 kernel, or no pidfd available)
                    verified = (peerPid > 0) && (peerPid == mainPid);
                }

                if (!verified) {
                    qCCritical(LogSystem) << "sudo-helper rejected a P2P connection from pid" << peerPid;
                    ::_exit(4);
                }

                if (!QDBusConnection(connection).registerObject(u"/Sudo"_s, sudoServer, QDBusConnection::ExportAdaptors)) {
                    qCCritical(LogSystem) << "sudo-helper could not register the /Sudo object";
                    ::_exit(4);
                }
            });

            // Hand the address back, then close the pipe so the main process sees EOF.
            const QByteArray address = dbusServer.address().toUtf8();
            if (qt_safe_write(addressPipe[1], address.constData(), address.size()) != address.size())
                throw Exception(errno, "could not send the sudo-helper D-Bus address to the main process");
            qt_safe_close(addressPipe[1]);

            ::_exit(app.exec());
        } catch (const Exception &e) {
            qCCritical(LogSystem) << "Failed to start sudo helper:" << e.what();
            ::_exit(1);
        }
        // This point in the sudo-helper process should never be reached
    }

    // parent process, this is the main process

    // Close our copy of the write end so that the read in startServer() will see EOF once
    // the helper has sent its address, and keep the read end around.
    qt_safe_close(addressPipe[1]);
    s_sudoAddressPipe.reset(addressPipe[0]);

    // Drop privileges and fix up the environment accordingly
    try {
#  if defined(QT_AM_COVERAGE)
        ::umask(realUmask); // reset umask, see above for explanation
#  endif

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
            } else if (xdgRTD.isEmpty()) {
                // XDG_RUNTIME_DIR is not set, but Wayland requires it, so set it to a sane default
                xdgRTD = "/run/user/" + QByteArray::number(setUser.uid());
                ::setenv("XDG_RUNTIME_DIR", xdgRTD.constData(), 1);
            }
            // We are NOT changing to the user's home dir on purpose to avoid overriding a systemd setting

            qCInfo(LogSystem).nospace() << "The sudo-helper process is active and the main process is now running as "
                                        << setUser.name() << ":" << setGroup.name();
        }

        // The actual D-Bus connection to the helper needs a running event loop, so it is deferred
        // to startServer() below (called after the QCoreApplication constructor).

    } catch (const Exception &e) {
        ::kill(helperPid, SIGKILL);
        throw;
    }

    // Make sure the main process dies when the helper process dies.
    // The other way around is handled by the helper's mainPidFd watch above.
    std::thread watcher([helperPid]() {
        int status = 0;
        if (qt_safe_waitpid(helperPid, &status, 0) > 0) {  // blocks until child exits
            qCCritical(LogSystem, "The sudo-helper process died with %s %d, so the main process needs to follow suit",
                WIFSIGNALED(status) ? "signal" : "exit code",
                WIFSIGNALED(status) ? WTERMSIG(status) : WEXITSTATUS(status));
            ::_exit(3);
        }
    });
    watcher.detach();
#endif // QT_CONFIG(am_multi_process)
}

/*! \internal
    Must be called after QCoreApplication is constructed.
*/
void Sudo::startServer()
{
    if (SudoClient::instance())
        return;  // already started, or fallback already in place

    if (!QCoreApplication::instance())
        throw Exception("Sudo::startServer must be called after QCoreApplication is constructed");

#if QT_CONFIG(am_multi_process)
    if (s_sudoAddressPipe) {
        // Get the helper's P2P D-Bus address from the inherited pipe. This blocks until the helper
        // has written it and closed its write end, so it also acts as a "helper is ready" barrier.
        QByteArray address;
        address.resize(2048); // if that's not enough then libdbus is broken
        int n = qt_safe_read(s_sudoAddressPipe.get(), address.data(), address.size());
        if (n < 0)
            throw Exception(errno, "failed to read the sudo-helper D-Bus address");
        else if (n >= 0)
            address.resize(n);

        s_sudoAddressPipe.reset();

        if (address.isEmpty())
            throw Exception("the sudo-helper did not provide a D-Bus address");

        // Give the connection an obscure name, so nobody can accidentally send on it.
        auto connection = QDBusConnection::connectToPeer(QString::fromUtf8(address),
                                                         QUuid::createUuid().toString());
        if (!connection.isConnected()) {
            throw Exception("could not connect to the sudo-helper P2P D-Bus: %1")
                .arg(connection.lastError().message());
        }

        // The helper registers /Sudo synchronously in its newConnection handler, i.e. before it
        // dispatches any method call on this connection, so the proxy is usable immediately.
        auto *d = new SudoClientPrivate;
        d->iface = new IoQtApplicationManagerSudoInterface(QString(), u"/Sudo"_s, connection);
        SudoClient::s_instance = new SudoClient(d); // owned for process lifetime; parents the proxy
        return;
    }
#endif // QT_CONFIG(am_multi_process)
    throw Exception("Sudo::startServer was called before fallbackServer or forkServer");
}


/////////////////////////////////////////////////////////////////////
// SudoClient
/////////////////////////////////////////////////////////////////////


SudoClient *SudoClient::s_instance = nullptr;

SudoClient *SudoClient::instance()
{
    return s_instance;
}

SudoClient::SudoClient(SudoClientPrivate *dd)
    : d(dd)
{
#if QT_CONFIG(am_multi_process)
    if (d->iface)
        d->iface->setParent(this);
#endif
}

SudoClient::~SudoClient() = default;

bool SudoClient::isFallbackImplementation() const
{
    return d->isFallback;
}

#if QT_CONFIG(am_multi_process)

template<typename T>
static T checkDBusReply(QDBusPendingReply<T> &&reply, const char *operation)
{
    reply.waitForFinished();

    if (!reply.isError()) {
        if constexpr (std::is_same_v<T, void>)
             return;
        else
            return reply.value();
    }

    const QDBusError error = reply.error();

    // "Failed" errors are generated by us, when an exception is thrown in the sudo-helper.
    if (error.type() == QDBusError::Failed)
        throw Exception(error.message());

    // Any other error means we lost the connection to the helper process.
    qFatal("Sudo: lost the connection to the sudo-helper process during '%s': %s [%s]",
           operation, qPrintable(error.message()), qPrintable(error.name()));
}

#endif // QT_CONFIG(am_multi_process)

/*! \internal
    In fallback mode (no root helper), this runs in-process.
*/
void SudoClient::removeRecursive(const QString &fileOrDir)
{
    if (d->isFallback) {
        if (!recursiveOperation(fileOrDir, safeRemove))
            throw Exception(errno, "could not recursively remove %1").arg(fileOrDir);
        return;
    }
#if QT_CONFIG(am_multi_process)
    if (d->iface)
        return checkDBusReply<void>(d->iface->removeRecursive(fileOrDir), __func__);
#endif
    throw Exception("The sudo-helper process is not available.");
}

/*! \internal
    \a namespacePidFd is an already-open pidfd for the process whose mount namespace to enter, or -1
    to mount in the helper's own namespace. It is forwarded to the helper over D-Bus. In fallback mode
    this throws: it has no meaning without the privileged helper.
*/
void SudoClient::bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                                     int namespacePidFd)
{
    if (d->isFallback)
        throw Exception("bindMountFileSystem requires the root sudo-helper, which is not running");

#if QT_CONFIG(am_multi_process)
    if (d->iface) {
        // QDBusUnixFileDescriptor can only transport valid file descriptors, so we need to open
        // a placeholder fd when namespacePidFd is -1 and we need a useNamespacePidFd parameter to
        // tell the helper to ignore it.
        const bool useNamespacePidFd = (namespacePidFd >= 0);
        Unix::Fd placeholder;
        int fd = namespacePidFd;
        if (!useNamespacePidFd) {
            placeholder = Unix::Fd { int(::syscall(SYS_pidfd_open, ::getpid(), 0)) };
            if (!placeholder)
                throw Exception(errno, "could not open our own pidfd");
            fd = placeholder.get();
        }

        return checkDBusReply<void>(d->iface->bindMountFileSystem(from, to, readOnly, useNamespacePidFd,
                                                                  QDBusUnixFileDescriptor(fd)),
                                                                  __func__);
    }
#else
    Q_UNUSED(from)
    Q_UNUSED(to)
    Q_UNUSED(readOnly)
    Q_UNUSED(namespacePidFd)
#endif // QT_CONFIG(am_multi_process)
    throw Exception("The sudo-helper process is not available.");
}

/*! \internal
    In fallback mode (no root helper), this runs in-process.
*/
void SudoClient::setExtendedAttribute(const QString &file, const QByteArray &attrName, const QByteArray &attrValue)
{
    if (d->isFallback) {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        if (::setxattr(qPrintable(file), attrName.constData(), attrValue.constData(), attrValue.size(), 0) != 0)
            throw Exception(errno, "could not set extended attribute '%1' on file '%2'").arg(attrName).arg(file);
        return;
#else
        throw Exception("Extended attributes are not supported on this platform");
#endif
    }
#if QT_CONFIG(am_multi_process)
    if (d->iface)
        return checkDBusReply<void>(d->iface->setExtendedAttribute(file, attrName, attrValue), __func__);
#else
    Q_UNUSED(file)
    Q_UNUSED(attrName)
    Q_UNUSED(attrValue)
#endif
    throw Exception("The sudo-helper process is not available.");
}


/////////////////////////////////////////////////////////////////////
// SudoServer
/////////////////////////////////////////////////////////////////////


#if QT_CONFIG(am_multi_process)

#define catchExceptionAsDBusError(...) catch(const Exception &e) { \
    sendErrorReply(QDBusError::Failed, e.errorString()); \
    return __VA_ARGS__; \
}

SudoServer::SudoServer(QObject *parent)
    : QObject(parent)
{
}

void SudoServer::removeRecursive(const QString &fileOrDir)
{
    try {
        if (!recursiveOperation(fileOrDir, safeRemove))
            throw Exception(errno, "could not recursively remove %1").arg(fileOrDir);
    } catchExceptionAsDBusError()
}

void SudoServer::bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                                     bool useNamespacePidFd,
                                     const QDBusUnixFileDescriptor &namespacePidFd)
{
    try {
        // Create a detached mount point for our source location
        Unix::Fd fromFd { int(::syscall(SYS_open_tree, -EBADF, qPrintable(from), OPEN_TREE_CLOEXEC | OPEN_TREE_CLONE)) };
        if (!fromFd)
            throw Exception(errno, "could not create a detached mount point for %1").arg(from);

        if (readOnly) {
            ::mount_attr mountAttr { MOUNT_ATTR_RDONLY, 0, 0, 0 };
            if (::syscall(SYS_mount_setattr, fromFd.get(), "", AT_EMPTY_PATH | AT_RECURSIVE, &mountAttr, sizeof(mountAttr)) < 0)
                throw Exception(errno, "could not set the detached mount point for %1 to read-only").arg(from);
        }

        if (useNamespacePidFd) {
            // namespacePidFd is the target's pidfd, opened and kept open by the caller (the container,
            // via the main process), so we enter that exact process's mount namespace - no re-open,
            // no pid-recycle window. The fd is borrowed and valid for this synchronous call only.
            const int pidFd = namespacePidFd.fileDescriptor();
            if (pidFd < 0)
                throw Exception("no pidfd was passed for the target mount namespace");

            // We need to enter the mount namespace of pidFd to do the actual mount, but this is risky
            // because we could fail to reset back to our original namespace and would then have no
            // other option than killing ourselves. To make matters worse, setns() fails on
            // multi-threaded processes and we are definitely multi-threaded, since QtDBus dispatches
            // on its own connection-manager thread. The solution: fork() a throw-away process that
            // does the setns() and the mount, and then immediately exits.

            const QByteArray toLocal = to.toLocal8Bit();

            int pipeFd[2] = { -1, -1 };
            if (qt_safe_pipe(pipeFd) < 0)
                throw Exception(errno, "could not create pipe for helper process of bindMountFileSystem");

            pid_t pid = ::fork();
            if (pid < 0) {
                qt_safe_close(pipeFd[0]);
                qt_safe_close(pipeFd[1]);
                throw Exception(errno, "could not fork helper process for bindMountFileSystem");
            } else if (pid == 0) {
                // child process, throw-away setns+mount

                qt_safe_close(pipeFd[0]);

                //NB: we just forked from a multi-threaded process, which means we need to be extra
                //    careful to not do anything that could cause deadlocks due to locks held by
                //    other threads at fork() time: throwing exceptions and QString operations are
                //    not possible. For the most part, we need to act like a signal handler here.

                // Enter the mount namespace of the target process
                if (::setns(pidFd, CLONE_NEWNS) < 0) {
                    int e = errno;
                    qt_safe_write(pipeFd[1], &e, sizeof(e));
                    ::_exit(1);
                }
                // Mount the detached mount point to the final location within the mount namespace
                if (::syscall(SYS_move_mount, fromFd.get(), "", -EBADF, toLocal.constData(), MOVE_MOUNT_F_EMPTY_PATH) < 0) {
                    int e = errno;
                    qt_safe_write(pipeFd[1], &e, sizeof(e));
                    ::_exit(2);
                }
                ::_exit(0);
            } else {
                // parent process, sudo-helper

                qt_safe_close(pipeFd[1]);
                Unix::Fd readPipeFd { pipeFd[0] }; // make sure to close the pipe on throw

                int status = 0;
                if (qt_safe_waitpid(pid, &status, 0) < 0)
                    throw Exception(errno, "error waiting for helper process of bindMountFileSystem");

                if (!WIFEXITED(status)) {
                    throw Exception("helper process of bindMountFileSystem failed with signal %1")
                        .arg(WTERMSIG(status));
                }

                int exitStatus = WEXITSTATUS(status);
                if (exitStatus != 0) {
                    int e = 0;
                    if (qt_safe_read(readPipeFd.get(), &e, sizeof(e)) == sizeof(e)) {
                        switch (exitStatus) {
                        case 1:
                            throw Exception(e, "could not enter the target mount namespace");
                        case 2:
                            throw Exception(e, "could not move the detached mount point to %1").arg(to);
                        default:
                            break;
                        }
                    }
                    throw Exception("helper process of bindMountFileSystem failed with unknown error");
                }
            }
        } else { // no target namespace: mount in the helper's own mount namespace
            if (::syscall(SYS_move_mount, fromFd.get(), "", -EBADF, qPrintable(to), MOVE_MOUNT_F_EMPTY_PATH) < 0)
                throw Exception(errno, "could not move the detached mount point to %1").arg(to);
        }
    } catchExceptionAsDBusError()
}

void SudoServer::setExtendedAttribute(const QString &file, const QByteArray &attrName,
                                      const QByteArray &attrValue)
{
    try {
        if (::setxattr(qPrintable(file), attrName.constData(), attrValue.constData(), attrValue.size(), 0) != 0)
            throw Exception(errno, "could not set extended attribute '%1' on file '%2'").arg(attrName).arg(file);
    } catchExceptionAsDBusError()
}

#endif // QT_CONFIG(am_multi_process)

QT_END_NAMESPACE_AM
