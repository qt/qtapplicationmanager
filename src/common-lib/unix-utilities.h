// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:privilege-management

#ifndef UNIX_UTILITIES_H
#define UNIX_UTILITIES_H

#include <QtAppManCommon/qtappmancommonglobal.h>

#if defined(Q_OS_UNIX)

#  include <QtCore/qbasicatomic.h>
#  include <QtCore/QByteArray>
#  include <QtCore/QSet>

#  include <vector>

#  include <sys/types.h>
#  include <sys/param.h>

struct passwd;
struct group;

QT_BEGIN_NAMESPACE_AM

namespace Unix {

class Q_APPMANCOMMON_EXPORT Capability
{
public:
    // Mirrors the CAP_* defines from <linux/capability.h> numerically, so that callers do not
    // need to include the kernel header. The values are verified against the kernel ABI via
    // static_asserts in unix-utilities.cpp.
    enum class Cap : quint32 {
        Chown             = 0,
        DacOverride       = 1,
        DacReadSearch     = 2,
        Fowner            = 3,
        Fsetid            = 4,
        Kill              = 5,
        Setgid            = 6,
        Setuid            = 7,
        Setpcap           = 8,
        LinuxImmutable    = 9,
        NetBindService    = 10,
        NetBroadcast      = 11,
        NetAdmin          = 12,
        NetRaw            = 13,
        IpcLock           = 14,
        IpcOwner          = 15,
        SysModule         = 16,
        SysRawio          = 17,
        SysChroot         = 18,
        SysPtrace         = 19,
        SysPacct          = 20,
        SysAdmin          = 21,
        SysBoot           = 22,
        SysNice           = 23,
        SysResource       = 24,
        SysTime           = 25,
        SysTtyConfig      = 26,
        Mknod             = 27,
        Lease             = 28,
        AuditWrite        = 29,
        AuditControl      = 30,
        Setfcap           = 31,
        MacOverride       = 32,
        MacAdmin          = 33,
        Syslog            = 34,
        WakeAlarm         = 35,
        BlockSuspend      = 36,
        AuditRead         = 37,
        Perfmon           = 38,
        Bpf               = 39,
        CheckpointRestore = 40,
    };

    static void reduceTo(const std::vector<Cap> &capsToKeep) noexcept(false);
};


class Group;

class Q_APPMANCOMMON_EXPORT User
{
public:
    User();
    User(const User &other);
    ~User();

    User &operator=(const User &) = default;

    bool isValid() const { return m_valid; }
    uid_t uid() const { return m_uid; }
    gid_t gid() const { return m_gid; }
    const char *name() const { return m_name.constData(); }
    const char *dir() const { return m_dir.constData(); }
    const char *shell() const { return m_shell.constData(); }

    QSet<gid_t> supplementaryGroupIds(const Group &group) noexcept(false);

    void setCurrent(bool permanently = true) noexcept(false);

    static void setCurrentSupplementaryGroupIds(const QSet<gid_t> setSupGids) noexcept(false);

    // The result is always valid
    static User parse(const QByteArray &user) noexcept(false);

    static uid_t currentId();
    static uid_t currentEffectiveId();

private:
    explicit User(const struct ::passwd *pw);

    bool m_valid;
    QByteArray m_name;
    uid_t m_uid = static_cast<uid_t>(-1);
    gid_t m_gid = static_cast<gid_t>(-1);
    QByteArray m_dir;
    QByteArray m_shell;
    static quint64 s_count;
};


class Q_APPMANCOMMON_EXPORT Group
{
public:
    Group();
    Group(const Group &other);
    ~Group();

    Group &operator=(const Group &) = default;

    bool isValid() const { return m_valid; }
    gid_t gid() const { return m_gid; }
    const char *name() const { return m_name.constData(); }

    void setCurrent(bool permanently = true) noexcept(false);

    // The result is always valid
    static Group parse(const QByteArray &group) noexcept(false);

    // The result is always valid
    static Group fromUser(const User &user) noexcept(false);

    static constexpr int MaxSupplementary = NGROUPS_MAX;

private:
    explicit Group(const struct ::group *gr);

    bool m_valid;
    QByteArray m_name;
    gid_t m_gid;
    static quint64 s_count;
};


class Q_APPMANCOMMON_EXPORT Fd
{
public:
    Fd() = default;
    Fd(int fd) : m_fd(fd) { }
    Fd(const Fd &) = delete;
    Fd(Fd &&mv) { reset(mv.release()); }
    ~Fd() { reset(); }
    int get() const { return m_fd; }
    int release() { return m_fd.fetchAndStoreOrdered(-1); }
    void reset(int newFd = -1)
    {
        int oldFd = m_fd.fetchAndStoreOrdered(newFd);
        if ((oldFd >= 0) && (oldFd != newFd))
            closeImpl(oldFd);
    }

    explicit operator bool() const { return m_fd != -1; }
    explicit operator int() const { return m_fd; }
    int operator*() const { return m_fd; }

    Fd &operator=(const Fd &) = delete;
    Fd &operator=(Fd &&mv)
    {
        reset(mv.release());
        return *this;
    }

private:
    QBasicAtomicInt m_fd = -1;

    static void closeImpl(int fd);
};

} // namespace Unix

QT_END_NAMESPACE_AM

#endif // defined(Q_OS_UNIX)

#endif // UNIX_UTILITIES_H
