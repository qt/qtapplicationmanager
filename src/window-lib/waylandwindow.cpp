/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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
#include "logging.h"
#include "applicationmanager.h"
#include "application.h"
#include "windowmanager.h"
#include "waylandwindow.h"
#include "waylandcompositor.h"
#include "waylandqtamserverextension_p.h"

#include <QWaylandWlShellSurface>

QT_BEGIN_NAMESPACE_AM

bool WaylandWindow::m_watchdogEnabled = true;

WaylandWindow::WaylandWindow(AbstractApplication *app, WindowSurface *surf)
    : Window(app)
    , m_pingTimer(new QTimer(this))
    , m_pongTimer(new QTimer(this))
    , m_surface(surf)
{
    if (surf) {
        connect(surf, &WindowSurface::pong,
                this, &WaylandWindow::pongReceived);
        connect(m_surface, &WindowSurface::redraw,
                this, &WaylandWindow::frameUpdated);
        connect(m_surface, &QWaylandSurface::hasContentChanged, this, &WaylandWindow::onContentStateChanged);
        connect(m_surface, &QWaylandSurface::sizeChanged, this, &Window::sizeChanged);

        m_pingTimer->setInterval(1000);
        m_pingTimer->setSingleShot(true);
        connect(m_pingTimer, &QTimer::timeout, this, &WaylandWindow::pingTimeout);
        m_pongTimer->setInterval(2000);
        m_pongTimer->setSingleShot(true);
        connect(m_pongTimer, &QTimer::timeout, this, &WaylandWindow::pongTimeout);

        connect(surf->compositor()->amExtension(), &WaylandQtAMServerExtension::windowPropertyChanged,
                this, [this](QWaylandSurface *surface, const QString &name, const QVariant &value) {
            if (surface == m_surface)
                emit windowPropertyChanged(name, value);
        });

        connect(surf, &QWaylandSurface::surfaceDestroyed, this, [this]() {
            m_surface = nullptr;
            onContentStateChanged();
            emit waylandSurfaceChanged();
        });

        enableOrDisablePing();
    }
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

    qCCritical(LogGraphics) << "Stopping application" << application()->id() << "because we did not receive a Wayland-Pong for" << m_pongTimer->interval() << "msec";
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
    if (m_surface)
        m_surface->compositor()->amExtension()->setWindowProperty(m_surface, name, value);
    return (m_surface);
}

QVariant WaylandWindow::windowProperty(const QString &name) const
{
    return windowProperties().value(name);
}

QVariantMap WaylandWindow::windowProperties() const
{
    return m_surface ? m_surface->compositor()->amExtension()->windowProperties(m_surface) : QVariantMap();
}

auto WaylandWindow::contentState() const -> ContentState
{
    if (m_surface)
        return m_surface->hasContent() ? SurfaceWithContent : SurfaceNoContent;
    else
        return NoSurface;
}

void WaylandWindow::enableOrDisablePing()
{
    if (m_watchdogEnabled) {
        m_pingTimer->stop();
        m_pongTimer->stop();

        if (m_surface && m_surface->hasContent())
            pingTimeout();
    }
}

void WaylandWindow::onContentStateChanged()
{
    qCDebug(LogGraphics) << this << "of" << application()->id() << "contentState changed to" << contentState();

    enableOrDisablePing();
    emit contentStateChanged();
}

QSize WaylandWindow::size() const
{
    return m_surface->size();
}

void WaylandWindow::resize(const QSize &newSize)
{
    if (!m_surface)
        return;

    AbstractApplication *app = nullptr; // prevent expensive lookup when not printing qDebugs

    qCDebug(LogGraphics) << "Sending geometry change request to Wayland client for surface"
        << m_surface << "new:" << newSize << "of"
        << ((app = ApplicationManager::instance()->fromProcessId(m_surface->client()->processId()))
                ? app->id() : QString::fromLatin1("pid: %1").arg(m_surface->client()->processId()));

    m_surface->sendResizing(newSize);
}

QWaylandQuickSurface* WaylandWindow::waylandSurface() const
{
    return m_surface;
}

void WaylandWindow::close()
{
    if (m_surface)
        m_surface->close();
}

QT_END_NAMESPACE_AM

#endif // AM_MULTI_PROCESS
