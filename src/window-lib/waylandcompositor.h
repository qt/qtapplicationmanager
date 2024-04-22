// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WAYLANDCOMPOSITOR_H
#define WAYLANDCOMPOSITOR_H

#include <QtAppManCommon/global.h>

#if QT_CONFIG(am_multi_process)

#include <QtWaylandCompositor/QWaylandQuickCompositor>

#include <QtWaylandCompositor/QWaylandQuickSurface>
#include <QtWaylandCompositor/QWaylandQuickItem>

#include <QtCore/QMap>
#include <QtCore/QPointer>

QT_FORWARD_DECLARE_CLASS(QWaylandResource)
QT_FORWARD_DECLARE_CLASS(QWaylandWlShell)
QT_FORWARD_DECLARE_CLASS(QWaylandWlShellSurface)
QT_FORWARD_DECLARE_CLASS(QWaylandTextInputManager)
QT_FORWARD_DECLARE_CLASS(QWaylandQtTextInputMethodManager)

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

Q_SIGNALS:
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
    ~WaylandCompositor() override;

    void registerOutputWindow(QQuickWindow *window);

    WaylandQtAMServerExtension *amExtension();

    void xdgPing(WindowSurface*);

Q_SIGNALS:
    void surfaceMapped(QtAM::WindowSurface *surface);

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
    QWaylandQtTextInputMethodManager *m_qtTextInputMethodManager;
    QWaylandTextInputManager *m_textInputManager;
    QMap<uint, QPointer<WindowSurface>> m_xdgPingMap;
};

QT_END_NAMESPACE_AM

#endif // QT_CONFIG(am_multi_process)

#endif // WAYLANDCOMPOSITOR_H
