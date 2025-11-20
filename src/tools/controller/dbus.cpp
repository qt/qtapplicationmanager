// Copyright (C) 2025 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtAppManCommon/utilities.h>

#include "dbus.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

template<typename T>
static T *tryConnectToDBusInterface(const QString &service, const QString &path,
                                    const QString &connectionName, QObject *parent)
{
    // we are working with very small delays in the milli-second range here, so a linear factor
    // to support valgrind would have to be very large and probably conflict with usage elsewhere
    // in the codebase, where the ranges are normally in the seconds.
    static const int timeout = timeoutFactor() * timeoutFactor();

    QDBusConnection conn(connectionName);

    if (!conn.isConnected())
        return nullptr;
    if (!service.isEmpty() && conn.interface()) {
        // the 'T' constructor can block up to 25sec (!), if the service is not registered!
        if (!conn.interface()->isServiceRegistered(service))
            return nullptr;
    }

    QElapsedTimer timer;
    timer.start();

    do {
        T *iface = new T(service, path, conn, parent);
        if (!iface->lastError().isValid())
            return iface;
        delete iface;
        QThread::msleep(static_cast<unsigned long>(timeout));
    } while (timer.elapsed() < (100 * timeout)); // 100msec base line

    return nullptr;
}


DBus::DBus()
{
    registerDBusTypes();
}

void DBus::setInstanceInfo(const QVariantMap &instanceInfo)
{
    m_dbusAddresses = instanceInfo[u"dbus"_s].toMap();
}

void DBus::connectToManager() noexcept(false)
{
    if (m_manager)
        return;

    auto conn = connectTo(u"io.qt.ApplicationManager"_s);
    m_manager = tryConnectToDBusInterface<IoQtApplicationManagerInterface>(m_dbusService,
                                                                           u"/ApplicationManager"_s,
                                                                           conn.name(), this);
    if (!m_manager) {
        throw Exception("Could not connect to the io.qt.ApplicationManager D-Bus interface on %1")
            .arg(m_dbusName);
    }
}

void DBus::connectToPackager() noexcept(false)
{
    if (m_packager)
        return;

    auto conn = connectTo(u"io.qt.PackageManager"_s);
    m_packager = tryConnectToDBusInterface<IoQtPackageManagerInterface>(m_dbusService,
                                                                        u"/PackageManager"_s,
                                                                        conn.name(), this);
    if (!m_packager) {
        throw Exception("Could not connect to the io.qt.PackageManager D-Bus interface on %1")
            .arg(m_dbusName);
    }
}

QDBusConnection DBus::connectTo(const QString &iface) noexcept(false)
{
    QDBusConnection conn(iface);

    QString dbus = m_dbusAddresses.value(iface).toString();
    if (dbus.isEmpty()) {
        throw Exception("This application manager instance does not expose the D-Bus interface "
                        "%1.\nDid you forget to enable development mode?").arg(iface);
    }

    if (dbus == u"system") {
        conn = QDBusConnection::systemBus();
        m_dbusName = u"[system-bus]"_s;
    } else if (dbus == u"session") {
        conn = QDBusConnection::sessionBus();
        m_dbusName = u"[session-bus]"_s;
    } else if (dbus.startsWith(u"p2p:")) {
        const auto address = dbus.mid(4);
        conn = QDBusConnection::connectToPeer(address, u"p2p"_s);
        m_dbusName = u"[p2p] "_s + address;
    } else {
        conn = QDBusConnection::connectToBus(dbus, u"custom_%1"_s.arg(iface));
        m_dbusName = dbus;
    }

    if (conn.name() != u"p2p") // no service names allowed on p2p busses
        m_dbusService = u"io.qt.ApplicationManager"_s;

    if (!conn.isConnected()) {
        throw Exception(Error::IO, "Could not connect to the application manager D-Bus interface %1 at %2: %3")
            .arg(iface, m_dbusName, conn.lastError().message());
    }

    installDisconnectWatcher(conn, u"io.qt.ApplicationManager"_s);
    return conn;
}

void DBus::installDisconnectWatcher(const QDBusConnection &conn, const QString &serviceName)
{
    if (m_disconnectedEmitted)
        return;

    if (!m_connections.contains(conn.name())) {
        auto *watcher = new QDBusServiceWatcher(serviceName, conn, QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged,
                this, [this](const QString &, const QString &, const QString &) {
            disconnectDetected(u"owner changed"_s);
        });
        m_connections.append(conn.name());
    }

    // serviceOwnerChanged does not work if the bus-daemon process dies (as is the case when
    // the AM starts its own session bus in --dbus=auto mode and then later crashes, killing
    // the bus-daemon with it).
    // QDBusConnection::isConnected() does not have a change signal, so we have to poll.
    if (!m_disconnectTimer) {
        m_disconnectTimer = new QTimer(this);
        connect(m_disconnectTimer, &QTimer::timeout, this, [this]() {
            for (const auto &name : std::as_const(m_connections)) {
                if (!QDBusConnection(name).isConnected()) {
                    disconnectDetected(u"bus died"_s);
                    break;
                }
            }
        });
        m_disconnectTimer->start(500ms);
    }
}

void DBus::disconnectDetected(const QString &reason)
{
    if (!m_disconnectedEmitted) {
        emit disconnected(reason);
        m_disconnectedEmitted = true;
        if (m_disconnectTimer)
            m_disconnectTimer->stop();
    }
}

IoQtPackageManagerInterface *DBus::packager() const
{
    return m_packager;
}

IoQtApplicationManagerInterface *DBus::manager() const
{
    return m_manager;
}

void QtAM::DBus::throwOnError()
{
    for (auto *iface : std::initializer_list<const QDBusAbstractInterface *>{ m_manager, m_packager }) {
        if (iface && iface->lastError().isValid())
            throw Exception("D-Bus error on %1: %2").arg(iface->interface(), iface->lastError().message());
    }
}

QT_END_NAMESPACE_AM
