// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WAYLANDXDGWATCHDOG_H
#define WAYLANDXDGWATCHDOG_H

#include <chrono>

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtAppManSystemUI/qtappmansystemuiglobal.h>

QT_FORWARD_DECLARE_CLASS(QWaylandClient)
QT_FORWARD_DECLARE_CLASS(QWaylandCompositor)
QT_FORWARD_DECLARE_CLASS(QWaylandXdgShell)

QT_BEGIN_NAMESPACE_AM

class AbstractRuntime;

class Q_APPMANSYSTEMUI_EXPORT WaylandXdgWatchdog : public QObject
{
    Q_OBJECT
public:
    explicit WaylandXdgWatchdog(QWaylandXdgShell *xdgShell);

    void setTimeouts(std::chrono::milliseconds checkInterval, std::chrono::milliseconds warnTimeout,
                     std::chrono::milliseconds killTimeout);

private:
    bool isClientWatchingEnabled() const;
    void watchClient(QWaylandClient *client);
    void pingClients();

    QPointer<QWaylandXdgShell> m_xdgShell;
    std::chrono::milliseconds m_checkInterval { };
    std::chrono::milliseconds m_warnTimeout { };
    std::chrono::milliseconds m_killTimeout { };
    QTimer m_pingTimer;

    struct ClientData {
        QWaylandClient *m_client = nullptr;
        qint64 m_pid = 0;
        uint m_pingSerial = 0;
        QString m_description;
        QList<AbstractRuntime *> m_runtimes;
        bool m_hasDebugWrapper = false;
        QTimer *m_pongWarnTimer = nullptr;
        QTimer *m_pongKillTimer = nullptr;
        QElapsedTimer m_lastPing;
    };
    void onPongWarnTimeout(ClientData *cd);
    void onPongKillTimeout(ClientData *cd);
    void onPongReceived(uint serial);

    QList<ClientData *> m_clients;
};

QT_END_NAMESPACE_AM

#endif // WAYLANDXDGWATCHDOG_H
