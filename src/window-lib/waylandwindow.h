// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WAYLANDWINDOW_H
#define WAYLANDWINDOW_H

#include <QtAppManWindow/window.h>

#if QT_CONFIG(am_multi_process)

#include <QtWaylandCompositor/QWaylandQuickSurface>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE_AM

class WindowSurface;

class WaylandWindow : public Window
{
    Q_OBJECT
    Q_PROPERTY(QWaylandQuickSurface *waylandSurface READ waylandSurface NOTIFY waylandSurfaceChanged FINAL)
    Q_PROPERTY(QWaylandXdgSurface *waylandXdgSurface READ waylandXdgSurface NOTIFY waylandXdgSurfaceChanged FINAL)

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

Q_SIGNALS:
    void waylandSurfaceChanged();
    void waylandXdgSurfaceChanged();

private Q_SLOTS:
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

#endif // QT_CONFIG(am_multi_process)

#endif // WAYLANDWINDOW_H
