/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

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
