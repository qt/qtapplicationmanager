// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:privilege-management

#include "unix-utilities.h"

#if defined(Q_OS_UNIX)
#  include <QtCore/QVector>
#  include <QtCore/private/qcore_unix_p.h>

#  include "exception.h"

#  include <array>
#  include <cerrno>
#  include <unistd.h>
#  include <pwd.h>
#  include <grp.h>

using namespace Qt::StringLiterals;

///////////////////////////////////////////////////////////////////////
// Capability
///////////////////////////////////////////////////////////////////////

#  if defined(Q_OS_LINUX)
#    include <linux/capability.h>

// These two functions are implemented in glibc, but the header file is
// in the separate libcap-dev package. Since we want to avoid unnecessary
// dependencies, we just declare them here
extern "C" int capset(cap_user_header_t header, cap_user_data_t data);
extern "C" int capget(cap_user_header_t header, const cap_user_data_t data);

// Support for old/broken C libraries
#    if defined(_LINUX_CAPABILITY_VERSION) && !defined(_LINUX_CAPABILITY_VERSION_1)
#      define _LINUX_CAPABILITY_VERSION_1 _LINUX_CAPABILITY_VERSION
#      define _LINUX_CAPABILITY_U32S_1    1
#      if !defined(CAP_TO_INDEX)
#        define CAP_TO_INDEX(x) ((x) >> 5)
#      endif
#      if !defined(CAP_TO_MASK)
#        define CAP_TO_MASK(x)  (1 << ((x) & 31))
#      endif
#    endif
#    if defined(_LINUX_CAPABILITY_VERSION_3) // use 64-bit support, if available
#      define AM_CAP_VERSION _LINUX_CAPABILITY_VERSION_3
#      define AM_CAP_SIZE    _LINUX_CAPABILITY_U32S_3
#    else // fallback to 32-bit support
#      define AM_CAP_VERSION _LINUX_CAPABILITY_VERSION_1
#      define AM_CAP_SIZE    _LINUX_CAPABILITY_U32S_1
#    endif

