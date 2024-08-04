// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QWaylandClient>
#include <QWaylandSurface>
#include <QWaylandXdgShell>

#include "application.h"
#include "applicationmanager.h"
#include "logging.h"
#include "utilities.h"
#include "waylandxdgwatchdog.h"

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

QT_BEGIN_NAMESPACE_AM

WaylandXdgWatchdog::WaylandXdgWatchdog(QWaylandXdgShell *xdgShell)
    : QObject(xdgShell)
    , m_xdgShell(xdgShell)
{
    m_pingTimer.setSingleShot(true);
    m_pongWarnTimer.setSingleShot(true);
    m_pongKillTimer.setSingleShot(true);

    // we don't get notified when clients are connecting, so we have to watch for new surfaces
    connect(m_xdgShell, &QWaylandXdgShell::xdgSurfaceCreated,
            this, [this](QWaylandXdgSurface *surface) {
        auto *client = (surface && surface->surface()) ? surface->surface()->client() : nullptr;
        watchClient(client);
    });

    // send out pings
    connect(&m_pingTimer, &QTimer::timeout,
            this, &WaylandXdgWatchdog::pingClients);

    // handle missing pong replies
    connect(&m_pongWarnTimer, &QTimer::timeout,
            this, &WaylandXdgWatchdog::onPongWarnTimeout);
    connect(&m_pongKillTimer, &QTimer::timeout,
            this, &WaylandXdgWatchdog::onPongKillTimeout);

    // receive back pongs
    connect(m_xdgShell, &QWaylandXdgShell::pong,
            this, &WaylandXdgWatchdog::onPongReceived);
}

bool WaylandXdgWatchdog::isClientWatchingEnabled() const
{
    if (m_checkInterval <= 0ms)
        return false;
    else
        return (m_warnTimeout > 0ms) || (m_killTimeout > 0ms);
}

void WaylandXdgWatchdog::setTimeouts(std::chrono::milliseconds checkInterval,
                                     std::chrono::milliseconds warnTimeout,
                                     std::chrono::milliseconds killTimeout)
{
    m_checkInterval = std::max(0ms, checkInterval);
    m_pingTimer.setInterval(m_checkInterval);
    if (m_checkInterval > 0ms) {
        m_pingTimer.start();
    } else {
        m_pingTimer.stop();
        m_pongWarnTimer.stop();
        m_pongKillTimer.stop();
    }

    // these will be picked up automatically
    m_warnTimeout = std::max(0ms, warnTimeout);
    m_killTimeout = std::max(0ms, killTimeout);

    if (m_warnTimeout == m_killTimeout)
        m_warnTimeout = 0ms;

    if (m_warnTimeout > m_killTimeout) {
        qCWarning(LogWatchdog).nospace()
            << "Wayland client warning timeout (" << m_warnTimeout
            << ") is greater than kill timeout (" << m_killTimeout << ")";
    }
}

void WaylandXdgWatchdog::watchClient(QWaylandClient *client)
{
    auto updateClientData = [](ClientData *cd) {
        auto pid = cd->m_client->processId();
        cd->m_apps = ApplicationManager::instance()->fromProcessId(pid);

        QString desc = u"Wayland client "_s;
        if (!cd->m_apps.isEmpty()) {
            bool first = true;
            for (const auto *app : std::as_const(cd->m_apps)) {
                if (first)
                    first = false;
                else
                    desc.append(u',');
                desc.append(app->id());
            }
            desc.append(u" / ");
        }
        desc = desc + u"pid: " + QString::number(pid);
        cd->m_description = desc;
    };

    if (!client)
        return;

    if (!isClientWatchingEnabled())
        return;

    for (auto *cd : std::as_const(m_clients)) {
        if (cd->m_client == client) {
            updateClientData(cd);
            return;
        }
    }

    auto *cd = new ClientData;
    cd->m_client = client;
    updateClientData(cd);
    m_clients << cd;

    if (!m_pingTimer.isActive())
        m_pingTimer.start();

    qCInfo(LogWatchdog).nospace().noquote()
        << cd->m_description << " is being watched now (check every " << m_checkInterval
        << ", warn/kill after " << m_warnTimeout << "/" << m_killTimeout << ")";

    connect(client, &QObject::destroyed, this, [this, cd](QObject *) {
        qCInfo(LogWatchdog).noquote()
            << cd->m_description << "has disconnected and is not being watched anymore";
        m_clients.removeOne(cd);
        delete cd;

        if (m_clients.isEmpty() && m_pingTimer.isActive())
            m_pingTimer.stop();
    });
}

