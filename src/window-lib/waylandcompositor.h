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

#pragma once

#include <QWaylandQuickCompositor>
#include <QtAppManWindow/windowmanager.h>

#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
#  include <QWaylandQuickSurface>
#  include <QWaylandQuickItem>

QT_FORWARD_DECLARE_CLASS(QWaylandResource)
QT_FORWARD_DECLARE_CLASS(QWaylandWlShell)
QT_FORWARD_DECLARE_CLASS(QWaylandWlShellSurface)
QT_FORWARD_DECLARE_CLASS(QWaylandTextInputManager)
QT_BEGIN_NAMESPACE
namespace QtWayland {
class ExtendedSurface;
class SurfaceExtensionGlobal;
}
QT_END_NAMESPACE

#else // QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
#  include <QtAppManWindow/windowmanager.h>

QT_FORWARD_DECLARE_CLASS(QWaylandSurfaceItem)

#endif // QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)

QT_BEGIN_NAMESPACE_AM

class WindowSurfaceQuickItem;

// A WindowSurface object exists for every Wayland surface created in the Wayland server.
// Not every WindowSurface maybe an application's Window though - those that are, are available
// through the WindowManager model.

#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
class WindowSurface : public QObject
{
    Q_OBJECT
public:
    WindowSurface(QWaylandSurface *surface);

private:
    QWaylandSurfaceItem *m_item;

#else
class WindowSurface : public QWaylandQuickSurface
{
    Q_OBJECT
public:
    WindowSurface(QWaylandCompositor *comp, QWaylandClient *client, uint id, int version);
    QWaylandWlShellSurface *shellSurface() const;

private:
    void setShellSurface(QWaylandWlShellSurface *ss);
    void setExtendedSurface(QtWayland::ExtendedSurface *e);

private:
    WindowSurfaceQuickItem *m_item = nullptr;
    QWaylandWlShellSurface *m_shellSurface = nullptr;
    QtWayland::ExtendedSurface *m_extendedSurface = nullptr;
#endif

public:
    QWaylandSurface *surface() const;
    QQuickItem *item() const;
    qint64 processId() const;
    QWindow *outputWindow() const;

    void takeFocus();
    void ping();

    QVariantMap windowProperties() const;
    void setWindowProperty(const QString &name, const QVariant &value);

signals:
    void pong();
    void windowPropertyChanged(const QString &name, const QVariant &value);

private:
    QWaylandSurface *m_surface;

    friend class WaylandCompositor;
};

class WaylandCompositor : public QWaylandQuickCompositor // clazy:exclude=missing-qobject-macro
{
public:
    WaylandCompositor(QQuickWindow* window, const QString &waylandSocketName, WindowManager *manager);
    void registerOutputWindow(QQuickWindow *window);
    QWaylandSurface *waylandSurfaceFromItem(QQuickItem *surfaceItem) const;

protected:
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    void doCreateSurface(QWaylandClient *client, uint id, int version);
    void createShellSurface(QWaylandSurface *surface, const QWaylandResource &resource);
    void extendedSurfaceReady(QtWayland::ExtendedSurface *ext, QWaylandSurface *surface);

    QWaylandWlShell *m_shell;
    QVector<QWaylandOutput *> m_outputs;
    QtWayland::SurfaceExtensionGlobal *m_surfExt;
    QWaylandTextInputManager *m_textInputManager;
#else
    void surfaceCreated(QWaylandSurface *surface) override;
    bool openUrl(QWaylandClient *client, const QUrl &url) override;
    void sendCallbacks();
public:
    const char *socketName() const; // we need to shadow the base class' version, since it is broken
#endif // QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)

private:
    WindowManager *m_manager;
};

QT_END_NAMESPACE_AM
