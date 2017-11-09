/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
#  include <QWaylandQuickSurface>
#  include <QWaylandSurfaceItem>
#  include <QWaylandClient>

#  include "window.h"
#else
#  include <QWaylandWlShell>
#  include <QWaylandQuickOutput>
#  include <QWaylandTextInputManager>
#  include <private/qwlextendedsurface_p.h>
#  if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
#    include <QWaylandWindowManagerExtension>
typedef QWaylandWindowManagerExtension QWaylandQtWindowManager;
#  else
#    include <QWaylandQtWindowManager>
#  endif
#  include "waylandcompositor_p.h"
#endif

QT_BEGIN_NAMESPACE_AM

#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
WindowSurface::WindowSurface(QWaylandSurface *surface)
    : m_item(nullptr)
    , m_surface(surface)
{
    connect(m_surface, &QObject::destroyed, [this]() { delete this; });
    connect(m_surface, &QWaylandSurface::pong, this, &WindowSurface::pong);
    connect(m_surface, &QWaylandSurface::windowPropertyChanged, this, &WindowSurface::windowPropertyChanged);
}
#else

WindowSurface::WindowSurface(QWaylandCompositor *comp, QWaylandClient *client, uint id, int version)
    : QWaylandQuickSurface(comp, client, id, version)
    , m_surface(this)
{ }

void WindowSurface::setShellSurface(QWaylandWlShellSurface *shellSurface)
{
    m_shellSurface = shellSurface;
    connect(m_shellSurface, &QWaylandWlShellSurface::pong,
            this, &WindowSurface::pong);
    m_item = new WindowSurfaceQuickItem(this);
}

void WindowSurface::setExtendedSurface(QtWayland::ExtendedSurface *extendedSurface)
{
    m_extendedSurface = extendedSurface;
    if (m_extendedSurface) {
        connect(m_extendedSurface, &QtWayland::ExtendedSurface::windowPropertyChanged,
                this, &WindowSurface::windowPropertyChanged);
    }
}

QWaylandWlShellSurface *WindowSurface::shellSurface() const
{
    return m_shellSurface;
}

#endif


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
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    if (QWaylandOutput *o = m_surface->mainOutput())
        return o->window();
#elif QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
    if (QWaylandView *v = m_surface->throttlingView())
        return v->output()->window();
#else
    if (QWaylandView *v = m_surface->primaryView())
        return v->output()->window();
#endif
    return nullptr;
}

void WindowSurface::takeFocus()
{
    m_item->takeFocus();
}

void WindowSurface::ping()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    m_surface->ping();
#else
    m_shellSurface->ping();
#endif
}

QVariantMap WindowSurface::windowProperties() const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    return m_surface->windowProperties();
#else
    return m_extendedSurface ? m_extendedSurface->windowProperties() : QVariantMap();
#endif
}

void WindowSurface::setWindowProperty(const QString &name, const QVariant &value)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    m_surface->setWindowProperty(name, value);
#else
    if (m_extendedSurface)
        m_extendedSurface->setWindowProperty(name, value);
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)

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

#endif // QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)


WaylandCompositor::WaylandCompositor(QQuickWindow *window, const QString &waylandSocketName, WindowManager *manager)
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    : QWaylandQuickCompositor(qPrintable(waylandSocketName), DefaultExtensions | SubSurfaceExtension)
    , m_manager(manager)
{
    registerOutputWindow(window);
    window->winId();
    addDefaultShell();
    QObject::connect(window, &QQuickWindow::beforeSynchronizing, [this]() { frameStarted(); });
    QObject::connect(window, &QQuickWindow::afterRendering, [this]() { sendCallbacks(); });
    setOutputGeometry(window->geometry());

#else // QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    : QWaylandQuickCompositor()
    , m_shell(new QWaylandWlShell(this))
    , m_surfExt(new QtWayland::SurfaceExtensionGlobal(this))
    , m_textInputManager(new QWaylandTextInputManager(this))
    , m_manager(manager)
{
    setSocketName(waylandSocketName.toUtf8());
    registerOutputWindow(window);

#  if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
    connect(this, &QWaylandCompositor::createSurface, this, &WaylandCompositor::doCreateSurface);
#  else
    connect(this, &QWaylandCompositor::surfaceRequested, this, &WaylandCompositor::doCreateSurface);
#  endif
    connect(this, &QWaylandCompositor::surfaceCreated, [this](QWaylandSurface *s) {
        connect(s, &QWaylandSurface::surfaceDestroyed, this, [this, s]() {
            m_manager->waylandSurfaceDestroyed(static_cast<WindowSurface *>(s));
        });
        m_manager->waylandSurfaceCreated(static_cast<WindowSurface *>(s));
    });

#  if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
    connect(m_shell, &QWaylandWlShell::createShellSurface, this, &WaylandCompositor::createShellSurface);
#  else
    connect(m_shell, &QWaylandWlShell::wlShellSurfaceRequested, this, &WaylandCompositor::createShellSurface);
#  endif
    connect(m_surfExt, &QtWayland::SurfaceExtensionGlobal::extendedSurfaceReady, this, &WaylandCompositor::extendedSurfaceReady);

    auto wmext = new QWaylandQtWindowManager(this);
    connect(wmext, &QWaylandQtWindowManager::openUrl, this, [](QWaylandClient *client, const QUrl &url) {
        if (ApplicationManager::instance()->fromProcessId(client->processId()))
            ApplicationManager::instance()->openUrl(url.toString());
    });

    create();
#endif // QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
}

void WaylandCompositor::registerOutputWindow(QQuickWindow* window)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    createOutput(window, QString(), QString());
#else
    auto output = new QWaylandQuickOutput(this, window);
    output->setSizeFollowsWindow(true);
    m_outputs.append(output);
#endif
    window->winId();
}

