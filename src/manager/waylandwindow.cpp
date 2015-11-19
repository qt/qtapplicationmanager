/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#ifndef AM_SINGLEPROCESS_MODE

#include "waylandwindow.h"
#include "applicationmanager.h"
#include "application.h"
#include "global.h"

WaylandWindow::WaylandWindow(const Application *app, QWaylandSurfaceItem *surfaceItem)
    : Window(app, surfaceItem)
    , m_pingTimer(new QTimer(this))
    , m_pongTimer(new QTimer(this))
{
    if (surfaceItem && surfaceItem->surface()) {
        connect(surfaceItem->surface(), &QWaylandSurface::windowPropertyChanged,
                this, &Window::windowPropertyChanged);
        connect(surfaceItem->surface(), &QWaylandSurface::pong,
                this, &WaylandWindow::pongReceived);

        m_pingTimer->setInterval(1000);
        m_pingTimer->setSingleShot(true);
        connect(m_pingTimer, &QTimer::timeout, this, &WaylandWindow::pingTimeout);
        m_pongTimer->setInterval(2000);
        m_pongTimer->setSingleShot(true);
        connect(m_pongTimer, &QTimer::timeout, this, &WaylandWindow::pongTimeout);
    }
}

QWaylandSurface *WaylandWindow::surface() const
{
    if (QWaylandSurfaceItem *item = qobject_cast<QWaylandSurfaceItem *>(surfaceItem()))
        return item->surface();
    return 0;
}

bool WaylandWindow::isPingEnabled() const
{
    return m_pingTimer->isActive() || m_pongTimer->isActive();
}

void WaylandWindow::enablePing(bool b)
{
    m_pingTimer->stop();
    m_pongTimer->stop();

    if (b)
        pingTimeout();
}

void WaylandWindow::pongReceived()
{
    m_pongTimer->stop();
    m_pingTimer->start();
}

void WaylandWindow::pongTimeout()
{
    if (!application())
        return;

    qCCritical(LogWayland) << "Stopping application" << application()->id() << "because we did not receive a Wayland-Pong for" << m_pongTimer->interval() << "msec";
    ApplicationManager::instance()->stopApplication(application(), true);
}

void WaylandWindow::pingTimeout()
{
    m_pingTimer->stop();
    m_pongTimer->start();
    if (QWaylandSurface *wls = surface()) {
        if (wls->handle() && wls->client())
            wls->ping();
    }
}

bool WaylandWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    if (QWaylandSurface *wls = surface()) {
        QVariant oldValue = wls->windowProperties().value(name);

        if (oldValue != value) {
            wls->setWindowProperty(name, value);
            emit windowPropertyChanged(name, value);
        }
        return true;
    }
    return false;
}

QVariant WaylandWindow::windowProperty(const QString &name) const
{
    if (const QWaylandSurface *wls = surface()) {
        return wls->windowProperties().value(name);
    }
    return QVariant();
}

QVariantMap WaylandWindow::windowProperties() const
{
    if (const QWaylandSurface *wls = surface()) {
        return wls->windowProperties();
    }
    return QVariantMap();
}

#endif // AM_SINGLEPROCESS_MODE
