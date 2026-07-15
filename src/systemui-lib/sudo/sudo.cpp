// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:privilege-management

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSet>
#include <QSocketNotifier>
#include <QStandardPaths>
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
#ifndef MOVE_MOUNT_T_EMPTY_PATH
#  define MOVE_MOUNT_T_EMPTY_PATH 0x00000040
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
static Unix::Fd s_sudoAddressPipe; // clazy:exclude=non-pod-global-static
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

            // We need to die, when the main process dies: pidfds are designed to become readable
            // when the process they refer to dies.
            if (mainPidFd) {
                auto *mainDead = new QSocketNotifier(mainPidFd.get(), QSocketNotifier::Read, &app);
                QObject::connect(mainDead, &QSocketNotifier::activated, &app, [] {
#  if defined(QT_AM_COVERAGE)
                    __gcov_dump();
#  endif
                    ::_exit(0);
                });
            }

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
    Verify \a fileOrDir resolves to a path at or below one of \a allowedRoots. The target's parent
    is canonicalized (it must exist; the target itself may be a dangling symlink we are about to
    delete), so symlinks anywhere above the leaf cannot smuggle the operation outside an allowed
    root. Until the allowed roots have been set, every removal is rejected (fail-closed).

    In QT_BUILD_INTERNAL builds a non-empty \a testPrefix implicitly allows anything beneath it: the
    test-cleanup callers operate inside their temp prefix and never set real roots. This branch is
    compiled out of production builds, where setTestRootPathPrefix() itself is rejected.
*/
static void checkRemoveRecursiveAllowed(const QString &fileOrDir,
                                        const std::optional<QStringList> &allowedRoots,
                                        const std::optional<QString> &testPrefix)
{
    // Canonicalize the parent (resolves symlinked ancestors) but keep the leaf literal: safeRemove
    // deletes the named entry, not its symlink target, and the leaf may be a dangling symlink that
    // canonicalFilePath() would resolve to "".
    const QFileInfo fi(fileOrDir);
    const QString canonicalParent = fi.dir().canonicalPath();
    if (canonicalParent.isEmpty())
        throw Exception("removeRecursive target has no resolvable parent: %1").arg(fileOrDir);
    const QString canonicalTarget = QDir::cleanPath(canonicalParent + u'/' + fi.fileName());

    const auto isUnder = [&canonicalTarget](const QString &root) {
        const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
        return !canonicalRoot.isEmpty()
               && ((canonicalTarget == canonicalRoot)
                   || canonicalTarget.startsWith(canonicalRoot + u'/'));
    };

#if defined(QT_BUILD_INTERNAL)
    if (testPrefix && !testPrefix->isEmpty() && isUnder(*testPrefix))
        return;
#else
    Q_UNUSED(testPrefix)
#endif

    if (!allowedRoots)
        throw Exception("removeRecursive called before setAllowedRemoveRecursiveRoots");

    for (const QString &root : *allowedRoots) {
        if (isUnder(root))
            return;
    }
    throw Exception("removeRecursive target (%1) escapes all allowed roots (%2)")
        .arg(fileOrDir).arg(*allowedRoots);
}

