/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
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

#include <QQuickView>
#include <QWaylandOutput>
#include <QWaylandSeat>
#include <QWaylandKeymap>

#include "global.h"
#include "logging.h"
#include "windowmanager.h"
#include "application.h"
#include "applicationmanager.h"
#include "waylandcompositor.h"

#include <QWaylandWlShell>
#include <QWaylandXdgShellV5>
#include <QWaylandQuickOutput>
#include <QWaylandTextInputManager>
#include <QWaylandQtWindowManager>
#include "waylandqtamserverextension_p.h"

QT_BEGIN_NAMESPACE_AM

WindowSurface::WindowSurface(WaylandCompositor *comp, QWaylandClient *client, uint id, int version)
    : QWaylandQuickSurface(comp, client, id, version)
    , m_surface(this)
    , m_compositor(comp)
{ }

void WindowSurface::setShellSurface(QWaylandWlShellSurface *shellSurface)
{
    Q_ASSERT(!m_xdgSurface);
    m_wlSurface = shellSurface;
    connect(m_wlSurface, &QWaylandWlShellSurface::pong,
            this, &WindowSurface::pong);
}

void WindowSurface::sendResizing(const QSize &size)
{
    if (m_xdgSurface)
        m_xdgSurface->sendResizing(size);
    else
        m_wlSurface->sendConfigure(size, QWaylandWlShellSurface::NoneEdge);
}

WaylandCompositor *WindowSurface::compositor() const
{
    return m_compositor;
}

QWaylandSurface *WindowSurface::surface() const
{
    return m_surface;
}

qint64 WindowSurface::processId() const
{
    return m_surface->client()->processId();
}

QWindow *WindowSurface::outputWindow() const
{
    if (QWaylandView *v = m_surface->primaryView())
        return v->output()->window();
    return nullptr;
}

void WindowSurface::ping()
{
    if (m_xdgSurface) {
        // FIXME: ping should be done per-application, not per-window
        m_compositor->xdgPing(this);
    } else {
        m_wlSurface->ping();
    }
}

void WindowSurface::close()
{
    if (m_xdgSurface)
        m_xdgSurface->sendClose();
    else
        qCWarning(LogGraphics) << this << "is not using the XDG V5 Shell extension. Unable to send close signal.";
}

WaylandCompositor::WaylandCompositor(QQuickWindow *window, const QString &waylandSocketName)
    : QWaylandQuickCompositor()
    , m_wlShell(new QWaylandWlShell(this))
    , m_xdgShell(new QWaylandXdgShellV5(this))
    , m_amExtension(new WaylandQtAMServerExtension(this))
    , m_textInputManager(new QWaylandTextInputManager(this))
{
    setSocketName(waylandSocketName.toUtf8());
    registerOutputWindow(window);

    connect(this, &QWaylandCompositor::surfaceRequested, this, &WaylandCompositor::doCreateSurface);

    connect(m_wlShell, &QWaylandWlShell::wlShellSurfaceRequested, this, &WaylandCompositor::createWlSurface);

    connect(m_xdgShell, &QWaylandXdgShellV5::xdgSurfaceCreated, this, &WaylandCompositor::onXdgSurfaceCreated);
    connect(m_xdgShell, &QWaylandXdgShellV5::pong, this, &WaylandCompositor::onXdgPongReceived);

    auto wmext = new QWaylandQtWindowManager(this);
    connect(wmext, &QWaylandQtWindowManager::openUrl, this, [](QWaylandClient *client, const QUrl &url) {
        if (ApplicationManager::instance()->fromProcessId(client->processId()))
            ApplicationManager::instance()->openUrl(url.toString());
    });

    create();

    // set a sensible default keymap
    defaultSeat()->keymap()->setLayout(QLocale::system().name().section('_', 1, 1).toLower());
}

void WaylandCompositor::xdgPing(WindowSurface* surface)
{
    uint serial = m_xdgShell->ping(surface->client());
    m_xdgPingMap[serial] = surface;
}

void WaylandCompositor::onXdgPongReceived(uint serial)
{
    WindowSurface *surface = m_xdgPingMap.take(serial);
    if (surface) {
        emit surface->pong();
    }
}

void WaylandCompositor::registerOutputWindow(QQuickWindow* window)
{
    auto output = new QWaylandQuickOutput(this, window);
    output->setSizeFollowsWindow(true);
    m_outputs.append(output);
    window->winId();
}

WaylandQtAMServerExtension *WaylandCompositor::amExtension()
{
    return m_amExtension;
}

void WaylandCompositor::doCreateSurface(QWaylandClient *client, uint id, int version)
{
    (void) new WindowSurface(this, client, id, version);
}


void WaylandCompositor::createWlSurface(QWaylandSurface *surface, const QWaylandResource &resource)
{
    WindowSurface *windowSurface = static_cast<WindowSurface *>(surface);
    QWaylandWlShellSurface *ss = new QWaylandWlShellSurface(m_wlShell, windowSurface, resource);
    windowSurface->setShellSurface(ss);

    connect(windowSurface, &QWaylandSurface::hasContentChanged, this, [this, windowSurface]() {
        if (windowSurface->hasContent())
            emit this->surfaceMapped(static_cast<WindowSurface*>(windowSurface));
    });
}


void WaylandCompositor::onXdgSurfaceCreated(QWaylandXdgSurfaceV5 *xdgSurface)
{
    WindowSurface *windowSurface = static_cast<WindowSurface*>(xdgSurface->surface());

    Q_ASSERT(!windowSurface->m_wlSurface);
    windowSurface->m_xdgSurface = xdgSurface;

    connect(windowSurface, &QWaylandSurface::hasContentChanged, this, [this, windowSurface]() {
        if (windowSurface->hasContent())
            emit this->surfaceMapped(windowSurface);
    });
}

QT_END_NAMESPACE_AM
