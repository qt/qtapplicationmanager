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

#include <QWaylandQuickSurface>
#include <QWaylandQuickItem>
#include <QWaylandQuickCompositor>

#include <QtAppManWindow/windowmanager.h>

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

QT_BEGIN_NAMESPACE_AM

class SurfaceQuickItem;

class Surface : public QWaylandQuickSurface, public WindowSurface
{
    Q_OBJECT

public:
    Surface(QWaylandCompositor *comp, QWaylandClient *client, uint id, int version);

    void setShellSurface(QWaylandWlShellSurface *ss);
    void setExtendedSurface(QtWayland::ExtendedSurface *e);

    QWaylandWlShellSurface *shellSurface() const;
    QtWayland::ExtendedSurface *extendedSurface() const;

    QQuickItem *item() const override;

    void takeFocus() override;
    void ping() override;
    qint64 processId() const override;
    QWindow *outputWindow() const override;

    QVariantMap windowProperties() const override;
    void setWindowProperty(const QString &n, const QVariant &v) override;

    void connectPong(const std::function<void ()> &cb) override;
    void connectWindowPropertyChanged(const std::function<void (const QString &, const QVariant &)> &cb) override;

private:
    SurfaceQuickItem *m_item;
    QWaylandWlShellSurface *m_shellSurface;
    QtWayland::ExtendedSurface *m_ext;
};

class SurfaceQuickItem : public QWaylandQuickItem
{
    Q_OBJECT

public:
    SurfaceQuickItem(Surface *s);

    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

    Surface *m_surface;
};


class WaylandCompositor : public QWaylandQuickCompositor
{
public:
    WaylandCompositor(QQuickWindow* window, const QString &waylandSocketName, WindowManager *manager);

    void registerOutputWindow(QQuickWindow *window);
    void doCreateSurface(QWaylandClient *client, uint id, int version);
    void createShellSurface(QWaylandSurface *surface, const QWaylandResource &resource);
    void extendedSurfaceReady(QtWayland::ExtendedSurface *ext, QWaylandSurface *surface);
    QWaylandSurface *waylandSurfaceFromItem(QQuickItem *surfaceItem) const;

private:
    WindowManager *m_manager;
    QWaylandWlShell *m_shell;
    QVector<QWaylandOutput *> m_outputs;
    QtWayland::SurfaceExtensionGlobal *m_surfExt;
    QWaylandTextInputManager *m_textInputManager;
};

QT_END_NAMESPACE_AM