/*! \internal
    In fallback mode (no root helper), this runs in-process.
*/
void SudoClient::removeRecursive(const QString &fileOrDir)
{
    if (d->isFallback) {
        checkRemoveRecursiveAllowed(fileOrDir, d->allowedRemoveRoots, d->testPrefix);
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
    Set-once call restricting removeRecursive() to targets under one of \a roots.
*/
void SudoClient::setAllowedRemoveRecursiveRoots(const QStringList &roots)
{
    if (d->allowedRemoveRoots && (*d->allowedRemoveRoots != roots))
        throw Exception("setAllowedRemoveRecursiveRoots was already called with a different value");
    d->allowedRemoveRoots = roots;
#if QT_CONFIG(am_multi_process)
    if (!d->isFallback && d->iface)
        checkDBusReply<void>(d->iface->setAllowedRemoveRecursiveRoots(roots), __func__);
#endif
}

/*! \internal
    \a namespacePidFd is an already-open pidfd for the process whose mount namespace to enter, or -1
    to mount in the helper's own namespace. It is forwarded to the helper over D-Bus. In fallback mode
    this throws: it has no meaning without the privileged helper.
*/
void SudoClient::bindMountFileSystem(const QString &source, const QString &target, bool readOnly,
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

        return checkDBusReply<void>(d->iface->bindMountFileSystem(source, target, readOnly, useNamespacePidFd,
                                                                  QDBusUnixFileDescriptor(fd)),
                                                                  __func__);
    }
#else
    Q_UNUSED(source)
    Q_UNUSED(target)
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


///////////////////////////////////////////////////////////////////////
// Trusted files
///////////////////////////////////////////////////////////////////////

// Resolve relPath to an absolute path under <baseDir>[/<instanceId>]/, with lexical + symlink
// containment. The privileged helper (System) and the non-root in-process fallback (User) share
// everything except the base dir: System uses the FHS /var trees, User the per-user writable
// location.
enum class TrustedRoot { System, User };

static QString trustedPath(TrustedRoot root, QStandardPaths::StandardLocation location,
                           const QString &relPath, const std::optional<QString> &testPrefix,
                           const std::optional<QString> &instanceId)
{
    QString baseDir;
    switch (location) {
    case QStandardPaths::StateLocation:
        baseDir = (root == TrustedRoot::System) ? u"/var/lib/qtapplicationmanager/"_s
                                                : QStandardPaths::writableLocation(location);
        break;
    case QStandardPaths::CacheLocation:
        baseDir = (root == TrustedRoot::System) ? u"/var/cache/qtapplicationmanager/"_s
                                                : QStandardPaths::writableLocation(location);
        break;
    default:
        throw Exception("unsupported trusted-file location: %1").arg(int(location));
    }

    if (relPath.isEmpty())
        throw Exception("trusted-file relPath must not be empty");
    if (relPath.startsWith(u'/'))
        throw Exception("trusted-file relPath must be relative, not absolute: %1").arg(relPath);
#if !defined(QT_BUILD_INTERNAL)
    if (testPrefix)
        throw Exception("a test prefix is not supported in production builds");
#endif
    // When a testPrefix is set, baseDir nests under it. POSIX leading slashes get folded
    // implicitly by QDir::cleanPath, but on Windows "C:/..." in the middle of a concatenated
    // path stays as-is and breaks mkpath - strip the drive letter explicitly.
    QString nestedBaseDir = baseDir;
#if defined(Q_OS_WINDOWS)
    if (testPrefix && (nestedBaseDir.size() >= 3) && (nestedBaseDir[1] == u':')
        && ((nestedBaseDir[2] == u'/') || (nestedBaseDir[2] == u'\\'))) {
        nestedBaseDir.remove(0, 3);
    }
#endif
    const QString anchor = QDir::cleanPath(testPrefix.value_or(QString()) + nestedBaseDir
                                           + u'/' + instanceId.value_or(QString())) + u'/';
    const QString fullPath = QDir::cleanPath(anchor + relPath);
    if (!QString(fullPath + u'/').startsWith(anchor))
        throw Exception("trusted-file relPath escapes the anchor: %1 -> %2").arg(relPath, fullPath);

    // For the symlink-escape check, canonicalize both sides so any OS-level symlinks above the
    // anchor (e.g. /var -> /private/var on macOS test temp dirs) resolve consistently. If either
    // path doesn't exist yet, skip - first access creates it and the next call re-checks.
    const QString canonicalParent = QFileInfo(fullPath).dir().canonicalPath();
    const QString canonicalAnchor = QFileInfo(anchor).canonicalFilePath();
    if (!canonicalParent.isEmpty() && !canonicalAnchor.isEmpty()
        && !QString(canonicalParent + u'/').startsWith(canonicalAnchor + u'/')) {
        throw Exception("trusted-file relPath escapes the anchor via symlink: %1 -> %2")
            .arg(relPath, canonicalParent);
    }
    return fullPath;
}

/*! \internal
    Trusted (root-owned 0400) files are partitioned per instance under the id set here. Reads return a
    ready-to-read TrustedFile; writes return a TrustedSaveFile (write + commit()); removes are direct.
*/
void SudoClient::setInstanceId(const QString &instanceId)
{
    if (d->instanceId && (*d->instanceId != instanceId))
        throw Exception("setInstanceId was already called with a different value");
    d->instanceId = instanceId;
#if QT_CONFIG(am_multi_process)
    if (!d->isFallback && d->iface)
        checkDBusReply<void>(d->iface->setInstanceId(instanceId), __func__);
#endif
}

#if defined(QT_BUILD_INTERNAL)
void SudoClient::setTestRootPathPrefix(const QString &prefix)
{
    d->testPrefix = prefix;
#if QT_CONFIG(am_multi_process)
    if (!d->isFallback && d->iface)
        checkDBusReply<void>(d->iface->setTestRootPathPrefix(prefix), __func__);
#  endif
}

/*! \internal
    Test-only seam: commit an arbitrary fd to exercise the helper's staging-session validation
    (foreign fd / one-shot). Not compiled into production builds.
*/
void SudoClient::commitRawFdForTest(int fd)
{
    d->commitTrusted(fd);
}

/*! \internal
    Test-only: clear the previously-set instance-id so a single test process can exercise
    parseWithArguments() with multiple --instance-id values across test methods. The
    "throws on different value" check in setInstanceId() is correct for production (one
    Configuration per process); in tests we just need to roll back between scenarios.
    Fallback-mode only: there is no helper process whose state would also need to be cleared.
    Not compiled into production builds.
*/
void SudoClient::resetInstanceIdForTest()
{
    d->instanceId.reset();
}
#endif // QT_BUILD_INTERNAL


/*! \internal
    The only \a location values implemented are CacheLocation (/var/cache) and StateLocation (/var/lib).
*/
std::unique_ptr<TrustedFile> SudoClient::openTrustedFile(QStandardPaths::StandardLocation location, const QString &relPath)
{
    if (d->isFallback) {
        const QString fullPath = trustedPath(TrustedRoot::User, location, relPath,
                                             d->testPrefix, d->instanceId);
        auto tf = std::make_unique<TrustedFile>();
        tf->setFileName(fullPath);
        if (!tf->open(QIODevice::ReadOnly))
            throw Exception("could not open trusted file %1: %2").arg(fullPath, tf->errorString());
        return tf;
    }
#if QT_CONFIG(am_multi_process)
    if (d->iface) {
        auto fdw = checkDBusReply<QDBusUnixFileDescriptor>(d->iface->openTrustedFile(int(location),
                                                                                     relPath),
                                                           __func__);
        Unix::Fd fd = qt_safe_dup(fdw.fileDescriptor()); // own a copy past the reply's lifetime
        if (!fd)
            throw Exception(errno, "could not dup the trusted-file descriptor");
        auto tf = std::make_unique<TrustedFile>();
        if (!tf->open(fd.get(), QIODevice::ReadOnly, QFileDevice::AutoCloseHandle))
            throw Exception("could not open the trusted file: %1").arg(tf->errorString());
        (void) fd.release(); // NOLINT(bugprone-unused-return-value)
        return tf;
    }
#endif // QT_CONFIG(am_multi_process)
    throw Exception("The sudo-helper process is not available.");
}

std::unique_ptr<TrustedSaveFile> SudoClient::openTrustedSaveFile(QStandardPaths::StandardLocation location, const QString &relPath)
{
    if (d->isFallback) {
        const QString fullPath = trustedPath(TrustedRoot::User, location, relPath,
                                             d->testPrefix, d->instanceId);
        const QString parentDir = QFileInfo(fullPath).absolutePath();
        if (!QDir().mkpath(parentDir))
            throw Exception("could not create trusted directory %1").arg(parentDir);
        // Stage into a temp file in the same dir; commit() renames it atomically over the target.
        const QString tempPath = fullPath + u".commit-"_s + QUuid::createUuid().toString(QUuid::Id128);
        auto tf = std::make_unique<TrustedSaveFile>();
        tf->setFileName(tempPath);
        if (!tf->open(QIODevice::WriteOnly | QIODevice::Truncate))
            throw Exception("could not open trusted file %1 for writing: %2").arg(fullPath, tf->errorString());
        tf->d->client = this;
        tf->d->relPath = relPath;
        tf->d->isFallback = true;
        tf->d->fallbackFinalPath = fullPath;
        return tf;
    }
#if QT_CONFIG(am_multi_process)
    if (d->iface) {
        auto fdw = checkDBusReply<QDBusUnixFileDescriptor>(d->iface->openTrustedSaveFile(int(location),
                                                                                         relPath),
                                                           __func__);
        Unix::Fd fd = qt_safe_dup(fdw.fileDescriptor());  // own a copy past the reply's lifetime
        if (!fd)
            throw Exception(errno, "could not dup the trusted-file descriptor");
        auto tf = std::make_unique<TrustedSaveFile>();
        if (!tf->open(fd.get(), QIODevice::WriteOnly, QFileDevice::AutoCloseHandle))
            throw Exception("could not open the trusted file for writing: %1").arg(tf->errorString());
        (void) fd.release(); // NOLINT(bugprone-unused-return-value)

        tf->d->client = this;
        tf->d->relPath = relPath;
        return tf;
    }
#endif // QT_CONFIG(am_multi_process)
    throw Exception("The sudo-helper process is not available.");
}

void SudoClient::removeTrustedFile(QStandardPaths::StandardLocation location, const QString &relPath)
{
    if (d->isFallback) {
        const QString fullPath = trustedPath(TrustedRoot::User, location, relPath,
                                             d->testPrefix, d->instanceId);
        QFile f(fullPath);
        if (f.exists() && !f.remove())
            throw Exception(f, "could not remove trusted file");
        return;
    }
#if QT_CONFIG(am_multi_process)
    if (d->iface) {
        checkDBusReply<void>(d->iface->removeTrustedFile(int(location), relPath), __func__);
        return;
    }
#endif
    throw Exception("The sudo-helper process is not available.");
}

void SudoClientPrivate::commitTrusted(int writtenFd)
{
    // Helper case only (fallback commits locally in TrustedSaveFile::commit()). The helper recovers
    // the target path from the staging session, so the fd is all we pass.
#if QT_CONFIG(am_multi_process)
    if (iface) {
        checkDBusReply<void>(iface->commitTrustedSaveFile(QDBusUnixFileDescriptor(writtenFd)),
                             __func__);
        return;
    }
#else
    Q_UNUSED(writtenFd)
#endif
    throw Exception("The sudo-helper process is not available.");
}

void SudoClientPrivate::cancelTrusted(int stagingFd) noexcept
{
    // Fire-and-forget (called from ~TrustedSaveFile): never throw, never wait on a reply. A lost
    // cancel is harmless. If the helper is already gone there is no session left to drop.
#if QT_CONFIG(am_multi_process)
    if (iface)
        iface->cancelTrustedSaveFile(QDBusUnixFileDescriptor(stagingFd));
#else
    Q_UNUSED(stagingFd)
#endif
}


/*!
    \class TrustedFile
    \internal
    A QFile opened (for reading) on a descriptor the sudo-helper handed us for a root-owned 0400
    trusted file. Plain read; closing it closes the descriptor.
*/
TrustedFile::TrustedFile(QObject *parent)
    : QFile(parent)
{ }


/*!
    \class TrustedSaveFile
    \internal
    A QFile opened (for writing) on the helper's anonymous staging descriptor. Mirrors QSaveFile
    semantics: write, then commit() to atomically materialize the file as root-owned 0400. Without an
    explicit commit() the content is discarded on destruction.
*/
TrustedSaveFile::TrustedSaveFile(QObject *parent)
    : QFile(parent)
    , d(new TrustedSaveFilePrivate)
{ }

TrustedSaveFile::~TrustedSaveFile()
{
    if (!d->committed && !d->cancelled)
        cancel();
}

void TrustedSaveFile::commit()
{
    if (d->committed)
        return;
    if (d->cancelled)
        throw Exception("cannot commit a trusted file that was already cancelled");

    if (d->isFallback) {
        // No helper: replace the target ourselves. QFile::rename() won't overwrite, so drop any
        // existing target first. Not atomic - but the fallback has no privilege separation anyway;
        // the privileged helper path is the atomic one.
        flush();
        const QString tempPath = fileName();
        close();
        QFile::remove(d->fallbackFinalPath);
        if (!QFile::rename(tempPath, d->fallbackFinalPath)) {
            QFile::remove(tempPath);
            throw Exception("could not commit trusted file %1").arg(d->fallbackFinalPath);
        }
        d->committed = true;
        return;
    }

    if (!d->client)
        throw Exception("the sudo-helper connection for this trusted file is no longer available");
    flush();
    d->client->d->commitTrusted(handle());
    d->committed = true;
}

/*! \internal
    Discard the staging file (mirrors QSaveFile::cancelWriting()).
*/
void TrustedSaveFile::cancel()
{
    if (d->committed || d->cancelled)
        return;
    d->cancelled = true;

    if (d->isFallback) {
        // Drop the local staging temp; closing happens here so QFile::remove() can unlink it.
        const QString tempPath = fileName();
        if (isOpen())
            close();
        if (!tempPath.isEmpty())
            QFile::remove(tempPath);
        return;
    }

    if (d->client && isOpen()) // important: book keeping for the sudo-helper side
        d->client->d->cancelTrusted(handle());
    close();
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
    auto *sessionExpire = new QTimer(this);
    sessionExpire->setInterval(MaxSaveSessionDuration * 2);
    connect(sessionExpire, &QTimer::timeout, this, [this]() {
        const auto now = std::chrono::steady_clock::now();
        for (auto it = m_saveSessions.begin(); it != m_saveSessions.end(); ) {
            if ((now - it->second.issuedAt) > MaxSaveSessionDuration)
                it = m_saveSessions.erase(it);
            else
                ++it;
        }
    });
    sessionExpire->start();
}

std::pair<quint64, quint64> SudoServer::saveSessionKey(int fd)
{
    struct ::stat st
    {};
    if (::fstat(fd, &st) != 0)
        throw Exception(errno, "could not fstat the trusted staging descriptor");
    return {quint64(st.st_dev), quint64(st.st_ino)};
}

void SudoServer::removeRecursive(const QString &fileOrDir)
{
    try {
        checkRemoveRecursiveAllowed(fileOrDir, m_allowedRemoveRoots, m_testPrefix);
        if (!recursiveOperation(fileOrDir, safeRemove))
            throw Exception(errno, "could not recursively remove %1").arg(fileOrDir);
    } catchExceptionAsDBusError()
}

void SudoServer::setAllowedRemoveRecursiveRoots(const QStringList &roots)
{
    try {
        if (m_allowedRemoveRoots && (*m_allowedRemoveRoots != roots))
            throw Exception("setAllowedRemoveRecursiveRoots was already called with a different value");
        m_allowedRemoveRoots = roots;
    } catchExceptionAsDBusError()
}

void SudoServer::bindMountFileSystem(const QString &source, const QString &target, bool readOnly,
                                     bool useNamespacePidFd,
                                     const QDBusUnixFileDescriptor &namespacePidFd)
{
    try {
        const QByteArray sourceUtf8 = QDir::cleanPath(source).toLocal8Bit();
        const QByteArray targetUtf8 = QDir::cleanPath(target).toLocal8Bit();

        if (!sourceUtf8.startsWith('/'))
            throw Exception("source path for bindMountFileSystem must be absolute: %1").arg(source);
        if (!targetUtf8.startsWith('/'))
            throw Exception("target path for bindMountFileSystem must be absolute: %1").arg(target);

        // Defeat parent-symlink planting and detect non-canonical paths on both endpoints:
        //  - "source" lives on the host filesystem, where third-party writers may exist at any
        //    parent dir
        //  - "target" lives either on the host (no-namespace case) or inside the given mount
        //    namespace, which may contain writable areas owned by an app.
        // Strategy: openPath() opens the cleaned path race-free (O_NOFOLLOW), validatePath()
        // then asks the kernel which path that fd actually resolves to (readlink on
        // /proc/self/fd/N - a kernel-side query of the held fd, not a fresh path-walk) and
        // requires it to equal the cleaned input. move_mount then operates on the validated fd
        // (MOVE_MOUNT_T_EMPTY_PATH), so the kernel never re-resolves any path string.

        auto openPath = [](const QByteArray &path) -> Unix::Fd {
            // async-signal-safe
            return Unix::Fd { qt_safe_open(path.constData(), O_PATH | O_NOFOLLOW | O_CLOEXEC) };
        };

        auto validatePath = [](const QByteArray &path, Unix::Fd &fd) {
            // async-signal-safe
            if (fd) {
                // This is a TOCTOU race-free and async-signal-safe version of comparing path with
                // QDir::canonicalPath(path) to detect symlink planting.
                // Note: there is no POSIX way to achieve this, but using /proc/self/fd is the
                //       standard approach on Linux to this problem.

                std::array<char, 64> procPath;
                ::snprintf(procPath.data(), procPath.size(), "/proc/self/fd/%d", fd.get());
                std::array<char, 8 * PATH_MAX> canonical;
                ssize_t n = ::readlink(procPath.data(), canonical.data(), canonical.size());
                if ((qsizetype(n) == path.size())
                        && !::memcmp(canonical.data(), path.constData(), n)) {
                    return;
                } else {
                    errno = EINVAL; // path is not canonical, or changed under us
                }
            }
            fd.reset();
        };

        Unix::Fd sourcePathFd = openPath(sourceUtf8);
        if (!sourcePathFd)
            throw Exception(errno, "could not open the source path for bindMountFileSystem: %1").arg(source);
        validatePath(sourceUtf8, sourcePathFd);
        if (!sourcePathFd)
            throw Exception(errno, "the source path for bindMountFileSystem is not canonical or tampered with: %1").arg(source);

        // Create the detached mount from the validated fd, not from the path string. The mount
        // object is anchored on the inode we just verified, so no further string-resolution race.
        Unix::Fd sourceFd { int(::syscall(SYS_open_tree, sourcePathFd.get(), "",
                                          OPEN_TREE_CLOEXEC | OPEN_TREE_CLONE | AT_EMPTY_PATH | AT_RECURSIVE)) };
        if (!sourceFd)
            throw Exception(errno, "could not create a detached mount point for %1").arg(source);

        if (readOnly) {
            ::mount_attr mountAttr { MOUNT_ATTR_RDONLY, 0, 0, 0 };
            if (::syscall(SYS_mount_setattr, sourceFd.get(), "", AT_EMPTY_PATH | AT_RECURSIVE,
                          &mountAttr, sizeof(mountAttr)) < 0) {
                throw Exception(errno, "could not set the detached mount point for %1 to read-only").arg(source);
            }
        }

        if (!useNamespacePidFd) {
            // no target namespace: validate and mount in the helper's own mount namespace
            Unix::Fd targetFd = openPath(targetUtf8);
            if (!targetFd)
                throw Exception(errno, "could not open the target path for bindMountFileSystem: %1").arg(target);
            validatePath(targetUtf8, targetFd);
            if (!targetFd)
                throw Exception(errno, "the target path for bindMountFileSystem is not canonical or tampered with: %1").arg(target);

            if (::syscall(SYS_move_mount, sourceFd.get(), "", targetFd.get(), "",
                          MOVE_MOUNT_F_EMPTY_PATH | MOVE_MOUNT_T_EMPTY_PATH) < 0) {
                throw Exception(errno, "could not move the detached mount point to %1").arg(target);
            }
        } else {
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

            int pipeFd[2] = { -1, -1 };
            if (qt_safe_pipe(pipeFd) < 0)
                throw Exception(errno, "could not create pipe for helper process of bindMountFileSystem");

#  if defined(QT_AM_COVERAGE)
            // We need to make the gcda files generated by the root process writable by the normal user.
            // GCov will open all gcda files at fork() time, so we can get away with switching umasks
            // around the fork() call.
            mode_t realUmask = ::umask(0);
#  endif

            pid_t pid = ::fork();
            if (pid < 0) {
                qt_safe_close(pipeFd[0]);
                qt_safe_close(pipeFd[1]);
                throw Exception(errno, "could not fork helper process for bindMountFileSystem");
            } else if (pid == 0) {
                // child process, throw-away setns, validate and mount

                qt_safe_close(pipeFd[0]);

#if defined(QT_AM_COVERAGE)
                ::umask(realUmask); // reset umask, see above for explanation
#  endif
                auto exitWithErrno = [&](int exitCode) {
                    if (exitCode) {
                        int e = errno;
                        qt_safe_write(pipeFd[1], &e, sizeof(e));
                    }
#if defined(QT_AM_COVERAGE)
                    __gcov_dump();
#  endif
                    ::_exit(exitCode);
                };

                //NB: we just forked from a multi-threaded process, which means we need to be extra
                //    careful to not do anything that could cause deadlocks due to locks held by
                //    other threads at fork() time: throwing exceptions and QString operations are
                //    not possible. For the most part, we need to act like a signal handler here.

                // Enter the mount namespace of the target process
                if (::setns(pidFd, CLONE_NEWNS) < 0)
                    exitWithErrno(1);

                // Run the same 'open and validate' the no-namespace branch uses, but now resolving
                // inside the target namespace (which the parent could not have looked into).
                Unix::Fd targetFd = openPath(targetUtf8);
                if (!targetFd)
                    exitWithErrno(2);
                validatePath(targetUtf8, targetFd);
                if (!targetFd)
                    exitWithErrno(3);

                if (::syscall(SYS_move_mount, sourceFd.get(), "", targetFd.get(), "",
                              MOVE_MOUNT_F_EMPTY_PATH | MOVE_MOUNT_T_EMPTY_PATH) < 0) {
                    exitWithErrno(4);
                }
                exitWithErrno(0);
            } else {
                // parent process, sudo-helper

#  if defined(QT_AM_COVERAGE)
                ::umask(realUmask); // reset umask, see above for explanation
#  endif

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
                            throw Exception(e, "could not open the target path for bindMountFileSystem in the target namespace: %1").arg(target);
                        case 3:
                            throw Exception(e, "the target path for bindMountFileSystem is not canonical or tampered with: %1").arg(target);
                        case 4:
                            throw Exception(e, "could not move the detached mount point to %1").arg(target);
                        default:
                            break;
                        }
                    }
                    throw Exception("helper process of bindMountFileSystem failed with unknown error");
                }
            }
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

void SudoServer::setInstanceId(const QString &instanceId)
{
    try {
        if (m_instanceId && (*m_instanceId != instanceId))
            throw Exception("setInstanceId was already called with a different value");
        m_instanceId = instanceId;
    } catchExceptionAsDBusError()
}

void SudoServer::setTestRootPathPrefix(const QString &prefix)
{
#if defined(QT_BUILD_INTERNAL)
    m_testPrefix = prefix;
#else
    Q_UNUSED(prefix)
    sendErrorReply(QDBusError::Failed, u"setting a test prefix is not supported in production builds"_s);
#endif
}

QDBusUnixFileDescriptor SudoServer::openTrustedFile(int location, const QString &relPath)
{
    try {
        QString absPath = trustedPath(TrustedRoot::System, QStandardPaths::StandardLocation(location),
                                      relPath, m_testPrefix, m_instanceId);
        Unix::Fd fd { qt_safe_open(absPath.toLocal8Bit().constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC) };
        if (!fd)
            throw Exception(errno, "could not open trusted file %1 for reading").arg(absPath);
        struct ::stat st { };
        if ((::fstat(fd.get(), &st) != 0) || !S_ISREG(st.st_mode))
            throw Exception("trusted file %1 is not a regular file").arg(absPath);

        return QDBusUnixFileDescriptor(fd.get()); // dups; Unix::Fd closes the original
    } catchExceptionAsDBusError({})
}

QDBusUnixFileDescriptor SudoServer::openTrustedSaveFile(int location, const QString &relPath)
{
    try {
        QString absPath = trustedPath(TrustedRoot::System, QStandardPaths::StandardLocation(location),
                                      relPath, m_testPrefix, m_instanceId);
        const QByteArray parentDir = QFileInfo(absPath).absolutePath().toLocal8Bit();
        if (!QDir().mkpath(QString::fromLocal8Bit(parentDir)))
            throw Exception("could not create trusted directory %1").arg(parentDir);

        if ((::chown(parentDir.constData(), 0, 0) != 0)
            || (::chmod(parentDir.constData(), 0700) != 0)) {
            throw Exception(errno, "could not lock down trusted directory %1").arg(parentDir);
        }

        // Anonymous, root-owned 0400 staging inode on the same filesystem as the final path.
        Unix::Fd fd { qt_safe_open(parentDir.constData(), O_TMPFILE | O_WRONLY | O_CLOEXEC, 0400) };
        if (!fd)
            throw Exception(errno, "could not create a trusted staging file in %1").arg(parentDir);

        if (::fchown(fd.get(), 0, 0) != 0)
            throw Exception(errno, "could not chown the trusted staging file to root:root");

        if (m_saveSessions.size() >= MaxSaveSessions)
            throw Exception("too many outstanding trusted save sessions");

        // Keep our fd open so the inode (the lookup key) stays valid until commit/cancel.
        const auto key = saveSessionKey(fd.get());
        QDBusUnixFileDescriptor wire(fd.get()); // dups onto the wire; our Unix::Fd stays in the map
        m_saveSessions.insert_or_assign(key, SaveSession {
            std::move(fd), absPath, std::chrono::steady_clock::now() });
        return wire;

    } catchExceptionAsDBusError({})
}

void SudoServer::commitTrustedSaveFile(const QDBusUnixFileDescriptor &saveFd)
{
    try {
        // Look up by the fd's inode: a fd we never issued isn't here, and the target path was fixed
        // at open time, so the client can't redirect the commit.
        const auto it = m_saveSessions.find(saveSessionKey(saveFd.fileDescriptor()));
        if (it == m_saveSessions.end())
            throw Exception("unknown or expired trusted save session");

        const QString absPath = it->second.absPath;
        const QString parentDir = QFileInfo(absPath).absolutePath();
        if (!QDir().mkpath(parentDir))
            throw Exception("could not create trusted directory %1").arg(parentDir);

        // Materialize *our* retained inode under a random name, then atomically rename over the target.
        const QByteArray tempPath = (parentDir.toLocal8Bit() + "/.commit-"
                                     + QUuid::createUuid().toByteArray(QUuid::Id128));
        const QByteArray procPath = "/proc/self/fd/" + QByteArray::number(it->second.fd.get());
        if (::linkat(AT_FDCWD, procPath.constData(),
                     AT_FDCWD, tempPath.constData(), AT_SYMLINK_FOLLOW) != 0) {
            throw Exception(errno, "could not link the trusted staging file into place");
        }
        if (::rename(tempPath.constData(), absPath.toLocal8Bit().constData()) != 0) {
            ::unlink(tempPath.constData());
            throw Exception(errno, "could not rename the trusted staging file to %1").arg(absPath);
        }
        m_saveSessions.erase(it); // one-shot; closes our retained fd
    } catchExceptionAsDBusError()
}

void SudoServer::cancelTrustedSaveFile(const QDBusUnixFileDescriptor &saveFd)
{
    try {
        m_saveSessions.erase(saveSessionKey(saveFd.fileDescriptor()));
    } catchExceptionAsDBusError()
}

void SudoServer::removeTrustedFile(int location, const QString &relPath)
{
    try {
        QString absPath = trustedPath(TrustedRoot::System, QStandardPaths::StandardLocation(location),
                                      relPath, m_testPrefix, m_instanceId);
        if ((::unlink(absPath.toLocal8Bit().constData()) != 0) && (errno != ENOENT))
            throw Exception(errno, "could not remove trusted file %1").arg(absPath);
    } catchExceptionAsDBusError()
}

#endif // QT_CONFIG(am_multi_process)

QT_END_NAMESPACE_AM
