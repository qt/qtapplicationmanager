// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:privilege-management

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QtEndian>
#include <QDataStream>
#include <qplatformdefs.h>
#include <QDataStream>

#include "logging.h"
#include "sudo.h"
#include "utilities.h"
#include "exception.h"

#include <cerrno>

using namespace Qt::StringLiterals;

#if defined(Q_OS_LINUX)
# include "processtitle.h"

#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/errno.h>
#  include <sys/ioctl.h>
#  include <sys/stat.h>
#  include <sys/prctl.h>
#  include <sys/mount.h>
#  include <sys/syscall.h>
#  include <sys/xattr.h>
#  include <sched.h>
#  include <linux/capability.h>

// These two functions are implemented in glibc, but the header file is
// in the separate libcap-dev package. Since we want to avoid unnecessary
// dependencies, we just declare them here
extern "C" int capset(cap_user_header_t header, cap_user_data_t data);
extern "C" int capget(cap_user_header_t header, const cap_user_data_t data);

// Support for old/broken C libraries
#  if defined(_LINUX_CAPABILITY_VERSION) && !defined(_LINUX_CAPABILITY_VERSION_1)
#    define _LINUX_CAPABILITY_VERSION_1 _LINUX_CAPABILITY_VERSION
#    define _LINUX_CAPABILITY_U32S_1    1
#    if !defined(CAP_TO_INDEX)
#      define CAP_TO_INDEX(x) ((x) >> 5)
#    endif
#    if !defined(CAP_TO_MASK)
#      define CAP_TO_MASK(x)  (1 << ((x) & 31))
#    endif
#  endif
#  if defined(_LINUX_CAPABILITY_VERSION_3) // use 64-bit support, if available
#    define AM_CAP_VERSION _LINUX_CAPABILITY_VERSION_3
#    define AM_CAP_SIZE    _LINUX_CAPABILITY_U32S_3
#  else // fallback to 32-bit support
#    define AM_CAP_VERSION _LINUX_CAPABILITY_VERSION_1
#    define AM_CAP_SIZE    _LINUX_CAPABILITY_U32S_1
#  endif

// Convenient way to ignore EINTR on any system call
#  define EINTR_LOOP(cmd) __extension__ ({__typeof__(cmd) res = 0; do { res = cmd; } while (res == -1 && errno == EINTR); res; })


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

#ifndef MOUNT_ATTR_SIZE_VER0
#  define MOUNT_ATTR_SIZE_VER0 32

struct mount_attr {
    __u64 attr_set;
    __u64 attr_clr;
    __u64 propagation;
    __u64 userns_fd;
};
#endif

#ifndef AT_RECURSIVE
#  define AT_RECURSIVE 0x8000
#endif

#ifndef AT_EMPTY_PATH
#  define AT_EMPTY_PATH 0x1000
#endif

#ifndef SYS_mount_setattr
#  define SYS_mount_setattr 442
#endif

#ifndef SYS_pidfd_open
#  define SYS_pidfd_open 434
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

class Group;

class User
{
public:
    User() : User(nullptr) { }
    User(const User &other)
        : m_valid(other.m_valid)
        , m_name(other.m_name)
        , m_uid(other.m_uid)
        , m_gid(other.m_gid)
        , m_dir(other.m_dir)
        , m_shell(other.m_shell)
    {
        ++s_count;
    }

    ~User()
    {
        if (--s_count == 0)
            ::endpwent();
    }

    User &operator=(const User &) = default;

    bool isValid() const { return m_valid; }
    uid_t uid() const { return m_uid; }
    gid_t gid() const { return m_gid; }
    const char *name() const { return m_name.constData(); }
    const char *dir() const { return m_dir.constData(); }
    const char *shell() const { return m_shell.constData(); }

    QSet<gid_t> supplementaryGroupIds(const Group &group);

    void setCurrent(bool permanently = true)
    {
        if (::setresuid(uid(), uid(), permanently ? uid() : 0) < 0) {
            throw Exception(errno, "Could not %1set the user to %2")
                .arg(permanently ? "permanently " : "").arg(name());
        }
    }

    static void setCurrentSupplementaryGroupIds(const QSet<gid_t> setSupGids)
    {
        if (::setgroups(setSupGids.size(), QVector<gid_t>(setSupGids.cbegin(), setSupGids.cend()).constData()) < 0)
            throw Exception(errno, "Could not set supplementary groups (%2)").arg(setSupGids);
    }

