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

#pragma once

#include <QWaylandQuickCompositor>

#include "windowmanager.h"

QT_FORWARD_DECLARE_CLASS(QWaylandSurfaceItem)


class SurfaceQuickItem;

class Surface : public WindowSurface
{
public:
    Surface(QWaylandSurface *s);

    QQuickItem *item() const override;

    void takeFocus() override;
    void ping() override;
    qint64 processId() const override;
    QWindow *outputWindow() const override;

    QVariantMap windowProperties() const override;
    void setWindowProperty(const QString &n, const QVariant &v) override;

    void connectPong(const std::function<void ()> &cb) override;
    void connectWindowPropertyChanged(const std::function<void (const QString &, const QVariant &)> &cb) override;

    QWaylandSurfaceItem *m_item;
};


class WaylandCompositor : public QWaylandQuickCompositor
{
public:
    WaylandCompositor(QQuickWindow *window, const QString &waylandSocketName, WindowManager *manager);

    void registerOutputWindow(QQuickWindow *window);

    void surfaceCreated(QWaylandSurface *surface) override;

#if QT_VERSION < QT_VERSION_CHECK(5,5,0)
    bool openUrl(WaylandClient *client, const QUrl &url) override;
#else
    bool openUrl(QWaylandClient *client, const QUrl &url) override;
#endif

    QWaylandSurface *waylandSurfaceFromItem(QQuickItem *surfaceItem) const;

    void sendCallbacks();

private:
    WindowManager *m_manager;
};
