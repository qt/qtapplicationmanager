/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include "applicationmanager.h"
#include "application.h"
#include "dbus-policy.h"

AM_BEGIN_NAMESPACE

QMap<QByteArray, DBusPolicy> parseDBusPolicy(const QVariantMap &yamlFragment)
{
    QMap<QByteArray, DBusPolicy> result;

#if defined(QT_DBUS_LIB)
    for (auto it = yamlFragment.cbegin(); it != yamlFragment.cend(); ++it) {
        const QVariantMap &policy = it->toMap();
        DBusPolicy dbp;

        bool ok;
        const QVariantList uidList = policy.value(qSL("uids")).toList();
        foreach (const QVariant &v, uidList) {
            uint uid = v.toUInt(&ok);
            if (ok)
                dbp.m_uids << uid;
        }
        qSort(dbp.m_uids);
        dbp.m_executables = policy.value(qSL("executables")).toStringList();
        dbp.m_executables.sort();
        dbp.m_capabilities = policy.value(qSL("capabilities")).toStringList();
        dbp.m_capabilities.sort();

        result.insert(it.key().toLocal8Bit(), dbp);
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

AM_END_NAMESPACE
