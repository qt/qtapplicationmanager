/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

QT_FORWARD_DECLARE_CLASS(QWaylandXdgShell)
QT_FORWARD_DECLARE_CLASS(QWaylandXdgSurface)
QT_FORWARD_DECLARE_CLASS(QWaylandXdgToplevel)
QT_FORWARD_DECLARE_CLASS(QWaylandXdgPopup)


QT_BEGIN_NAMESPACE_AM

class WaylandCompositor;
class WaylandQtAMServerExtension;
class WindowSurfaceQuickItem;

// A WindowSurface object exists for every Wayland surface created in the Wayland server.
// Not every WindowSurface maybe an application's Window though - those that are, are available
// through the WindowManager model.
//
// we support wl-shell and xdg-shell shell integration extensions. Both wl-shell and xdg-shell-v5
// are deprecated and additionally xdg-shell-v5 is partially broken in qtwayland.
//
// In any case, wl-shell doesn't provide all the features needed by appman, so clients using it will never work
// perfectly.

class WindowSurface : public QWaylandQuickSurface
{
    Q_OBJECT
public:
    WindowSurface(WaylandCompositor *comp, QWaylandClient *client, uint id, int version);
    QWaylandSurface *surface() const;
    QWaylandWlShellSurface *shellSurface() const;
    QWaylandXdgSurface *xdgSurface() const;
    WaylandCompositor *compositor() const;

    QString applicationId() const;
    bool isPopup() const;
    QRect popupGeometry() const;
    void sendResizing(const QSize &size);

    qint64 processId() const;
    QWindow *outputWindow() const;

    void ping();
    void close();

signals:
    void pong();
    void popupGeometryChanged();
    void xdgSurfaceChanged();

private:
    void setShellSurface(QWaylandWlShellSurface *ss);

private:
    QWaylandSurface *m_surface;
    WaylandCompositor *m_compositor;
    QWaylandWlShellSurface *m_wlSurface = nullptr;

    QWaylandXdgSurface *m_xdgSurface = nullptr;
    QWaylandXdgToplevel *m_topLevel = nullptr;
    QWaylandXdgPopup *m_popup = nullptr;

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
    void onXdgSurfaceCreated(QWaylandXdgSurface *xdgSurface);
    void onTopLevelCreated(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface);
    void onPopupCreated(QWaylandXdgPopup *popup, QWaylandXdgSurface *xdgSurface);
    void onXdgPongReceived(uint serial);

    QWaylandWlShell *m_wlShell;
    QWaylandXdgShell *m_xdgShell;
    QVector<QWaylandOutput *> m_outputs;
    WaylandQtAMServerExtension *m_amExtension;
    QWaylandTextInputManager *m_textInputManager;
    QMap<uint, QPointer<WindowSurface>> m_xdgPingMap;
};

QT_END_NAMESPACE_AM

#endif // AM_MULTIPROCESS
