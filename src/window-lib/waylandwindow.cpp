/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#if defined(AM_MULTI_PROCESS)

#include "global.h"
#include "applicationmanager.h"
#include "application.h"
#include "windowmanager.h"
#include "waylandwindow.h"
#include "waylandcompositor.h"

QT_BEGIN_NAMESPACE_AM

WaylandWindow::WaylandWindow(const Application *app, WindowSurface *surf)
    : Window(app, surf->item())
    , m_pingTimer(new QTimer(this))
    , m_pongTimer(new QTimer(this))
    , m_surface(surf)
{
    if (surf && surf->item()) {
        connect(surf, &WindowSurface::pong,
                this, &WaylandWindow::pongReceived);
        connect(surf, &WindowSurface::windowPropertyChanged,
                this, &WaylandWindow::windowPropertyChanged);

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

void WaylandWindow::setClosing()
{
    Window::setClosing();
    m_surface = nullptr;
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
    if (m_surface)
        m_surface->ping();
}

bool WaylandWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    if (m_surface) {
        QVariant oldValue = m_surface->windowProperties().value(name);

        if (oldValue != value)
            m_surface->setWindowProperty(name, value);
        return true;
    }
    return false;
}

QVariant WaylandWindow::windowProperty(const QString &name) const
{
    if (m_surface)
        return m_surface->windowProperties().value(name);
    return QVariant();
}

QVariantMap WaylandWindow::windowProperties() const
{
    if (m_surface)
        return m_surface->windowProperties();
    return QVariantMap();
}

QT_END_NAMESPACE_AM

#endif // AM_MULTI_PROCESS