    // The result is always valid
    static User parse(const QByteArray &user)
    {
        bool ok;
        if (uid_t uid = user.toUInt(&ok); ok) {
            if (struct ::passwd *pw = ::getpwuid(uid))
                return User(pw);
        }
        if (struct ::passwd *pw = ::getpwnam(user.constData()))
            return User(pw);

        throw Exception("unknown user '%1'").arg(user);
    }

private:
    explicit User(const struct ::passwd *pw)
        : m_valid(pw)
        , m_name(pw ? pw->pw_name : "")
        , m_uid(pw ? pw->pw_uid : static_cast<uid_t>(-1))
        , m_gid(pw ? pw->pw_gid : static_cast<gid_t>(-1))
        , m_dir(pw ? pw->pw_dir : "")
        , m_shell(pw ? pw->pw_shell : "")
    {
        ++s_count;
    }

    bool m_valid;
    QByteArray m_name;
    uid_t m_uid = static_cast<uid_t>(-1);
    gid_t m_gid = static_cast<gid_t>(-1);
    QByteArray m_dir;
    QByteArray m_shell;
    static quint64 s_count;
};

quint64 User::s_count = 0;

class Group
{
public:
    Group() : Group(nullptr) { }
    Group(const Group &other)
        : m_valid(other.m_valid)
        , m_name(other.m_name)
        , m_gid(other.m_gid)
    {
        ++s_count;
    }

    ~Group()
    {
        if (--s_count == 0)
            ::endgrent();
    }

    Group &operator=(const Group &) = default;

    bool isValid() const { return m_valid; }
    gid_t gid() const { return m_gid; }
    const char *name() const { return m_name.constData(); }

    void setCurrent(bool permanently = true)
    {
        if (::setresgid(gid(), gid(), permanently ? gid() : 0) < 0) {
            throw Exception(errno, "Could not %1set the group to %2")
                .arg(permanently ? "permanently ": "").arg(name());
        }
    }

    // The result is always valid
    static Group parse(const QByteArray &group)
    {
        bool ok;
        if (gid_t gid = group.toUInt(&ok); ok) {
            if (struct ::group *gr = ::getgrgid(gid))
                return Group(gr);
        }
        if (struct ::group *gr = ::getgrnam(group.constData()))
            return Group(gr);
        throw Exception("unknown user '%1'").arg(group);
    }

    // The result is always valid
    static Group fromUser(const User &user)
    {
        if (user.isValid()) {
            if (struct ::group *gr = ::getgrgid(user.gid()))
                return Group(gr);
        }
        throw Exception("cannot determine group of user '%1'").arg(user.isValid() ? user.name() : "<unknown>");
    }

    static constexpr int MaxSupplementary = NGROUPS_MAX;

private:
    explicit Group(const struct ::group *gr)
        : m_valid(gr)
        , m_name(gr ? gr->gr_name : "")
        , m_gid(gr ? gr->gr_gid : static_cast<gid_t>(-1))
    {
        ++s_count;
    }

    bool m_valid;
    QByteArray m_name;
    gid_t m_gid;
    static quint64 s_count;
};

quint64 Group::s_count = 0;

QSet<gid_t> User::supplementaryGroupIds(const Group &mainGroup)
{
    gid_t supGids[NGROUPS_MAX + 1];
    int supGidsLen = NGROUPS_MAX + 1;
    gid_t mainGid = mainGroup.isValid() ? mainGroup.gid() : gid();
    if (::getgrouplist(name(), mainGid, supGids, &supGidsLen) < 0)
        throw Exception("Could not get supplementary groups for user %1").arg(name());
    QSet<gid_t> result { supGids, supGids + supGidsLen };
    result.remove(mainGid);
    return result;
}

QT_END_NAMESPACE_AM

#endif // Q_OS_LINUX


QT_BEGIN_NAMESPACE_AM

void Sudo::fallbackServer()
{
    if (SudoServer::instance() && SudoClient::instance()) {
        if (!SudoClient::instance()->isFallbackImplementation())
            throw Exception("Sudo::fallbackServer was called after Sudo::forkServer");
        return;
    }

    SudoServer::createInstance(-1);
    SudoClient::createInstance(-1, SudoServer::instance());
}

