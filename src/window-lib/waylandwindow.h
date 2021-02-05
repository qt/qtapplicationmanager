/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
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

#include <QtAppManWindow/window.h>

#if defined(AM_MULTI_PROCESS)

#include <QWaylandQuickSurface>
#include <QWaylandXdgShell>
#include <QTimer>

QT_BEGIN_NAMESPACE_AM

class WindowSurface;

class WaylandWindow : public Window
{
    Q_OBJECT
    Q_PROPERTY(QWaylandQuickSurface *waylandSurface READ waylandSurface NOTIFY waylandSurfaceChanged)
    Q_PROPERTY(QWaylandXdgSurface *waylandXdgSurface READ waylandXdgSurface NOTIFY waylandXdgSurfaceChanged)

public:
    WaylandWindow(Application *app, WindowSurface *surface);

    bool isInProcess() const override { return false; }

    bool isPopup() const override;
    QPoint requestedPopupPosition() const override;

    bool setWindowProperty(const QString &name, const QVariant &value) override;
    QVariant windowProperty(const QString &name) const override;
    QVariantMap windowProperties() const override;

    ContentState contentState() const override;

    void close() override;

    QSize size() const override;
    void resize(const QSize &size) override;

    WindowSurface *surface() const { return m_surface; }

    QWaylandQuickSurface *waylandSurface() const;
    QWaylandXdgSurface *waylandXdgSurface() const;

    static bool m_watchdogEnabled;

signals:
    void waylandSurfaceChanged();
    void waylandXdgSurfaceChanged();

private slots:
    void pongReceived();
    void pongTimeout();
    void pingTimeout();
    void onContentStateChanged();

private:
    QString applicationId() const;

    void enableOrDisablePing();
    QTimer *m_pingTimer;
    QTimer *m_pongTimer;
    WindowSurface *m_surface;
    QVariantMap m_windowProperties;
};

QT_END_NAMESPACE_AM

#endif // AM_MULTI_PROCESS
