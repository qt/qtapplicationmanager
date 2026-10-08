// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QRegularExpression>
#include <qplatformdefs.h>
#include "systemd.h"
#include "exception.h"
#include "logging.h"

#if defined(Q_OS_LINUX)
#  include <sys/socket.h>
#  include <sys/un.h>
#  include <unistd.h>
#endif

using namespace std::chrono_literals;

QT_BEGIN_NAMESPACE_AM

Systemd *Systemd::instance()
{
    static Systemd instance;
    return &instance;
}

Systemd::~Systemd()
{
    if (m_notifySocketFd != -1)
        QT_CLOSE(m_notifySocketFd);
}

Systemd::Systemd()
{
#if defined(Q_OS_LINUX)
    auto getAndUnset = [](const char *name) {
        auto var = qgetenv(name);
        qunsetenv(name);
        return var;
    };

    m_notifySocket  = getAndUnset("NOTIFY_SOCKET");
    m_watchdogUsec  = getAndUnset("WATCHDOG_USEC");
    m_watchdogPid   = getAndUnset("WATCHDOG_PID");
#endif
}

bool Systemd::checkPid(const QByteArray &pidVar)
{
    if (!pidVar.isEmpty()) {
        qint64 pid = pidVar.toLongLong();
        if (!pid || (pid != QCoreApplication::applicationPid()))
            return false;
    }
    return true;
}

bool Systemd::notify(const QString &state)
{
    if (m_notifySocket.isEmpty())
        return false;

    try {
        QByteArray stateStr = state.toUtf8();
        if (stateStr.isEmpty())
            throw Exception("empty notify messages are not allowed");

        // connect lazily, keep the connection open, but only try to connect once
        if (m_notifySocketFd == -1) {
            if (m_notifySocketTriedToConnect)
                return false;
            m_notifySocketTriedToConnect = true;

            auto socketPath = m_notifySocket;

            if ((socketPath.at(0) != '@') && (socketPath.at(0) != '/'))
                throw Exception("invalid socket address: %1").arg(socketPath);

#if defined(Q_OS_LINUX)
            // QLocalSocket cannot send datagrams and systemd does not allow streams...
            union {
                struct ::sockaddr sa;
                struct ::sockaddr_un sun;
            } socketAddr;
            ::memset(&socketAddr, 0, sizeof(socketAddr));
            socketAddr.sun.sun_family = AF_UNIX;

            if (socketPath.size() >= qsizetype(sizeof(socketAddr.sun.sun_path)))
                throw Exception("socket path too long: %1").arg(socketPath);
            ::memcpy(socketAddr.sun.sun_path, socketPath.constData(), socketPath.size());
            if (socketPath.at(0) == u'@') // abstract socket
                socketAddr.sun.sun_path[0] = 0;

            int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
            if (fd < 0)
                throw Exception(errno, "cannot create DGRAM socket");

            if (::connect(fd, &socketAddr.sa, offsetof(struct sockaddr_un, sun_path) + socketPath.size()) != 0) {
                ::close(fd);
                throw Exception(errno, "cannot connect to socket at %1").arg(socketPath);
            }
            m_notifySocketFd = fd;
#else
            Q_ASSERT(false);
#endif
        }

        if (QT_WRITE(m_notifySocketFd, stateStr.constData(), stateStr.size()) != stateStr.size())
            throw Exception(errno, "failed to send notify string");

        return true;
    } catch (const Exception &e) {
        qCWarning(LogSystem).noquote() << "Systemd notify:" << e.errorString();
        return false;
    }
}

std::optional<std::chrono::milliseconds> Systemd::watchdogTimeout(bool ignorePid)
{
    if (m_watchdogUsec.isEmpty())
        return { };

    if (!ignorePid && !checkPid(m_watchdogPid))
        return { };

    auto msecs = std::chrono::milliseconds(m_watchdogUsec.toULongLong() / 1000);
    if (msecs <= 1ms) // this needs to be > 0, when divided by 2
        return { };
    return msecs;
}

QT_END_NAMESPACE_AM