QWaylandSurface *WaylandCompositor::waylandSurfaceFromItem(QQuickItem *surfaceItem) const
{
    // QWaylandQuickItem::surface() will return a nullptr to WindowManager::waylandSurfaceDestroyed,
    // if the app crashed. We return our internal copy of that surface pointer instead.
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    if (QWaylandSurfaceItem *item = qobject_cast<QWaylandSurfaceItem *>(surfaceItem))
          return item->surface();
#else
    if (WindowSurfaceQuickItem *item = qobject_cast<WindowSurfaceQuickItem *>(surfaceItem))
        return item->m_windowSurface->surface();
#endif
    return nullptr;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)

void WaylandCompositor::doCreateSurface(QWaylandClient *client, uint id, int version)
{
    (void) new WindowSurface(this, client, id, version);
}

void WaylandCompositor::createShellSurface(QWaylandSurface *surface, const QWaylandResource &resource)
{
    WindowSurface *windowSurface = static_cast<WindowSurface *>(surface);
    QWaylandWlShellSurface *ss = new QWaylandWlShellSurface(m_shell, windowSurface, resource);
    windowSurface->setShellSurface(ss);

#if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
    connect(windowSurface, &QWaylandSurface::mappedChanged, this, [this, windowSurface]() {
        if (windowSurface->isMapped())
#else
    connect(windowSurface, &QWaylandSurface::hasContentChanged, this, [this, windowSurface]() {
        if (windowSurface->hasContent())
#endif
            m_manager->waylandSurfaceMapped(windowSurface);
        else
            m_manager->waylandSurfaceUnmapped(windowSurface);
    });
}

void WaylandCompositor::extendedSurfaceReady(QtWayland::ExtendedSurface *ext, QWaylandSurface *surface)
{
    WindowSurface *windowSurface = static_cast<WindowSurface *>(surface);
    windowSurface->setExtendedSurface(ext);
}

#else // if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)

void WaylandCompositor::surfaceCreated(QWaylandSurface *surface)
{
    WindowSurface *windowSurface = new WindowSurface(surface);
    m_manager->waylandSurfaceCreated(windowSurface);
    QObject::connect(surface, &QWaylandSurface::mapped, [windowSurface, surface, this]() {
        windowSurface->m_item = static_cast<QWaylandSurfaceItem *>(surface->views().at(0));
        windowSurface->m_item->setResizeSurfaceToItem(true);
        windowSurface->m_item->setTouchEventsEnabled(true);
        m_manager->waylandSurfaceMapped(windowSurface);
    });
    QObject::connect(surface, &QWaylandSurface::unmapped, [windowSurface, this]() {
        m_manager->waylandSurfaceUnmapped(windowSurface);
    });
    QObject::connect(surface, &QWaylandSurface::surfaceDestroyed, [windowSurface, this]() {
        m_manager->waylandSurfaceDestroyed(windowSurface);
    });

}

bool WaylandCompositor::openUrl(QWaylandClient *client, const QUrl &url)
{
    if (ApplicationManager::instance()->fromProcessId(client->processId()))
        return ApplicationManager::instance()->openUrl(url.toString());
    return false;
}

void WaylandCompositor::sendCallbacks()
{
    QList<QWaylandSurface *> listToSend;

    // TODO: optimize! no need to send this to hidden/minimized/offscreen/etc. surfaces
    const auto windows = m_manager->windows();
    for (const Window *win : windows) {
        if (!win->isClosing() && !win->isInProcess()) {
            if (QWaylandSurface *surface = waylandSurfaceFromItem(win->windowItem())) {
                listToSend << surface;
            }
        }
    }

    if (!listToSend.isEmpty())
        sendFrameCallbacks(listToSend);
}

const char *WaylandCompositor::socketName() const
{
    static QByteArray sn;
    if (sn.isEmpty()) {
        sn = QWaylandCompositor::socketName();
        if (sn.isEmpty()) {
            sn = qgetenv("WAYLAND_DISPLAY");
            if (sn.isEmpty())
                sn = "wayland-0";
        }
    }
    return sn.constData();
}

#endif // if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)

QT_END_NAMESPACE_AM
