/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#pragma once

#include <QtAppManCommon/global.h>

#if defined(AM_MULTI_PROCESS)

#include <QWaylandQuickCompositor>

#include <QWaylandQuickSurface>
#include <QWaylandQuickItem>

#include <QMap>
#include <QPointer>

QT_FORWARD_DECLARE_CLASS(QWaylandResource)
QT_FORWARD_DECLARE_CLASS(QWaylandWlShell)
QT_FORWARD_DECLARE_CLASS(QWaylandWlShellSurface)
QT_FORWARD_DECLARE_CLASS(QWaylandTextInputManager)

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    QT_FORWARD_DECLARE_CLASS(QWaylandXdgShell)
    QT_FORWARD_DECLARE_CLASS(QWaylandXdgSurface)
    QT_FORWARD_DECLARE_CLASS(QWaylandXdgToplevel)
    typedef QWaylandXdgShell WaylandXdgShell;
    typedef QWaylandXdgSurface WaylandXdgSurface;
#else
    QT_FORWARD_DECLARE_CLASS(QWaylandXdgShellV5)
    QT_FORWARD_DECLARE_CLASS(QWaylandXdgSurfaceV5)
    typedef QWaylandXdgShellV5 WaylandXdgShell;
    typedef QWaylandXdgSurfaceV5 WaylandXdgSurface;
#endif

QT_BEGIN_NAMESPACE_AM

class WaylandCompositor;
class WaylandQtAMServerExtension;
class WindowSurfaceQuickItem;

// A WindowSurface object exists for every Wayland surface created in the Wayland server.
// Not every WindowSurface maybe an application's Window though - those that are, are available
// through the WindowManager model.

// On Qt 5.11: we support wl-shell and xdg-shell-v5 shell integration extensions. There's no xdg-shell yet
//             in qtwayland
//
// On Qt 5.12: we support wl-shell and xdg-shell shell integration extensions. Both wl-shell and xdg-shell-v5
//             are deprecated and additionally xdg-shell-v5 is partially broken in qtwayland.
//
// In any case, wl-shell doesn't provide all the features needed by appman, so clients using it will never work
// perfectly.

class WindowSurface : public QWaylandQuickSurface
{
    Q_OBJECT
public:
    WindowSurface(WaylandCompositor *comp, QWaylandClient *client, uint id, int version);
    QWaylandWlShellSurface *shellSurface() const;
    WaylandCompositor *compositor() const;

    void sendResizing(const QSize &size);

private:
    void setShellSurface(QWaylandWlShellSurface *ss);

public:
    QWaylandSurface *surface() const;
    qint64 processId() const;
    QWindow *outputWindow() const;

    void ping();
    void close();

signals:
    void pong();

private:
    QWaylandSurface *m_surface;
    WaylandCompositor *m_compositor;
    QWaylandWlShellSurface *m_wlSurface = nullptr;

    WaylandXdgSurface *m_xdgSurface = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    QWaylandXdgToplevel *m_topLevel = nullptr;
#endif

    friend class WaylandCompositor;
};

class WaylandCompositor : public QWaylandQuickCompositor // clazy:exclude=missing-qobject-macro
{
    Q_OBJECT
public:
    WaylandCompositor(QQuickWindow* window, const QString &waylandSocketName);
    void registerOutputWindow(QQuickWindow *window);

    WaylandQtAMServerExtension *amExtension();

    void xdgPing(WindowSurface*);

signals:
    void surfaceMapped(QT_PREPEND_NAMESPACE_AM(WindowSurface) *surface);

protected:
    void doCreateSurface(QWaylandClient *client, uint id, int version);
    void createWlSurface(QWaylandSurface *surface, const QWaylandResource &resource);
    void onXdgSurfaceCreated(WaylandXdgSurface *xdgSurface);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    void onTopLevelCreated(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface);
#endif
    void onXdgPongReceived(uint serial);

    QWaylandWlShell *m_wlShell;
    WaylandXdgShell *m_xdgShell;
    QVector<QWaylandOutput *> m_outputs;
    WaylandQtAMServerExtension *m_amExtension;
    QWaylandTextInputManager *m_textInputManager;
    QMap<uint, QPointer<WindowSurface>> m_xdgPingMap;
};

QT_END_NAMESPACE_AM

#endif // AM_MULTIPROCESS
