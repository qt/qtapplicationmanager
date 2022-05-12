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

#include "windowframetimer.h"

#include <qqmlinfo.h>

#include "waylandwindow.h"
#include "inprocesswindow.h"
#include "qmlinprocessapplicationmanagerwindow.h"


QT_BEGIN_NAMESPACE_AM

WindowFrameTimer::WindowFrameTimer(QObject *parent)
    : FrameTimer(parent)
{ }

bool WindowFrameTimer::connectToAppManWindow()
{
    if (qobject_cast<QmlInProcessApplicationManagerWindow *>(m_window.data())) {
        qmlWarning(this) << "It makes no sense to measure the FPS of an application's window in single-process mode."
                            " FrameTimer won't operate with the given window.";
        return true;
    }

    Window *appManWindow = qobject_cast<Window*>(m_window.data());
    if (!appManWindow)
        return false;

    if (qobject_cast<InProcessWindow*>(appManWindow)) {
        qmlWarning(this) << "It makes no sense to measure the FPS of a WindowObject in single-process mode."
                            " FrameTimer won't operate with the given window.";
        return true;
    }

#if defined(AM_MULTI_PROCESS)
    WaylandWindow *waylandWindow = qobject_cast<WaylandWindow*>(m_window);
    Q_ASSERT(waylandWindow);

    connect(waylandWindow, &WaylandWindow::waylandSurfaceChanged,
            this, &WindowFrameTimer::connectToWaylandSurface, Qt::UniqueConnection);

    connectToWaylandSurface();
#endif

    return true;
}

void WindowFrameTimer::disconnectFromAppManWindow()
{
#if defined(AM_MULTI_PROCESS)
    disconnectFromWaylandSurface();
#endif
}

#if defined(AM_MULTI_PROCESS)
void WindowFrameTimer::connectToWaylandSurface()
{
    WaylandWindow *waylandWindow = qobject_cast<WaylandWindow*>(m_window);
    Q_ASSERT(waylandWindow);

    disconnectFromWaylandSurface();

    m_waylandSurface = waylandWindow->waylandSurface();
    if (m_waylandSurface)
        connect(m_waylandSurface, &QWaylandQuickSurface::redraw, this, &WindowFrameTimer::newFrame, Qt::UniqueConnection);
}

void WindowFrameTimer::disconnectFromWaylandSurface()
{
    if (!m_waylandSurface)
        return;

    disconnect(m_waylandSurface, nullptr, this, nullptr);

    m_waylandSurface = nullptr;
}
#endif


QT_END_NAMESPACE_AM

#include "moc_windowframetimer.cpp"
