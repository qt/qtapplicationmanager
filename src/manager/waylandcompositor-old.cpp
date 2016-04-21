/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
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

#include <QWaylandQuickSurface>
#include <QWaylandSurfaceItem>
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
#  include <QWaylandOutput>
#  include <QWaylandClient>
#endif
#include <QQuickView>

#include "global.h"
#include "waylandwindow.h"
#include "applicationmanager.h"
#include "waylandcompositor-old.h"


Surface::Surface(QWaylandSurface *s)
    : m_item(0)
{
    m_surface = s;
    QObject::connect(s, &QObject::destroyed, [this]() { delete this; });
}

QQuickItem *Surface::item() const { return m_item; }

void Surface::takeFocus() { m_item->takeFocus(); }

void Surface::ping() { m_surface->ping(); }

qint64 Surface::processId() const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    return m_surface->client()->processId();
#else
    return m_surface->processId();
#endif
}

QWindow *Surface::outputWindow() const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    if (QWaylandOutput *o = m_surface->mainOutput())
        return o->window();
    return 0;
#else
    return m_surface->compositor()->window();
#endif
}

QVariantMap Surface::windowProperties() const { return m_surface->windowProperties(); }

void Surface::setWindowProperty(const QString &n, const QVariant &v) { m_surface->setWindowProperty(n, v); }

void Surface::connectPong(const std::function<void ()> &cb)
{
    QObject::connect(m_surface, &QWaylandSurface::pong, cb);
}

void Surface::connectWindowPropertyChanged(const std::function<void (const QString &, const QVariant &)> &cb)
{
    QObject::connect(m_surface, &QWaylandSurface::windowPropertyChanged, cb);
}

WaylandCompositor::WaylandCompositor(QQuickWindow *window, const QString &waylandSocketName, WindowManager *manager)
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    : QWaylandQuickCompositor(qPrintable(waylandSocketName), DefaultExtensions | SubSurfaceExtension)
#else
    : QWaylandQuickCompositor(window, qPrintable(waylandSocketName), DefaultExtensions | SubSurfaceExtension)
#endif
    , m_manager(manager)
{
    registerOutputWindow(window);

    setenv("WAYLAND_DISPLAY", qPrintable(waylandSocketName), 1);

    window->winId();
    addDefaultShell();

    QObject::connect(window, &QQuickWindow::beforeSynchronizing, [this]() { frameStarted(); });
    QObject::connect(window, &QQuickWindow::afterRendering, [this]() { sendCallbacks(); });
    setOutputGeometry(window->geometry());
}

void WaylandCompositor::registerOutputWindow(QQuickWindow *window)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    createOutput(window, QString(), QString());
    window->winId();
#else
    Q_UNUSED(window)
#endif
}

void WaylandCompositor::surfaceCreated(QWaylandSurface *surface)
{
    Surface *s = new Surface(surface);
    m_manager->waylandSurfaceCreated(s);
    QObject::connect(surface, &QWaylandSurface::mapped, [s, surface, this]() {
        s->m_item = static_cast<QWaylandSurfaceItem *>(surface->views().at(0));
        s->m_item->setResizeSurfaceToItem(true);
        s->m_item->setTouchEventsEnabled(true);
        m_manager->waylandSurfaceMapped(s);
    });
    QObject::connect(surface, &QWaylandSurface::unmapped, [s, this]() { m_manager->waylandSurfaceUnmapped(s); });
}

#if QT_VERSION < QT_VERSION_CHECK(5,5,0)
bool WaylandCompositor::openUrl(WaylandClient *client, const QUrl &url)
{
    QList<QWaylandSurface *> surfaces = surfacesForClient(client);
    qint64 pid = surfaces.isEmpty() ? 0 : surfaces.at(0)->processId();
#else
bool WaylandCompositor::openUrl(QWaylandClient *client, const QUrl &url)
{
    qint64 pid = client->processId();
#endif
    if (ApplicationManager::instance()->fromProcessId(pid))
        return ApplicationManager::instance()->openUrl(url.toString());
    return false;
}

QWaylandSurface *WaylandCompositor::waylandSurfaceFromItem(QQuickItem *surfaceItem) const
{
    if (QWaylandSurfaceItem *item = qobject_cast<QWaylandSurfaceItem *>(surfaceItem))
        return item->surface();
    return 0;
}

void WaylandCompositor::sendCallbacks()
{
    QList<QWaylandSurface *> listToSend;

    // TODO: optimize! no need to send this to hidden/minimized/offscreen/etc. surfaces
    foreach (const Window *win, m_manager->windows()) {
        if (!win->isClosing() && !win->isInProcess()) {
            if (QWaylandSurface *surface = waylandSurfaceFromItem(win->windowItem())) {
                listToSend << surface;
            }
        }
    }

    if (!listToSend.isEmpty()) {
        //qCDebug(LogWayland) << "sending callbacks to clients: " << listToSend; // note: debug-level 6 needs to be enabled manually...
        sendFrameCallbacks(listToSend);
    }
}
