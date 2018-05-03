/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/


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
#include "global.h"

#include <errno.h>

#if defined(Q_OS_LINUX)
# include "processtitle.h"

#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/errno.h>
#  include <sys/mount.h>
#  include <sys/ioctl.h>
#  include <sys/stat.h>
#  include <mntent.h>
#  include <linux/loop.h>
#  include <sys/prctl.h>
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

// Missing support for dynamic loop device management
#  if !defined(LOOP_CTL_REMOVE)
#    define LOOP_CTL_REMOVE 0x4C81
#  endif
#  if !defined(LOOP_CTL_GET_FREE)
#    define LOOP_CTL_GET_FREE 0x4C82
#  endif

// Convenient way to ignore EINTR on any system call
#  define EINTR_LOOP(cmd) __extension__ ({__typeof__(cmd) res = 0; do { res = cmd; } while (res == -1 && errno == EINTR); res; })


// Declared as weak symbol here, so we can check at runtime if we were compiled against libgcov
extern "C" void __gcov_init() __attribute__((weak));


QT_BEGIN_NAMESPACE_AM

static void sigHupHandler(int sig)
{
    if (sig == SIGHUP)
        _exit(0);
}

QT_END_NAMESPACE_AM

#endif // Q_OS_LINUX


QT_BEGIN_NAMESPACE_AM