void Sudo::forkServer(DropPrivileges dropPrivileges)
{
    if (SudoServer::instance() && SudoClient::instance()) {
        if (SudoClient::instance()->isFallbackImplementation())
            throw Exception("Sudo::forkServer was called after Sudo::fallbackServer");
        return;
    }

#if !defined(Q_OS_LINUX)
    Q_UNUSED(dropPrivileges)
    return fallbackServer();
#else
    uid_t realUid = ::getuid();
    uid_t effectiveUid = ::geteuid();

    if (realUid != effectiveUid)
        throw Exception("Running as suid executable is not supported anymore");

    if (realUid != 0)
        return fallbackServer();

    if (!setuidArg)
        setuidArg = ::getenv("AM_SETUID");

    if ((realUid != 0) && setuidArg)
        throw Exception("Cannot use the --setuid argument or $AM_SETUID when not running as root");

    User setUser;
    Group setGroup;
    QSet<gid_t> setSupGids;

    // setuidArg is initialized in checkSetuidArg, before main()
    if (!setuidArg) {
        // If we are running under sudo, we can also use SUDO_UID and SUDO_GID. This is especially
        // important for auto-tests, as the testrunner does not like extra command line arguments.
        const QByteArray sudoUid = ::getenv("SUDO_UID");
        const QByteArray sudoGid = ::getenv("SUDO_GID");
        if (!sudoUid.isEmpty() && !sudoGid.isEmpty()) {
            try {
                setUser = User::parse(sudoUid);
                setGroup = Group::parse(sudoGid);

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

            setUser = User::parse(list.at(0));
            setGroup = (list.size() >= 2) ? Group::parse(list.at(1))
                                          : Group::fromUser(setUser);
            const auto supGroups = list.mid(2);
            for (const auto &supGroup : supGroups)
                setSupGids << Group::parse(supGroup).gid();

            if (setSupGids.size() > Group::MaxSupplementary)
                throw Exception("too many supplementary groups, the maximum is %1").arg(Group::MaxSupplementary);

            if ((setUser.uid() == 0) || (setGroup.gid() == 0) || setSupGids.contains(0)) {
                throw Exception("user and group(s) need to be unprivileged (got: %1:%2, supplementary: %3)")
                    .arg(setUser.name()).arg(setGroup.name()).arg(setSupGids);
            }
        } catch (const Exception &e) {
            throw Exception("Error parsing --setuid / $AM_SETUID: %1").arg(e.errorString());
        }
    }

    int socketFds[2];
    if (::socketpair(AF_UNIX, SOCK_DGRAM, 0, socketFds) != 0)
        throw Exception(errno, "Could not create a pair of sockets");

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
        ::close(0);
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

        // Drop as many capabilities as possible, just to be on the safe side
        static const quint32 neededCapabilities[] = {
            CAP_SYS_ADMIN,
            CAP_SYS_CHROOT,
            CAP_SYS_PTRACE,
            CAP_CHOWN,
            CAP_FOWNER,
            CAP_DAC_OVERRIDE
        };

        bool capSetOk = false;
        __user_cap_header_struct capHeader { AM_CAP_VERSION, ::getpid() };
        __user_cap_data_struct capData[AM_CAP_SIZE];
        if (::capget(&capHeader, capData) == 0) {
            quint32 capNeeded[AM_CAP_SIZE];
            ::memset(&capNeeded, 0, sizeof(capNeeded));
            for (quint32 cap : neededCapabilities) {
                int idx = CAP_TO_INDEX(cap);
                Q_ASSERT(idx < AM_CAP_SIZE);
                capNeeded[idx] |= CAP_TO_MASK(cap);
            }
            for (int i = 0; i < AM_CAP_SIZE; ++i)
                capData[i].effective = capData[i].permitted = capData[i].inheritable = capNeeded[i];
            if (::capset(&capHeader, capData) == 0)
                capSetOk = true;
        }
        if (!capSetOk)
            qCCritical(LogSystem) << "could not drop privileges in the SudoServer process -- continuing with full root privileges";

        SudoServer::createInstance(socketFds[0]);
        ProcessTitle::setTitle("sudo helper");
        SudoServer::instance()->run();
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
            if (setSupGids.size() > Group::MaxSupplementary) {
                throw Exception("Too many supplementary groups when combining the groups of user "
                                "%1 with the ones specified for --setuid / $AM_SETUID")
                    .arg(setUser.name());
            }
            User::setCurrentSupplementaryGroupIds(setSupGids);

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

        SudoClient::createInstance(socketFds[1]);
        ::atexit([]() { SudoClient::instance()->stopServer(); });

    } catch (const Exception &e) {
        ::kill(pid, SIGKILL);
        throw;
    }
#endif
}

SudoInterface::SudoInterface()
{ }