void WaylandXdgWatchdog::pingClients()
{
    Q_ASSERT(!m_pongWarnTimer.isActive());
    Q_ASSERT(!m_pongKillTimer.isActive());

    if (!m_xdgShell)
        return;

    if (m_clients.isEmpty())
        return;

    if (m_warnTimeout <= 0ms && m_killTimeout <= 0ms)
        return;

    for (auto *cd : std::as_const(m_clients))
        cd->m_pingSerial = m_xdgShell->ping(cd->m_client);

    m_lastPing.start();

    m_pongWarnTimer.setInterval(m_warnTimeout);
    m_pongKillTimer.setInterval(m_killTimeout);

    if (m_warnTimeout > 0ms)
        m_pongWarnTimer.start();
    if (m_killTimeout > 0ms)
        m_pongKillTimer.start();
}

void WaylandXdgWatchdog::onPongWarnTimeout()
{
    Q_ASSERT(!m_pingTimer.isActive());
    auto elapsed = std::chrono::milliseconds(m_lastPing.elapsed());

    for (auto *cd : std::as_const(m_clients)) {
        if (cd->m_pingSerial) {
            qCCritical(LogWatchdog).nospace().noquote()
                << cd->m_description
                << " still hasn't sent a pong reply to a ping request in over "
                << elapsed << " (the warn threshold is" << m_warnTimeout << ")";
        }
    }
    if ((m_killTimeout <= 0ms) && (m_checkInterval > 0ms))
        m_pingTimer.start();
}

void WaylandXdgWatchdog::onPongKillTimeout()
{
    Q_ASSERT(!m_pingTimer.isActive());

    for (auto *cd : std::as_const(m_clients)) {
        if (cd->m_pingSerial) {
            cd->m_pingSerial = 0;

            qCCritical(LogWatchdog).nospace().noquote()
                << cd->m_description
                << " is getting killed, because it failed to send a pong reply to a ping request within "
                << m_killTimeout;

            if (cd->m_apps.isEmpty()) {
                cd->m_client->kill(SIGKILL);
            } else {
                for (auto *app : std::as_const(cd->m_apps))
                    ApplicationManager::instance()->stopApplicationInternal(app, true);
            }
        }
    }
    if (m_checkInterval > 0ms)
        m_pingTimer.start();
}

void WaylandXdgWatchdog::onPongReceived(uint serial)
{
    auto elapsed = std::chrono::milliseconds(m_lastPing.elapsed());

    ClientData *foundCd = nullptr;
    int missingPongs = 0;
    for (auto *cd : std::as_const(m_clients)) {
        if (cd->m_pingSerial == serial) {
            cd->m_pingSerial = 0;
            foundCd = cd;
        } else if (cd->m_pingSerial) {
            ++missingPongs;
        }
    }

    // unknown pong
    if (!foundCd)
        return;

    if ((m_warnTimeout > 0ms) && (elapsed > m_warnTimeout)) {
        qCWarning(LogWatchdog).nospace().noquote()
            << foundCd->m_description << " needed " << elapsed
            << " to send a pong reply to a ping request (the warn threshold is "
            << m_warnTimeout << ")";
    }

    // not really expected at this point anymore
    if (!m_pongWarnTimer.isActive() && !m_pongKillTimer.isActive())
        return;

    if (!missingPongs) {
        m_pongWarnTimer.stop();
        m_pongKillTimer.stop();
        m_pingTimer.start();
    }
}

QT_END_NAMESPACE_AM

#include "moc_waylandxdgwatchdog.cpp"
