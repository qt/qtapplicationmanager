/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#ifndef AM_SINGLE_PROCESS_MODE

#include "waylandwindow.h"
#include "applicationmanager.h"
#include "application.h"
#include "global.h"
#include "windowmanager.h"

WaylandWindow::WaylandWindow(const Application *app, WindowSurface *surf)
    : Window(app, surf->item())
    , m_pingTimer(new QTimer(this))
    , m_pongTimer(new QTimer(this))
    , m_surface(surf)
{
    if (surf && surf->item()) {
        surf->connectPong([this]() { pongReceived(); });
        surf->connectWindowPropertyChanged([this](const QString &n, const QVariant &v) { emit windowPropertyChanged(n, v); });

        m_pingTimer->setInterval(1000);
        m_pingTimer->setSingleShot(true);
        connect(m_pingTimer, &QTimer::timeout, this, &WaylandWindow::pingTimeout);
        m_pongTimer->setInterval(2000);
        m_pongTimer->setSingleShot(true);
        connect(m_pongTimer, &QTimer::timeout, this, &WaylandWindow::pongTimeout);
    }
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
    if (m_surface) {
        m_surface->ping();
    }
}

bool WaylandWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    if (m_surface) {
        QVariant oldValue = m_surface->windowProperties().value(name);

        if (oldValue != value) {
            m_surface->setWindowProperty(name, value);
            emit windowPropertyChanged(name, value);
        }
        return true;
    }
    return false;
}

QVariant WaylandWindow::windowProperty(const QString &name) const
{
    if (m_surface) {
        return m_surface->windowProperties().value(name);
    }
    return QVariant();
}

QVariantMap WaylandWindow::windowProperties() const
{
    if (m_surface) {
        return m_surface->windowProperties();
    }
    return QVariantMap();
}

#endif // AM_SINGLE_PROCESS_MODE
