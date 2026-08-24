// Copyright (C) 2025 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef DBUS_H
#define DBUS_H

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/exception.h>

#include "applicationmanager_interface_p.h"
#include "packagemanager_interface_p.h"

QT_BEGIN_NAMESPACE_AM

class DBus : public QObject
{
    Q_OBJECT

public:
    DBus();

    void setInstanceInfo(const QVariantMap &instanceInfo);
    void connectToManager() noexcept(false);
    void connectToPackager() noexcept(false);

    void throwOnError();

    Q_SIGNAL void disconnected(QString reason);

private:
    QDBusConnection connectTo(const QString &iface) noexcept(false);

    void installDisconnectWatcher(const QDBusConnection &conn, const QString &serviceName);
    void disconnectDetected(const QString &reason);

public:
    IoQtPackageManagerInterface *packager() const;
    IoQtApplicationManagerInterface *manager() const;

private:
    IoQtPackageManagerInterface *m_packager = nullptr;
    IoQtApplicationManagerInterface *m_manager = nullptr;
    QVariantMap m_dbusAddresses;
    QString m_dbusName;
    QString m_dbusService;
    QStringList m_connections;
    QTimer *m_disconnectTimer = nullptr;
    bool m_disconnectedEmitted = false;
};

QT_END_NAMESPACE_AM

#endif // DBUS_H
