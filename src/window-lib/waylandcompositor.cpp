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

#include "global.h"
#include "logging.h"
#include "windowmanager.h"
#include "application.h"
#include "applicationmanager.h"
#include "waylandcompositor.h"

#include <QWaylandWlShell>
#include <QWaylandQuickOutput>
#include <QWaylandTextInputManager>
#include <QWaylandQtWindowManager>
#include "waylandqtamserverextension_p.h"
#include "waylandcompositor_p.h"

QT_BEGIN_NAMESPACE_AM

WindowSurface::WindowSurface(WaylandCompositor *comp, QWaylandClient *client, uint id, int version)
    : QWaylandQuickSurface(comp, client, id, version)
    , m_surface(this)
    , m_compositor(comp)
{ }

void WindowSurface::setShellSurface(QWaylandWlShellSurface *shellSurface)
{
    m_shellSurface = shellSurface;
    connect(m_shellSurface, &QWaylandWlShellSurface::pong,
            this, &WindowSurface::pong);
    m_item = new WindowSurfaceQuickItem(this);
}

QWaylandWlShellSurface *WindowSurface::shellSurface() const
{
    return m_shellSurface;
}

WaylandCompositor *WindowSurface::compositor() const
{
    return m_compositor;
}

QWaylandSurface *WindowSurface::surface() const
{
    return m_surface;
}

QQuickItem *WindowSurface::item() const
{
    return m_item;
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

void WindowSurface::takeFocus()
{
    m_item->takeFocus();
}

void WindowSurface::ping()
{
    m_shellSurface->ping();
}


WindowSurfaceQuickItem::WindowSurfaceQuickItem(WindowSurface *windowSurface)
    : QWaylandQuickItem()
    , m_windowSurface(windowSurface)
{
    setSurface(windowSurface);
    setTouchEventsEnabled(true);
    setSizeFollowsSurface(false);
}

void WindowSurfaceQuickItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (newGeometry.isValid()) {
        const Application *app = nullptr; // prevent expensive lookup when not printing qDebugs
        qCDebug(LogGraphics) << "Sending geometry change request to Wayland client for surface"
                            << m_windowSurface->item() << "old:" << oldGeometry.size() << "new:"
                            << newGeometry.size() << "of"
                            << ((app = ApplicationManager::instance()->fromProcessId(m_windowSurface->client()->processId()))
                                ? app->id() : QString::fromLatin1("pid: %1").arg(m_windowSurface->client()->processId()));
        m_windowSurface->shellSurface()->sendConfigure(newGeometry.size().toSize(), QWaylandWlShellSurface::NoneEdge);
    }

    QWaylandQuickItem::geometryChanged(newGeometry, oldGeometry);
}


WaylandCompositor::WaylandCompositor(QQuickWindow *window, const QString &waylandSocketName, WindowManager *manager)
    : QWaylandQuickCompositor()
    , m_shell(new QWaylandWlShell(this))
    , m_amExtension(new WaylandQtAMServerExtension(this))
    , m_textInputManager(new QWaylandTextInputManager(this))
    , m_manager(manager)
{
    setSocketName(waylandSocketName.toUtf8());
    registerOutputWindow(window);

    connect(this, &QWaylandCompositor::surfaceRequested, this, &WaylandCompositor::doCreateSurface);
    connect(this, &QWaylandCompositor::surfaceCreated, [this](QWaylandSurface *s) {
        connect(s, &QWaylandSurface::surfaceDestroyed, this, [this, s]() {
            m_manager->waylandSurfaceDestroyed(static_cast<WindowSurface *>(s));
        });
        m_manager->waylandSurfaceCreated(static_cast<WindowSurface *>(s));
    });

    connect(m_shell, &QWaylandWlShell::wlShellSurfaceRequested, this, &WaylandCompositor::createShellSurface);

    auto wmext = new QWaylandQtWindowManager(this);
    connect(wmext, &QWaylandQtWindowManager::openUrl, this, [](QWaylandClient *client, const QUrl &url) {
        if (ApplicationManager::instance()->fromProcessId(client->processId()))
            ApplicationManager::instance()->openUrl(url.toString());
    });

    create();
}

void WaylandCompositor::registerOutputWindow(QQuickWindow* window)
{
    auto output = new QWaylandQuickOutput(this, window);
    output->setSizeFollowsWindow(true);
    m_outputs.append(output);
    window->winId();
}

QWaylandSurface *WaylandCompositor::waylandSurfaceFromItem(QQuickItem *surfaceItem) const
{
    // QWaylandQuickItem::surface() will return a nullptr to WindowManager::waylandSurfaceDestroyed,
    // if the app crashed. We return our internal copy of that surface pointer instead.
    if (WindowSurfaceQuickItem *item = qobject_cast<WindowSurfaceQuickItem *>(surfaceItem))
        return item->m_windowSurface->surface();
    return nullptr;
}

WaylandQtAMServerExtension *WaylandCompositor::amExtension()
{
    return m_amExtension;
}

void WaylandCompositor::doCreateSurface(QWaylandClient *client, uint id, int version)
{
    (void) new WindowSurface(this, client, id, version);
}

void WaylandCompositor::createShellSurface(QWaylandSurface *surface, const QWaylandResource &resource)
{
    WindowSurface *windowSurface = static_cast<WindowSurface *>(surface);
    QWaylandWlShellSurface *ss = new QWaylandWlShellSurface(m_shell, windowSurface, resource);
    windowSurface->setShellSurface(ss);

    connect(windowSurface, &QWaylandSurface::hasContentChanged, this, [this, windowSurface]() {
        if (windowSurface->hasContent())
            m_manager->waylandSurfaceMapped(windowSurface);
        else
            m_manager->waylandSurfaceUnmapped(windowSurface);
    });
}

QT_END_NAMESPACE_AM