#ifdef Q_OS_LINUX
bool SudoInterface::sendMessage(int socket, const QByteArray &msg, MessageType type, const QString &errorString)
{
    QByteArray packet;
    QDataStream ds(&packet, QDataStream::WriteOnly);
    ds << errorString << msg;
    packet.prepend((type == Request) ? "RQST" : "RPLY");

    auto bytesWritten = EINTR_LOOP(write(socket, packet.constData(), static_cast<size_t>(packet.size())));
    return bytesWritten == packet.size();
}


QByteArray SudoInterface::receiveMessage(int socket, MessageType type, QString *errorString)
{
    const int headerSize = 4;
    std::array<char, 8*1024> recvBuffer;
    auto bytesReceived = EINTR_LOOP(::recv(socket, recvBuffer.data(), recvBuffer.size(), 0));

    if ((bytesReceived < headerSize) || qstrncmp(recvBuffer.data(), (type == Request ? "RQST" : "RPLY"), 4)) {
        *errorString = u"failed to receive command from the SudoClient process"_s;
        //qCCritical(LogSystem) << *errorString;
        return { };
    }

    auto packet = QByteArray::fromRawData(recvBuffer.data() + headerSize, bytesReceived - headerSize);
    QDataStream ds(&packet, QDataStream::ReadOnly);
    QByteArray msg;
    ds >> *errorString >> msg;
    return msg;
}
#endif // Q_OS_LINUX


SudoClient *SudoClient::s_instance = nullptr;

SudoClient *SudoClient::instance()
{
    return s_instance;
}

bool SudoClient::isFallbackImplementation() const
{
    return m_socket < 0;
}

SudoClient::SudoClient(int socketFd)
    : m_socket(socketFd)
{ }

SudoClient *SudoClient::createInstance(int socketFd, SudoServer *shortCircuit)
{
    if (!s_instance) {
        s_instance = new SudoClient(socketFd);
        s_instance->m_shortCircuit = shortCircuit;
    }
    return s_instance;
}


// this is not nice, but it prevents a lot of copy/paste errors. (the C++ variadic template version
// would be equally ugly, since it needs a friend declaration in the public header)
template <typename R, typename C, typename ...Ps> R returnType(R (C::*)(Ps...));

#define CALL(FUNC_NAME, PARAM) \
    QByteArray msg; \
    QDataStream(&msg, QDataStream::WriteOnly) << #FUNC_NAME << PARAM; \
    QByteArray reply = call(msg); \
    QDataStream result(&reply, QDataStream::ReadOnly); \
    decltype(returnType(&SudoClient::FUNC_NAME)) r; \
    result >> r; \
    return r

bool SudoClient::removeRecursive(const QString &fileOrDir)
{
    CALL(removeRecursive, fileOrDir);
}

bool SudoClient::bindMountFileSystem(const QString &from, const QString &to, bool readOnly,
                                     quint64 namespacePid)
{
    CALL(bindMountFileSystem, from << to << readOnly << namespacePid);
}

bool SudoClient::setExtendedAttribute(const QString &file, const QByteArray &attrName, const QByteArray &attrValue)
{
    CALL(setExtendedAttribute, file << attrName << attrValue);
}

void SudoClient::stopServer()
{
#ifdef Q_OS_LINUX
    if (!m_shortCircuit && m_socket >= 0) {
        QByteArray msg;
        QDataStream(&msg, QDataStream::WriteOnly) << "stopServer";
        sendMessage(m_socket, msg, Request);
    }
#endif
}

QByteArray SudoClient::call(const QByteArray &msg)
{
    QMutexLocker locker(&m_mutex);

    if (m_shortCircuit) {
        const QByteArray res = m_shortCircuit->receive(msg);
        m_errorString = m_shortCircuit->lastError();
        return res;
    }

#ifdef Q_OS_LINUX
    if (m_socket >= 0) {
        if (sendMessage(m_socket, msg, Request))
            return receiveMessage(m_socket, Reply, &m_errorString);
    }
#else
    Q_UNUSED(m_socket)
#endif

    //qCCritical(LogSystem) << "failed to send command to the SudoServer process";
    m_errorString = u"failed to send command to the SudoServer process"_s;
    return { };
}



SudoServer *SudoServer::s_instance = nullptr;

SudoServer *SudoServer::instance()
{
    return s_instance;
}

SudoServer::SudoServer(int socketFd)
    : m_socket(socketFd)
{ }

SudoServer *SudoServer::createInstance(int socketFd)
{
    if (!s_instance)
        s_instance = new SudoServer(socketFd);
    return s_instance;
}