void Sudo::forkServer(DropPrivileges dropPrivileges, QStringList *warnings)
{
    bool canSudo = false;
    int loopControlFd = -1;

#if defined(Q_OS_LINUX)
    uid_t realUid = getuid();
    uid_t effectiveUid = geteuid();
    canSudo = (realUid == 0) || (effectiveUid == 0);

    // check for new style loopback device control
    loopControlFd = EINTR_LOOP(open("/dev/loop-control", O_RDWR));
    if (warnings && canSudo && (loopControlFd < 0))
        *warnings << qSL("Could not open /dev/loop-control, which is needed by the installer for SD-Card installations");
#else
    Q_UNUSED(warnings)
    Q_UNUSED(dropPrivileges)
#endif

    if (!canSudo) {
        SudoServer::createInstance(-1, loopControlFd);
        SudoClient::createInstance(-1, SudoServer::instance());
        if (warnings) {
            *warnings << qSL("For the installer to work correctly, the executable needs to be run either as root via sudo or SUID (preferred)");
            *warnings << qSL("(using fallback implementation - you might experience permission errors on installer operations)");
        }
        return;
    }

#if defined(Q_OS_LINUX)
    gid_t realGid = getgid();
    uid_t sudoUid = static_cast<uid_t>(qEnvironmentVariableIntValue("SUDO_UID"));

    // run as normal user (e.g. 1000): uid == 1000  euid == 1000
    // run with binary suid-root:      uid == 1000  euid == 0
    // run with sudo (no suid-root):   uid == 0     euid == 0    $SUDO_UID == 1000

    // treat sudo as special variant of a SUID executable
    if (realUid == 0 && effectiveUid == 0 && sudoUid != 0) {
        realUid = sudoUid;
        realGid = static_cast<gid_t>(qEnvironmentVariableIntValue("SUDO_GID"));

        if (setresgid(realGid, 0, 0) || setresuid(realUid, 0, 0))
            throw Exception(errno, "Could not set real user or group ID");
    }

    int socketFds[2];
    if (EINTR_LOOP(socketpair(AF_UNIX, SOCK_DGRAM, 0, socketFds)) != 0)
        throw Exception(errno, "Could not create a pair of sockets");

    // We need to make the gcda files generated by the root process writable by the normal user.
    // There is no way to detect a compilation with -ftest-coverage, but we can check for gcov
    // symbols at runtime. GCov will open all gcda files at fork() time, so we can get away with
    // switching umasks around the fork() call.

    mode_t realUmask = 0;
    if (__gcov_init)
        realUmask = umask(0);

    pid_t pid = fork();
    if (pid < 0) {
        throw Exception(errno, "Could not fork process");
    } else if (pid == 0) {
        // child
        close(0);
        setsid();

        // reset umask
        if (realUmask)
            umask(realUmask);

        // This call is Linux only, but it makes it so easy to detect a dying parent process.
        // We would have a big problem otherwise, since the main process drops its privileges,
        // which prevents it from sending SIGHUP to the child process, which still runs with
        // root privileges.
        prctl(PR_SET_PDEATHSIG, SIGHUP);
        signal(SIGHUP, sigHupHandler);

        // Drop as many capabilities as possible, just to be on the safe side
        static const quint32 neededCapabilities[] = {
            CAP_SYS_ADMIN,
            CAP_CHOWN,
            CAP_FOWNER,
            CAP_DAC_OVERRIDE
        };

        bool capSetOk = false;
        __user_cap_header_struct capHeader { AM_CAP_VERSION, getpid() };
        __user_cap_data_struct capData[AM_CAP_SIZE];
        if (capget(&capHeader, capData) == 0) {
            quint32 capNeeded[AM_CAP_SIZE];
            memset(&capNeeded, 0, sizeof(capNeeded));
            for (quint32 cap : neededCapabilities) {
                int idx = CAP_TO_INDEX(cap);
                Q_ASSERT(idx < AM_CAP_SIZE);
                capNeeded[idx] |= CAP_TO_MASK(cap);
            }
            for (int i = 0; i < AM_CAP_SIZE; ++i)
                capData[i].effective = capData[i].permitted = capData[i].inheritable = capNeeded[i];
            if (capset(&capHeader, capData) == 0)
                capSetOk = true;
        }
        if (!capSetOk)
            qCCritical(LogSystem) << "could not drop privileges in the SudoServer process -- continuing with full root privileges";

        SudoServer::createInstance(socketFds[0], loopControlFd);
        ProcessTitle::setTitle("%s", "sudo helper");
        SudoServer::instance()->run();
    }
    // parent
    close(loopControlFd);

    // reset umask
    if (realUmask)
        umask(realUmask);

    SudoClient::createInstance(socketFds[1]);

    if (realUid != effectiveUid) {
        // drop all root privileges
        if (dropPrivileges == DropPrivilegesPermanently) {
            if (setresgid(realGid, realGid, realGid) || setresuid(realUid, realUid, realUid)) {
                kill(pid, SIGKILL);
                throw Exception(errno, "Could not set real user or group ID");
            }
        } else {
            qCCritical(LogSystem) << "\nSudo was instructed to NOT drop root privileges permanently.\nThis is dangerous and should only be used in auto-tests!\n";
            if (setresgid(realGid, realGid, 0) || setresuid(realUid, realUid, 0)) {
                kill(pid, 9);
                throw Exception(errno, "Could not set real user or group ID");
            }
        }
    }
    ::atexit([]() { SudoClient::instance()->stopServer(); });
#endif
}

SudoInterface::SudoInterface()
{ }

#ifdef Q_OS_LINUX
bool SudoInterface::sendMessage(int socket, const QByteArray &msg, MessageType type, const QString &errorString)
{
    QByteArray packet;
    QDataStream ds(&packet, QIODevice::WriteOnly);
    ds << errorString << msg;
    packet.prepend((type == Request) ? "RQST" : "RPLY");

    auto bytesWritten = EINTR_LOOP(write(socket, packet.constData(), static_cast<size_t>(packet.size())));
    return bytesWritten == packet.size();
}