// Verify that the hand-maintained Capability::Cap enum still matches the kernel ABI. The newest
// caps may be missing from older kernel headers, so they are guarded individually.
#    define AM_VERIFY_CAP(Enum, Macro) \
        static_assert(quint32(QtAM::Unix::Capability::Cap::Enum) == (Macro), #Macro " value changed")

AM_VERIFY_CAP(Chown,          CAP_CHOWN);
AM_VERIFY_CAP(DacOverride,    CAP_DAC_OVERRIDE);
AM_VERIFY_CAP(DacReadSearch,  CAP_DAC_READ_SEARCH);
AM_VERIFY_CAP(Fowner,         CAP_FOWNER);
AM_VERIFY_CAP(Fsetid,         CAP_FSETID);
AM_VERIFY_CAP(Kill,           CAP_KILL);
AM_VERIFY_CAP(Setgid,         CAP_SETGID);
AM_VERIFY_CAP(Setuid,         CAP_SETUID);
AM_VERIFY_CAP(Setpcap,        CAP_SETPCAP);
AM_VERIFY_CAP(LinuxImmutable, CAP_LINUX_IMMUTABLE);
AM_VERIFY_CAP(NetBindService, CAP_NET_BIND_SERVICE);
AM_VERIFY_CAP(NetBroadcast,   CAP_NET_BROADCAST);
AM_VERIFY_CAP(NetAdmin,       CAP_NET_ADMIN);
AM_VERIFY_CAP(NetRaw,         CAP_NET_RAW);
AM_VERIFY_CAP(IpcLock,        CAP_IPC_LOCK);
AM_VERIFY_CAP(IpcOwner,       CAP_IPC_OWNER);
AM_VERIFY_CAP(SysModule,      CAP_SYS_MODULE);
AM_VERIFY_CAP(SysRawio,       CAP_SYS_RAWIO);
AM_VERIFY_CAP(SysChroot,      CAP_SYS_CHROOT);
AM_VERIFY_CAP(SysPtrace,      CAP_SYS_PTRACE);
AM_VERIFY_CAP(SysPacct,       CAP_SYS_PACCT);
AM_VERIFY_CAP(SysAdmin,       CAP_SYS_ADMIN);
AM_VERIFY_CAP(SysBoot,        CAP_SYS_BOOT);
AM_VERIFY_CAP(SysNice,        CAP_SYS_NICE);
AM_VERIFY_CAP(SysResource,    CAP_SYS_RESOURCE);
AM_VERIFY_CAP(SysTime,        CAP_SYS_TIME);
AM_VERIFY_CAP(SysTtyConfig,   CAP_SYS_TTY_CONFIG);
AM_VERIFY_CAP(Mknod,          CAP_MKNOD);
AM_VERIFY_CAP(Lease,          CAP_LEASE);
AM_VERIFY_CAP(AuditWrite,     CAP_AUDIT_WRITE);
AM_VERIFY_CAP(AuditControl,   CAP_AUDIT_CONTROL);
AM_VERIFY_CAP(Setfcap,        CAP_SETFCAP);
AM_VERIFY_CAP(MacOverride,    CAP_MAC_OVERRIDE);
AM_VERIFY_CAP(MacAdmin,       CAP_MAC_ADMIN);
AM_VERIFY_CAP(Syslog,         CAP_SYSLOG);
AM_VERIFY_CAP(WakeAlarm,      CAP_WAKE_ALARM);
AM_VERIFY_CAP(BlockSuspend,   CAP_BLOCK_SUSPEND);
AM_VERIFY_CAP(AuditRead,      CAP_AUDIT_READ);
#    ifdef CAP_PERFMON
AM_VERIFY_CAP(Perfmon,        CAP_PERFMON);
#    endif
#    ifdef CAP_BPF
AM_VERIFY_CAP(Bpf,            CAP_BPF);
#    endif
#    ifdef CAP_CHECKPOINT_RESTORE
AM_VERIFY_CAP(CheckpointRestore, CAP_CHECKPOINT_RESTORE);
#    endif

#    undef AM_VERIFY_CAP
#  endif // Q_OS_LINUX

QT_BEGIN_NAMESPACE_AM

namespace Unix {

void Capability::reduceTo(const std::vector<Cap> &capsToKeep)
{
#  if defined(Q_OS_LINUX)
    __user_cap_header_struct capHeader { AM_CAP_VERSION, ::getpid() };
    std::array<__user_cap_data_struct, AM_CAP_SIZE> capData { };

    if (::capget(&capHeader, capData.data()) != 0)
        throw Exception(errno, "capget failed");

    std::array<quint32, AM_CAP_SIZE> capNeeded { };
    for (Cap cap : capsToKeep) {
        const quint32 capValue = quint32(cap);
        if (!cap_valid(capValue))
            throw Exception("invalid capability %1").arg(capValue);
        capNeeded[CAP_TO_INDEX(capValue)] |= CAP_TO_MASK(capValue);
    }
    for (int i = 0; i < AM_CAP_SIZE; ++i) {
        capData[i].effective = capData[i].permitted = capNeeded[i];
        capData[i].inheritable = 0;
    }
    if (::capset(&capHeader, capData.data()) != 0)
        throw Exception(errno, "capset failed");
#  else
    Q_UNUSED(capsToKeep)
    throw Exception("Capability::reduceTo only works on Linux");
#  endif // Q_OS_LINUX
}


///////////////////////////////////////////////////////////////////////
// User
///////////////////////////////////////////////////////////////////////


quint64 User::s_count = 0;

User::User() : User(nullptr) { }

User::User(const User &other)
    : m_valid(other.m_valid)
    , m_name(other.m_name)
    , m_uid(other.m_uid)
    , m_gid(other.m_gid)
    , m_dir(other.m_dir)
    , m_shell(other.m_shell)
{
    ++s_count;
}

User::~User()
{
    if (--s_count == 0)
        ::endpwent();
}

void User::setCurrent(bool permanently)
{
#if defined(Q_OS_QNX) || defined(Q_OS_DARWIN)
    if (!permanently)
        throw Exception("Temporary user switching is not supported on QNX or Darwin");
    if (::setreuid(uid(), uid()) < 0) {
#else
    if (::setresuid(uid(), uid(), permanently ? uid() : 0) < 0) {
#endif
        throw Exception(errno, "Could not %1set the user to %2")
            .arg(permanently ? "permanently " : "").arg(name());
    }
}

void User::setCurrentSupplementaryGroupIds(const QSet<gid_t> setSupGids)
{
    if (::setgroups(setSupGids.size(), QVector<gid_t>(setSupGids.cbegin(), setSupGids.cend()).constData()) < 0)
        throw Exception(errno, "Could not set supplementary groups (%2)").arg(setSupGids);
}

User User::parse(const QByteArray &user)
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

User::User(const struct ::passwd *pw)
    : m_valid(pw)
    , m_name(pw ? pw->pw_name : "")
    , m_uid(pw ? pw->pw_uid : static_cast<uid_t>(-1))
    , m_gid(pw ? pw->pw_gid : static_cast<gid_t>(-1))
    , m_dir(pw ? pw->pw_dir : "")
    , m_shell(pw ? pw->pw_shell : "")
{
    ++s_count;
}

QSet<gid_t> User::supplementaryGroupIds(const Group &mainGroup)
{
    gid_t supGids[NGROUPS_MAX + 1];
    int supGidsLen = NGROUPS_MAX + 1;
    gid_t mainGid = mainGroup.isValid() ? mainGroup.gid() : gid();
#if defined(Q_OS_DARWIN)
    if (::getgrouplist(name(), (int) mainGid, (int *) supGids, &supGidsLen) < 0)
#else
    if (::getgrouplist(name(), mainGid, supGids, &supGidsLen) < 0)
#endif
        throw Exception("Could not get supplementary groups for user %1").arg(name());
    QSet<gid_t> result { supGids, supGids + supGidsLen };
    result.remove(mainGid);
    return result;
}

uid_t User::currentId()
{
    return ::getuid();
}

uid_t User::currentEffectiveId()
{
    return ::geteuid();
}


///////////////////////////////////////////////////////////////////////
// Group
///////////////////////////////////////////////////////////////////////


quint64 Group::s_count = 0;

Group::Group() : Group(nullptr) { }

Group::Group(const Group &other)
    : m_valid(other.m_valid)
    , m_name(other.m_name)
    , m_gid(other.m_gid)
{
    ++s_count;
}

Group::~Group()
{
    if (--s_count == 0)
        ::endgrent();
}

void Group::setCurrent(bool permanently)
{
#if defined(Q_OS_QNX) || defined(Q_OS_DARWIN)
    if (!permanently)
        throw Exception("Temporary group switching is not supported on QNX or Darwin");
    if (::setregid(gid(), gid()) < 0) {
#else
    if (::setresgid(gid(), gid(), permanently ? gid() : 0) < 0) {
#endif
        throw Exception(errno, "Could not %1set the group to %2")
            .arg(permanently ? "permanently ": "").arg(name());
    }
}

Group Group::parse(const QByteArray &group)
{
    bool ok;
    if (gid_t gid = group.toUInt(&ok); ok) {
        if (struct ::group *gr = ::getgrgid(gid))
            return Group(gr);
    }
    if (struct ::group *gr = ::getgrnam(group.constData()))
        return Group(gr);
    throw Exception("unknown group '%1'").arg(group);
}

Group Group::fromUser(const User &user)
{
    if (user.isValid()) {
        if (struct ::group *gr = ::getgrgid(user.gid()))
            return Group(gr);
    }
    throw Exception("cannot determine group of user '%1'").arg(user.isValid() ? user.name() : "<unknown>");
}

Group::Group(const struct ::group *gr)
    : m_valid(gr)
    , m_name(gr ? gr->gr_name : "")
    , m_gid(gr ? gr->gr_gid : static_cast<gid_t>(-1))
{
    ++s_count;
}


///////////////////////////////////////////////////////////////////////
// Fd
///////////////////////////////////////////////////////////////////////


void Fd::closeImpl(int fd)
{
    qt_safe_close(fd);
}

} // namespace Unix

QT_END_NAMESPACE_AM

#endif // Q_OS_UNIX