void SudoServer::run()
{
#ifdef Q_OS_LINUX
    QString dummy;

    forever {
        QByteArray msg = receiveMessage(m_socket, Request, &dummy);
        QByteArray reply = receive(msg);

        if (m_stop)
            exit(0);

        sendMessage(m_socket, reply, Reply, m_errorString);
    }
#else
    Q_UNUSED(m_socket)
    Q_ASSERT(false);
    exit(0);
#endif
}

QByteArray SudoServer::receive(const QByteArray &msg)
{
    QDataStream params(msg);
    char *functionArray;
    params >> functionArray;
    QByteArray function(functionArray);
    delete [] functionArray;
    QByteArray reply;
    QDataStream result(&reply, QDataStream::WriteOnly);
    m_errorString.clear();

    if (function == "removeRecursive") {
        QString fileOrDir;
        params >> fileOrDir;
        result << removeRecursive(fileOrDir);
    } else if (function == "bindMountFileSystem") {
        QString from;
        QString to;
        bool readOnly;
        quint64 namespacePid;
        params >> from >> to >> readOnly >> namespacePid;
        result << bindMountFileSystem(from, to, readOnly, namespacePid);
    } else if (function == "setExtendedAttribute") {
        QString file;
        QByteArray attrName;
        QByteArray attrValue;
        params >> file >> attrName >> attrValue;
        result << setExtendedAttribute(file, attrName, attrValue);
    } else if (function == "stopServer") {
        m_stop = true;
    } else {
        reply.truncate(0);
        m_errorString = u"unknown function '%1' called in SudoServer"_s.arg(QString::fromLatin1(function));
    }
    return reply;
}

bool SudoServer::removeRecursive(const QString &fileOrDir)
{
    try {
        if (!recursiveOperation(fileOrDir, safeRemove))
            throw Exception(errno, "could not recursively remove %1").arg(fileOrDir);
        return true;
    } catch (const Exception &e) {
        m_errorString = e.errorString();
        return false;
    }
}

bool SudoServer::bindMountFileSystem(const QString &from, const QString &to, bool readOnly, quint64 namespacePid)
{
#if defined(Q_OS_LINUX)
    bool result = true;
    int oldNsFd = -1;

    try {
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

            int pidFd = int(::syscall(SYS_pidfd_open, pid_t(namespacePid), 0));
            if (pidFd < 0)
                throw Exception(errno, "process %1 is not available").arg(namespacePid);
            if (::setns(pidFd, CLONE_NEWNS) < 0)
                throw Exception(errno, "could not enter the mount namespace of process %1").arg(namespacePid);
        }

        // Mount the detached mount point to the final location within the mount namespace
        if (::syscall(SYS_move_mount, fromFd, "", -EBADF, qPrintable(to), MOVE_MOUNT_F_EMPTY_PATH) < 0)
            throw Exception(errno, "could not move the detached mount point to %1").arg(to);

    } catch (const Exception &e) {
        result = false;
        m_errorString = e.errorString();
    }

    if ((oldNsFd >= 0) && namespacePid) {
        // Restore our old mount namespace
        if (::setns(oldNsFd, CLONE_NEWNS) < 0)
            qFatal() << "SudoHelper process is halted: could not reset the mount namespace:" << strerror(errno);
    }

    return result;
#else
    Q_UNUSED(from)
    Q_UNUSED(to)
    Q_UNUSED(readOnly)
    Q_UNUSED(namespacePid)
    m_errorString = u"bindMountFileSystem is only available on Linux"_s;
    return false;
#endif // Q_OS_LINUX
}

bool SudoServer::setExtendedAttribute(const QString &file, const QByteArray &attrName, const QByteArray &attrValue)
{
#if defined(Q_OS_LINUX)
    bool result = true;

    try {
        if (::setxattr(qPrintable(file), attrName.constData(), attrValue.constData(), attrValue.size(), 0) != 0)
            throw Exception(errno, "could not set extended attribute '%1' on file '%2'").arg(attrName).arg(file);
        return true;
    } catch (const Exception &e) {
        result = false;
        m_errorString = e.errorString();
    }
    return result;
#else
    Q_UNUSED(file)
    Q_UNUSED(attrName)
    Q_UNUSED(attrValue)
    m_errorString = u"setExtendedAttribute is only available on Linux"_s;
    return false;
#endif // Q_OS_LINUX
}

QT_END_NAMESPACE_AM
