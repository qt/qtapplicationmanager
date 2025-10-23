// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
#include "exception.h"
#include "dbuspolicy.h"
#include "dbuscontextadaptor.h"

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM

DBusPolicy *DBusPolicy::s_instance = nullptr;

DBusPolicy::~DBusPolicy()
{
    Q_ASSERT(s_instance == this);
    s_instance = nullptr;
}

DBusPolicy *DBusPolicy::instance()
{
    return s_instance;
}

// This uses function pointers to avoid strict coupling between AM modules:
// * applicationIdsForPid normally maps to ApplicationManager::identifyAllApplications
// * capabilitiesForApplicationId normally maps to ApplicationManager::capabilities

DBusPolicy *DBusPolicy::createInstance(const std::function<QStringList (qint64)> &applicationIdsForPid,
                                       const std::function<QStringList (const QString &)> &capabilitiesForApplicationId)
{
    Q_ASSERT(!s_instance);
    s_instance = new DBusPolicy();
    s_instance->m_applicationIdsForPid = applicationIdsForPid;
    s_instance->m_capabilitiesForApplicationId = capabilitiesForApplicationId;
    return s_instance;
}

bool DBusPolicy::add(const QDBusAbstractAdaptor *dbusAdaptor, const QVariantMap &yamlFragment)
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
        const QVariantList uidList = policy.value(u"uids"_s).toList();
        for (const QVariant &v : uidList) {
            uint uid = v.toUInt(&ok);
            if (ok)
                dbp.m_uids << uid;
        }
        std::sort(dbp.m_uids.begin(), dbp.m_uids.end());
        dbp.m_executables = variantToStringList(policy.value(u"executables"_s));
        dbp.m_executables.sort();
        dbp.m_capabilities = variantToStringList(policy.value(u"capabilities"_s));
        dbp.m_capabilities.sort();

        result.insert(func, dbp);
    }

    m_policies.insert(dbusAdaptor, result);
    return true;
}


void DBusPolicy::check(const QDBusAbstractAdaptor *dbusAdaptor, const QByteArray &function)
{
#if !defined(Q_OS_UNIX)
    Q_UNUSED(dbusAdaptor)
    Q_UNUSED(function)
#else
    Q_ASSERT(dbusAdaptor);
    QDBusContext *dbusContext = dbusAdaptor ? qobject_cast<DBusContextAdaptor *>(dbusAdaptor->parent())
                                            : nullptr;
    Q_ASSERT(dbusContext);

    if (!dbusAdaptor || !dbusContext || !dbusContext->calledFromDBus())
        throw Exception("cannot evalutate policy without a valid D-Bus context");

    auto ia = m_policies.constFind(dbusAdaptor);
    if (ia == m_policies.cend())
        return; // no policy for interface

    auto ip = (*ia).find(function);
    if (ip == (*ia).cend())
        return; // no policy for the function

    uint cachedCallerPid = 0;
    auto callerPid = [&] {
        if (!cachedCallerPid)
            cachedCallerPid = dbusContext->connection().interface()->servicePid(dbusContext->message().service());
        return cachedCallerPid;
    };

    int checksDone = 0; // the default is 'deny', so we need to keep track of 'allow' rules

    if (!ip->m_capabilities.isEmpty()) {
        Q_ASSERT(m_capabilitiesForApplicationId);
        Q_ASSERT(m_applicationIdsForPid);

        if (!m_capabilitiesForApplicationId || !m_applicationIdsForPid)
            throw Exception("cannot evaluate capabilities policy without application manager integration");

        const QStringList apps = m_applicationIdsForPid(callerPid());
        if (apps.size() > 1)
            throw Exception("multiple apps per pid (%1) are not supported").arg(callerPid());
        const QString appId = !apps.isEmpty() ? apps.constFirst() : QString();
        const QStringList appCaps = m_capabilitiesForApplicationId(appId);
        bool match = false;
        for (const QString &cap : ip->m_capabilities)
            match = match && std::binary_search(appCaps.cbegin(), appCaps.cend(), cap);
        if (!match)
            throw Exception("application '%1' has insufficient capabilities").arg(appId);
        ++checksDone;
    }
    if (!ip->m_executables.isEmpty()) {
#  if defined(Q_OS_LINUX)
        QString executable = QFileInfo(u"/proc/"_s + QString::number(callerPid()) + u"/exe"_s).symLinkTarget();
        if (executable.isEmpty())
            throw Exception("cannot get executable for pid '%1'").arg(callerPid());
        if (std::binary_search(ip->m_executables.cbegin(), ip->m_executables.cend(), executable))
            throw Exception("executable '%1' is denied").arg(executable);
        ++checksDone;
#  else
        throw Exception("the executables policy is not supported on this platform");
#  endif // defined(Q_OS_LINUX)
    }
    if (!ip->m_uids.isEmpty()) {
        uint uid = dbusContext->connection().interface()->serviceUid(dbusContext->message().service());
        if (std::binary_search(ip->m_uids.cbegin(), ip->m_uids.cend(), uid))
            throw Exception("uid '%1' is denied").arg(uid);
        ++checksDone;
    }
    if (!checksDone)
        throw Exception("denied");
#endif // !defined(Q_OS_UNIX)
}

QT_END_NAMESPACE_AM
