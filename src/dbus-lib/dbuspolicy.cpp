/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QDebug>
#include <QFileInfo>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusContext>
#include <QDBusAbstractAdaptor>
#include <QMetaMethod>
#include <algorithm>

#include "utilities.h"
#include "dbuspolicy.h"
#include "applicationmanager.h"

QT_BEGIN_NAMESPACE_AM

struct DBusPolicyEntry
{
    QList<uint> m_uids;
    QStringList m_executables;
    QStringList m_capabilities;
};

static QHash<QDBusAbstractAdaptor *, QMap<QByteArray, DBusPolicyEntry> > policies;


bool DBusPolicy::add(QDBusAbstractAdaptor *dbusAdaptor, const QVariantMap &yamlFragment)
{
    QMap<QByteArray, DBusPolicyEntry> result;
    const QMetaObject *mo = dbusAdaptor->metaObject();

    for (auto it = yamlFragment.cbegin(); it != yamlFragment.cend(); ++it) {
        const QByteArray func = it.key().toLocal8Bit();

        bool found = false;
        for (int mi = mo->methodOffset(); mi < mo->methodCount(); ++mi) {
            if (mo->method(mi).name() == func) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;

        const QVariantMap &policy = it->toMap();
        DBusPolicyEntry dbp;

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

        result.insert(func, dbp);
    }

    policies.insert(dbusAdaptor, result);
    return true;
}


bool DBusPolicy::check(QDBusAbstractAdaptor *dbusAdaptor, const QByteArray &function)
{
#if !defined(Q_OS_UNIX)
    Q_UNUSED(dbusAdaptor)
    Q_UNUSED(function)
    return true;
#else
    if (!dbusAdaptor)
        return false;
    QObject *realObject = dbusAdaptor->parent();
    if (!realObject)
        return false;
    QDBusContext *dbusContext = reinterpret_cast<QDBusContext *>(realObject->qt_metacast("QDBusContext"));
    if (!dbusContext)
        return false;
    if (!dbusContext->calledFromDBus())
        return false;

    auto ia = policies.constFind(dbusAdaptor);
    if (ia == policies.cend())
        return false;

    auto ip = (*ia).find(function);
    if (ip == (*ia).cend())
        return true;

    try {
        uint pid = uint(-1);

        if (!ip->m_capabilities.isEmpty()) {
            pid = dbusContext->connection().interface()->servicePid(dbusContext->message().service());
            const auto apps = ApplicationManager::instance()->identifyAllApplications(pid);
            if (apps.size() > 1)
                throw "multiple apps per pid are not supported";
            const QString appId = !apps.isEmpty() ? apps.constFirst() : QString();
            QStringList appCaps = ApplicationManager::instance()->capabilities(appId);
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
#endif // !defined(Q_OS_UNIX)

}

QT_END_NAMESPACE_AM
