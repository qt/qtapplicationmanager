/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#include <QDebug>
#include <QFileInfo>
#include <algorithm>
#if defined(QT_DBUS_LIB)
#  include <QDBusConnection>
#  include <QDBusConnectionInterface>
#  include <QDBusMessage>
#endif

#include "utilities.h"
#include "dbus-policy.h"

QT_BEGIN_NAMESPACE_AM

QMap<QByteArray, DBusPolicy> parseDBusPolicy(const QVariantMap &yamlFragment)
{
    QMap<QByteArray, DBusPolicy> result;

#if defined(QT_DBUS_LIB)
    for (auto it = yamlFragment.cbegin(); it != yamlFragment.cend(); ++it) {
        const QVariantMap &policy = it->toMap();
        DBusPolicy dbp;

        bool ok;
        const QVariantList uidList = policy.value(qSL("uids")).toList();
        for (const QVariant &v : uidList) {
            uint uid = v.toUInt(&ok);
            if (ok)
                dbp.m_uids << uid;
        }
        std::sort(dbp.m_uids.begin(), dbp.m_uids.end());
        dbp.m_executables = variantToStringList(policy.value(qSL("executables")));
        dbp.m_executables.sort();
        dbp.m_capabilities = variantToStringList(policy.value(qSL("capabilities")));
        dbp.m_capabilities.sort();

        result.insert(it.key().toLocal8Bit(), dbp);
    }
#else
    Q_UNUSED(yamlFragment)
#endif
    return result;
}


bool checkDBusPolicy(const QDBusContext *dbusContext, const QMap<QByteArray, DBusPolicy> &dbusPolicy,
                     const QByteArray &function, const std::function<QStringList(qint64)> &pidToCapabilities)
{
#if !defined(QT_DBUS_LIB) || !defined(Q_OS_UNIX)
    Q_UNUSED(dbusContext)
    Q_UNUSED(dbusPolicy)
    Q_UNUSED(function)
    Q_UNUSED(pidToCapabilities)
    return true;
#else
    if (!dbusContext->calledFromDBus())
        return true;

    auto ip = dbusPolicy.find(function);
    if (ip == dbusPolicy.cend())
        return true;

    try {
        uint pid = uint(-1);

        if (!ip->m_capabilities.isEmpty() && pidToCapabilities) {
            pid = dbusContext->connection().interface()->servicePid(dbusContext->message().service());
            QStringList appCaps = pidToCapabilities(pid);
            bool match = false;
            for (const QString &cap : ip->m_capabilities)
                match = match && std::binary_search(appCaps.cbegin(), appCaps.cend(), cap);
            if (!match)
                throw "insufficient capabilities";
        }
        if (!ip->m_executables.isEmpty()) {
#  if defined(Q_OS_LINUX)
            if (pid == uint(-1))
                pid = dbusContext->connection().interface()->servicePid(dbusContext->message().service());
            QString executable = QFileInfo(qSL("/proc/") + QString::number(pid) + qSL("/exe")).symLinkTarget();
            if (executable.isEmpty())
                throw "cannot get executable";
            if (std::binary_search(ip->m_executables.cbegin(), ip->m_executables.cend(), executable))
                throw "executable blocked";
#  else
            throw false;
#  endif // defined(Q_OS_LINUX)
        }
        if (!ip->m_uids.isEmpty()) {
            uint uid = dbusContext->connection().interface()->serviceUid(dbusContext->message().service());
            if (std::binary_search(ip->m_uids.cbegin(), ip->m_uids.cend(), uid))
                throw "uid blocked";
        }

        return true;

    } catch (const char *msg) {
        dbusContext->sendErrorReply(QDBusError::AccessDenied, QString::fromLatin1("Protected function call (%1)").arg(qL1S(msg)));
        return false;
    }
#endif // !defined(QT_DBUS_LIB) || !defined(Q_OS_UNIX)
}

QT_END_NAMESPACE_AM
