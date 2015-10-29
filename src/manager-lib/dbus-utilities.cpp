/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QDebug>

#include "applicationmanager.h"
#include "application.h"
#include "dbus-utilities.h"

#if defined(QT_DBUS_LIB)
#  if defined(AM_LIBDBUS_AVAILABLE)
#    include <dbus/dbus.h>
#    include <sys/socket.h>

Q_PID getDBusPeerPid(const QDBusConnection &conn)
{
    int socketFd = -1;
    if (dbus_connection_get_socket(static_cast<DBusConnection *>(conn.internalPointer()), &socketFd)) {
        struct ucred ucred;
        socklen_t ucredSize = sizeof(struct ucred);
        if (getsockopt(socketFd, SOL_SOCKET, SO_PEERCRED, &ucred, &ucredSize) == 0)
            return ucred.pid;
    }
    return Q_PID(-1);
}

#  else

Q_PID getDBusPeerPid(const QDBusConnection &conn)
{
    Q_UNUSED(conn)
    return Q_PID(-1);
}

#  endif
#endif

QMap<QByteArray, DBusPolicy> parseDBusPolicy(const QVariantMap &yamlFragment)
{
    QMap<QByteArray, DBusPolicy> result;

#if defined(QT_DBUS_LIB)
    foreach (const QString &f, yamlFragment.keys()) {
        QVariantMap policy = yamlFragment.value(f).toMap();
        DBusPolicy dbp;

        bool ok;
        foreach (const QVariant &v, policy.value("uids").toList()) {
            uint uid = v.toUInt(&ok);
            if (ok)
                dbp.m_uids << uid;
        }
        qSort(dbp.m_uids);
        dbp.m_executables = policy.value("executables").toStringList();
        dbp.m_executables.sort();
        dbp.m_capabilities = policy.value("capabilities").toStringList();
        dbp.m_capabilities.sort();

        result.insert(f.toLocal8Bit(), dbp);
    }
#else
    Q_UNUSED(yamlFragment)
#endif
    return result;
}


bool checkDBusPolicy(const QDBusContext *dbusContext, const QMap<QByteArray, DBusPolicy> &dbusPolicy, const QByteArray &function)
{
#if !defined(QT_DBUS_LIB) || defined(Q_OS_WIN)
    Q_UNUSED(dbusContext)
    Q_UNUSED(dbusPolicy)
    Q_UNUSED(function)
    return true;
#else
    if (!dbusContext->calledFromDBus())
        return true;

    auto ip = dbusPolicy.find(function);
    if (ip == dbusPolicy.cend())
        return true;

    try {
        uint pid = uint(-1);

        if (!ip->m_capabilities.isEmpty()) {
            pid = dbusContext->connection().interface()->servicePid(dbusContext->message().service());
            const Application *app = ApplicationManager::instance()->fromProcessId(pid);
            if (!app)
                throw "not an app with capabilities";
            QStringList appCaps = app->capabilities();
            bool match = false;
            foreach (const QString &cap, ip->m_capabilities) {
                if (qBinaryFind(appCaps, cap) != appCaps.cend())
                    match = true;
            }
            if (!match)
                throw "insufficient capabilities";
        }
        if (!ip->m_executables.isEmpty()) {
#  ifdef Q_OS_LINUX
            if (pid == uint(-1))
                pid = dbusContext->connection().interface()->servicePid(dbusContext->message().service());
            QString executable = QFileInfo("/proc/" + QByteArray::number(pid) + "/exe").symLinkTarget();
            if (executable.isEmpty())
                throw "cannot get executable";
            if (qBinaryFind(ip->m_executables, executable) == ip->m_executables.cend())
                throw "executable blocked";
#  else
            throw false;
#  endif
        }
        if (!ip->m_uids.isEmpty()) {
            uint uid = dbusContext->connection().interface()->serviceUid(dbusContext->message().service());
            if (qBinaryFind(ip->m_uids, uid) == ip->m_uids.cend())
                throw "uid blocked";
        }

        return true;

    } catch (const char *msg) {
        dbusContext->sendErrorReply(QDBusError::AccessDenied, QString::fromLatin1("Protected function call (%1)").arg(msg));
        return false;
    }
#endif
}
