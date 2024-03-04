// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DBUSPOLICY_H
#define DBUSPOLICY_H

#include <functional>

#include <QtAppManCommon/global.h>
#include <QtCore/QVariantMap>
#include <QtCore/QByteArray>

QT_FORWARD_DECLARE_CLASS(QDBusAbstractAdaptor)

QT_BEGIN_NAMESPACE_AM

class DBusPolicy
{
public:
    ~DBusPolicy();
    static DBusPolicy *instance();
    static DBusPolicy *createInstance(const std::function<QStringList(qint64)> &applicationIdsForPid,
                                      const std::function<QStringList(const QString &)> &capabilitiesForApplicationId);

    bool add(const QDBusAbstractAdaptor *dbusAdaptor, const QVariantMap &yamlFragment);
    bool check(const QDBusAbstractAdaptor *dbusAdaptor, const QByteArray &function);

private:
    Q_DISABLE_COPY_MOVE(DBusPolicy)
    DBusPolicy() = default;
    static DBusPolicy *s_instance;

    std::function<QStringList(qint64)> m_applicationIdsForPid;
    std::function<QStringList(const QString &)> m_capabilitiesForApplicationId;

    struct DBusPolicyEntry
    {
        QList<uint> m_uids;
        QStringList m_executables;
        QStringList m_capabilities;
    };
    QHash<const QDBusAbstractAdaptor *, QMap<QByteArray, DBusPolicyEntry>> m_policies;
};

QT_END_NAMESPACE_AM


#endif // DBUSPOLICY_H
