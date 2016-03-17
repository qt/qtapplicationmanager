/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QWaylandQuickSurface>
#include <QWaylandQuickItem>
#include <QWaylandQuickCompositor>

#include "windowmanager.h"

QT_FORWARD_DECLARE_CLASS(QWaylandResource)
QT_FORWARD_DECLARE_CLASS(QWaylandShell)
QT_FORWARD_DECLARE_CLASS(QWaylandShellSurface)
QT_BEGIN_NAMESPACE
namespace QtWayland {
class ExtendedSurface;
class SurfaceExtensionGlobal;
}
QT_END_NAMESPACE

class SurfaceQuickItem;


class Surface : public QWaylandQuickSurface, public WindowSurface
{
    Q_OBJECT

public:
    Surface(QWaylandCompositor *comp, QWaylandClient *client, uint id, int version);

    void setShellSurface(QWaylandShellSurface *ss);
    void setExtendedSurface(QtWayland::ExtendedSurface *e);

    QWaylandShellSurface *shellSurface() const;
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
    QWaylandShellSurface *m_shellSurface;
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
    void waylandSurfaceMappedChanged();
    QWaylandSurface *waylandSurfaceFromItem(QQuickItem *surfaceItem) const;

private:
    WindowManager *m_manager;
    QWaylandShell *m_shell;
    QVector<QWaylandOutput *> m_outputs;
    QtWayland::SurfaceExtensionGlobal *m_surfExt;
};