QByteArray SudoInterface::receiveMessage(int socket, MessageType type, QString *errorString)
{
    const int headerSize = 4;
    char recvBuffer[8*1024];
    auto bytesReceived = EINTR_LOOP(recv(socket, recvBuffer, sizeof(recvBuffer), 0));

    if ((bytesReceived < headerSize) || qstrncmp(recvBuffer, (type == Request ? "RQST" : "RPLY"), 4)) {
        *errorString = qL1S("failed to receive command from the SudoClient process");
        //qCCritical(LogSystem) << *errorString;
        return QByteArray();
    }

    QByteArray packet(recvBuffer + headerSize, int(bytesReceived) - headerSize);

    QDataStream ds(&packet, QIODevice::ReadOnly);
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
    QDataStream(&msg, QIODevice::WriteOnly) << #FUNC_NAME << PARAM; \
    QByteArray reply = call(msg); \
    QDataStream result(&reply, QIODevice::ReadOnly); \
    decltype(returnType(&SudoClient::FUNC_NAME)) r; \
    result >> r; \
    return r

QString SudoClient::attachLoopback(const QString &imagePath, bool readonly)
{
    CALL(attachLoopback, imagePath << readonly);
}

bool SudoClient::detachLoopback(const QString &loopDev)
{
    CALL(detachLoopback, loopDev);
}

bool SudoClient::mount(const QString &device, const QString &mountPoint, bool readonly, const QString &fstype)
{
    CALL(mount, device << mountPoint << readonly << fstype);
}

bool SudoClient::unmount(const QString &mountPoint, bool force)
{
    CALL(unmount, mountPoint << force);
}

bool SudoClient::mkfs(const QString &device, const QString &fstype, const QStringList &options)
{
    CALL(mkfs, device << fstype << options);
}

bool SudoClient::removeRecursive(const QString &fileOrDir)
{
    CALL(removeRecursive, fileOrDir);
}

bool SudoClient::setOwnerAndPermissionsRecursive(const QString &fileOrDir, uid_t user, gid_t group, mode_t permissions)
{
    CALL(setOwnerAndPermissionsRecursive, fileOrDir << user << group << permissions);
}

void SudoClient::stopServer()
{
#ifdef Q_OS_LINUX
    if (!m_shortCircuit && m_socket >= 0) {
        QByteArray msg;
        QDataStream(&msg, QIODevice::WriteOnly) << "stopServer";
        sendMessage(m_socket, msg, Request);
    }
#endif
}

QByteArray SudoClient::call(const QByteArray &msg)
{
    QMutexLocker locker(&m_mutex);

    if (m_shortCircuit)
        return m_shortCircuit->receive(msg);

#ifdef Q_OS_LINUX
    if (m_socket >= 0) {
        if (sendMessage(m_socket, msg, Request))
            return receiveMessage(m_socket, Reply, &m_errorString);
    }
#else
    Q_UNUSED(m_socket)
#endif

    //qCCritical(LogSystem) << "failed to send command to the SudoServer process";
    m_errorString = qL1S("failed to send command to the SudoServer process");
    return QByteArray();
}



SudoServer *SudoServer::s_instance = nullptr;

SudoServer *SudoServer::instance()
{
    return s_instance;
}

SudoServer::SudoServer(int socketFd, int loopControlFd)
    : m_socket(socketFd)
    , m_loopControl(loopControlFd)
{ }

SudoServer *SudoServer::createInstance(int socketFd, int loopControlFd)
{
    if (!s_instance)
        s_instance = new SudoServer(socketFd, loopControlFd);
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
    Q_UNUSED(m_loopControl)
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
    QDataStream result(&reply, QIODevice::WriteOnly);
    m_errorString.clear();

    if (function == "attachLoopback") {
        QString imagePath;
        bool readonly;
        params >> imagePath >> readonly;
        result << attachLoopback(imagePath, readonly);
    } else if (function == "detachLoopback") {
        QString loopDev;
        params >> loopDev;
        result << detachLoopback(loopDev);
    } else if (function == QByteArray("mount")) {
        QString device;
        QString mountPoint;
        bool readonly;
        QString fstype;
        params >> device >> mountPoint >> readonly >> fstype;
        result << mount(device, mountPoint, readonly, fstype);
    } else if (function == "unmount") {
        QString mountPoint;
        bool force;
        params >> mountPoint >> force;
        result << unmount(mountPoint, force);
    } else if (function == "mkfs") {
        QString device;
        QString fstype;
        QStringList options;
        params >> device >> fstype >> options;
        result << mkfs(device, fstype, options);
    } else if (function == "removeRecursive") {
        QString fileOrDir;
        params >> fileOrDir;
        result << removeRecursive(fileOrDir);
    } else if (function == "setOwnerAndPermissionsRecursive") {
        QString fileOrDir;
        uid_t user;
        gid_t group;
        mode_t permissions;
        params >> fileOrDir >> user >> group >> permissions;
        result << setOwnerAndPermissionsRecursive(fileOrDir, user, group, permissions);
    } else if (function == "stopServer") {
        m_stop = true;
    } else {
        reply.truncate(0);
        m_errorString = QString::fromLatin1("unknown function '%1' called in SudoServer").arg(qL1S(function));
    }
    return reply;
}

QString SudoServer::attachLoopback(const QString &imagePath, bool readonly)
{
    QString loopDev;
#if defined(Q_OS_LINUX)
    int loopFd = -1;
    int imageFd = -1;

    try {
        if (!QFile::exists(imagePath))
            throw Exception(Error::IO, "image %1 does not exist").arg(imagePath);

        // the kernel API has a race condition between getting the next
        // free loop device number and actually claiming it.
        for (int tries = 0; tries < 100; ++tries) {
            int loopId = EINTR_LOOP(ioctl(m_loopControl, LOOP_CTL_GET_FREE));

            if (loopId < 0)
                throw Exception(Error::IO, "the system could not allocate more loop devices");

            loopDev = qSL("/dev/loop%1").arg(loopId);
            loopFd = EINTR_LOOP(open(loopDev.toLocal8Bit(), readonly ? O_RDONLY : O_RDWR));

            if (loopFd < 0) {
                if (errno == EACCES)
                    continue; // race condition - retry
                else
                    throw Exception(Error::IO, "could not open loop device %1: %2").arg(loopDev).arg(QString::fromLocal8Bit(strerror(errno)));
            }

            struct loop_info64 loopInfo;

            if (EINTR_LOOP(ioctl(loopFd, LOOP_GET_STATUS64, &loopInfo)) == 0) {
                memset(&loopInfo, 0, sizeof(loopInfo));
                EINTR_LOOP(close(loopFd));
                continue; // race condition - retry
            } else if (errno == ENXIO) {
                break; // found a free one
            } else {
                throw Exception(Error::IO, "could not call LOOP_GET_STATUS64 on loop device: %1")
                        .arg(QString::fromLocal8Bit(strerror(errno)));
            }
        }

        if (loopFd < 0)
            throw Exception(Error::IO, "could not find a free loop device");

        imageFd = EINTR_LOOP(open(imagePath.toLocal8Bit(), readonly ? O_RDONLY : O_RDWR));
        if (imageFd < 0)
            throw Exception(Error::IO, "could not open image file %1: %2").arg(imagePath).arg(QString::fromLocal8Bit(strerror(errno)));

        if (EINTR_LOOP(ioctl(loopFd, LOOP_SET_FD, imageFd)) < 0)
            throw Exception(Error::IO, "could not attach the image %1 to the loop device %2: %3").arg(imagePath).arg(loopDev).arg(QString::fromLocal8Bit(strerror(errno)));

    } catch (const Exception &e) {
        m_errorString = e.errorString();
        loopDev.clear();
    }

    if (imageFd >= 0)
        EINTR_LOOP(close(imageFd));
    if (loopFd >= 0)
        EINTR_LOOP(close(loopFd));
#else
    Q_UNUSED(imagePath)
    Q_UNUSED(readonly)
#endif // Q_OS_LINUX

    return loopDev;
}

bool SudoServer::detachLoopback(const QString &loopDev)
{
    bool result = false;
#if defined(Q_OS_LINUX)
    int loopFd = -1;
    int loopId = -1;

    try {
        if (!loopDev.startsWith(qL1S("/dev/loop"))
                || [&loopId,&loopDev]{ bool ok; loopId = loopDev.midRef(9).toInt(&ok); return !ok; }()
                || (loopId < 0)) {
            throw Exception(Error::IO, "invalid loop device name: %1").arg(loopDev);
        }

        if ((loopFd = EINTR_LOOP(open(loopDev.toLocal8Bit(), O_RDWR))) < 0)
            throw Exception(Error::IO, "could not open loop device %1: %2").arg(loopDev).arg(QString::fromLocal8Bit(strerror(errno)));

        // we are working with very small delays in the micro-second range here, so a linear factor
        // to support valgrind would have to be very large and probably conflict with usage elsewhere
        // in the codebase, where the ranges are normally in the seconds.
        static const int timeout = timeoutFactor() * timeoutFactor() * 50;

        int clearErrno = 0;
        for (int tries = 0; tries < 100; ++tries) {
            if (EINTR_LOOP(ioctl(loopFd, LOOP_CLR_FD, 0)) == 0)
                break;

            clearErrno = errno;
            usleep(static_cast<useconds_t>(timeout)); // might still be busy after lazy umount
        }

        EINTR_LOOP(close(loopFd));

#if !defined(AM_SYSTEMD_WORKAROUND)
        // be nice and tell the kernel to clean up
        EINTR_LOOP(ioctl(m_loopControl, LOOP_CTL_REMOVE, loopId));
#endif
        if (clearErrno)
            throw Exception(Error::IO, "could not clear loop device %1: %2").arg(loopDev).arg(QString::fromLocal8Bit(strerror(clearErrno)));

        result = true;
    } catch (const Exception &e) {
        m_errorString = e.errorString();
    }

    if (loopFd >= 0)
        EINTR_LOOP(close(loopFd));

#else
    Q_UNUSED(loopDev)
#endif // Q_OS_LINUX
    return result;
}


bool SudoServer::mount(const QString &device, const QString &mountPoint, bool readonly, const QString &fstype)
{
#if defined(Q_OS_LINUX)
    unsigned long options = MS_MGC_VAL | (readonly ? MS_RDONLY : 0);

    try {
        if (!QFile::exists(device))
            throw Exception(Error::IO, "device %1 does not exist").arg(device);
        if (!QDir(mountPoint).exists())
            throw Exception(Error::IO, "mount point %1 does not exist").arg(mountPoint);

        if (::mount(device.toLocal8Bit(),
                    mountPoint.toLocal8Bit(),
                    fstype.toLocal8Bit(),
                    options, nullptr) < 0) {
            throw Exception(Error::IO, "could not mount device %1 to %2: %3").arg(device).arg(mountPoint).arg(QString::fromLocal8Bit(strerror(errno)));
        }
        return true;

    } catch (const Exception &e) {
        m_errorString = e.errorString();
    }
#else
    Q_UNUSED(device)
    Q_UNUSED(mountPoint)
    Q_UNUSED(readonly)
    Q_UNUSED(fstype)
#endif // Q_OS_LINUX
    return false;
}

bool SudoServer::unmount(const QString &mountPoint, bool force)
{
#if defined(Q_OS_LINUX)
    int options = force ? MNT_FORCE | MNT_DETACH : 0;

    try {
        if (!QDir(mountPoint).exists() && !force)
            throw Exception(Error::IO, "mount point %1 does not exist").arg(mountPoint);

        if (umount2(mountPoint.toLocal8Bit(), options) < 0)
            throw Exception(Error::IO, "could not un-mount %1: %2").arg(mountPoint).arg(QString::fromLocal8Bit(strerror(errno)));

        return true;
    } catch (const Exception &e) {
        m_errorString = e.errorString();
    }
#else
    Q_UNUSED(mountPoint)
    Q_UNUSED(force)
#endif // Q_OS_LINUX
    return false;
}


bool SudoServer::mkfs(const QString &device, const QString &fstype, const QStringList &options)
{
#if defined(Q_OS_LINUX)
    static QString mkfsBaseCmd(qSL("/sbin/mkfs."));
    static QString tune2fsCmd(qSL("/sbin/tune2fs"));

    bool isExt2 = fstype.startsWith(qL1S("ext")) && (fstype.midRef(3).toInt() >= 2);

    try {
        if (!QFile::exists(device))
            throw Exception("device %1 does not exist").arg(device);

        QString mkfsCmd = mkfsBaseCmd + fstype;

        if (!QFileInfo(mkfsCmd).isExecutable())
            throw Exception("the binary %1 is not available and executable").arg(mkfsCmd);

        QStringList mkfsOptions = options;

        if (isExt2) {
            if (!QFileInfo(tune2fsCmd).isExecutable())
                throw Exception("the binary %1 is not available and executable").arg(tune2fsCmd);

            // defaults to create a loop mounted app image
            if (options.isEmpty()) {
                mkfsOptions = QStringList() << qSL("-F")
                                            << qSL("-i") << qSL("4096")
                                            << qSL("-I") << qSL("128")
                                            << qSL("-b") << qSL("1024")
                                            << qSL("-m") << qSL("0")
                                            << qSL("-E") << qSL("root_owner=%1:%2").arg(getuid()).arg(getgid())
                                            << qSL("-O") << qSL("sparse_super,^resize_inode");
            }
        }

        mkfsOptions << device;

        static const int timeout = 20000 * timeoutFactor();

        QProcess p;
        p.setProgram(mkfsCmd);
        p.setArguments(mkfsOptions);

        p.start();
        p.waitForFinished(timeout);
        if (p.exitCode() != 0) {
            throw Exception("could not create an %1 filesystem on %2: %3")
                .arg(fstype).arg(device).arg(QString::fromLocal8Bit(p.readAllStandardError()));
        }

        if (isExt2) {
            // disable file-system checks on mount
            p.setProgram(tune2fsCmd);
            p.setArguments(QStringList() << qSL("-c") << qSL("0")
                                         << qSL("-i") << qSL("0")
                                         << device);
            p.start();
            p.waitForFinished(timeout);
            if (p.exitCode() != 0) {
                throw Exception("could not disable ext2 filesystem checks on %2: %3")
                        .arg(device).arg(QString::fromLocal8Bit(p.readAllStandardError()));
            }
        }
        return true;

    } catch (const Exception &e) {
        m_errorString = e.errorString();
    }
#else
    Q_UNUSED(device)
    Q_UNUSED(fstype)
    Q_UNUSED(options)
#endif // Q_OS_LINUX
    return false;
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

bool SudoServer::setOwnerAndPermissionsRecursive(const QString &fileOrDir, uid_t user, gid_t group, mode_t permissions)
{
#if defined(Q_OS_LINUX)
    static auto setOwnerAndPermissions =
            [user, group, permissions](const QString &path, RecursiveOperationType type) -> bool {
        if (type == RecursiveOperationType::EnterDirectory)
            return true;

        const QByteArray localPath = path.toLocal8Bit();
        mode_t mode = permissions;

        if (type == RecursiveOperationType::LeaveDirectory) {
            // set the x bit for directories, but only where it makes sense
            if (mode & 06)
                mode |= 01;
            if (mode & 060)
                mode |= 010;
            if (mode & 0600)
                mode |= 0100;
        }

        return ((chmod(localPath, mode) == 0) && (chown(localPath, user, group) == 0));
    };

    try {
        if (!recursiveOperation(fileOrDir, setOwnerAndPermissions)) {
            throw Exception(errno, "could not recursively set owner and permission on %1 to %2:%3 / %4")
                .arg(fileOrDir).arg(user).arg(group).arg(permissions, 4, 8, QLatin1Char('0'));
        }
    } catch (const Exception &e) {
        m_errorString = e.errorString();
        return false;
    }
#else
    Q_UNUSED(fileOrDir)
    Q_UNUSED(user)
    Q_UNUSED(group)
    Q_UNUSED(permissions)
#endif // Q_OS_LINUX
    return false;
}

QT_END_NAMESPACE_AM
