/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
#  include <sys/ioctl.h>
#  include <sys/stat.h>
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

#if defined(Q_OS_LINUX)
    uid_t realUid = getuid();
    uid_t effectiveUid = geteuid();
    canSudo = (realUid == 0) || (effectiveUid == 0);
#else
    Q_UNUSED(warnings)
    Q_UNUSED(dropPrivileges)
#endif

    if (!canSudo) {
        SudoServer::createInstance(-1);
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

        SudoServer::createInstance(socketFds[0]);
        ProcessTitle::setTitle("%s", "sudo helper");
        SudoServer::instance()->run();
    }
    // parent

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
    QDataStream result(&reply, QIODevice::WriteOnly);
    m_errorString.clear();

    if (function == "removeRecursive") {
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
