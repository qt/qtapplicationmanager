// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <qqmlinfo.h>
#include "systemframetimerimpl.h"
#include "applicationmanagerwindow.h"
#include "frametimer.h"
#include "inprocesswindow.h"
#if QT_CONFIG(am_multi_process)
#  include "waylandwindow.h"
#endif


QT_BEGIN_NAMESPACE_AM

SystemFrameTimerImpl::SystemFrameTimerImpl(FrameTimer *frameTimer)
    : FrameTimerImpl(frameTimer)
{ }

bool SystemFrameTimerImpl::connectToSystemWindow(QObject *window)
{
    if (qobject_cast<InProcessWindow *>(window)) {
        qmlWarning(frameTimer()) << "It makes no sense to measure the FPS of a WindowObject in single-process mode."
                                    " FrameTimer won't operate with the given window.";
        return true;
    }
#if QT_CONFIG(am_multi_process)
    if (auto *wlwin = qobject_cast<WaylandWindow *>(window)) {
        auto connectToWaylandSurface = [this](WaylandWindow *waylandWindow) {
            if (m_redrawConnection) {
                QObject::disconnect(m_redrawConnection);
                m_redrawConnection = { };
            }

            if (auto surface = waylandWindow ? waylandWindow->waylandSurface() : nullptr) {
                m_redrawConnection = QObject::connect(surface, &QWaylandQuickSurface::redraw,
                                                      frameTimer(), &FrameTimer::reportFrameSwap);
            }
        };

        m_surfaceChangeConnection = QObject::connect(wlwin, &WaylandWindow::waylandSurfaceChanged,
                                                     frameTimer(), [=]() {
            connectToWaylandSurface(wlwin);
        });
        connectToWaylandSurface(wlwin);
        return true;
    }
#endif
    return false;
}

void SystemFrameTimerImpl::disconnectFromSystemWindow(QObject *window)
{
    Q_UNUSED(window)
#if QT_CONFIG(am_multi_process)
    if (m_redrawConnection) {
        QObject::disconnect(m_redrawConnection);
        m_redrawConnection = { };
    }
    if (m_surfaceChangeConnection) {
        QObject::disconnect(m_surfaceChangeConnection);
        m_surfaceChangeConnection = { };
    }
#endif
}

QT_END_NAMESPACE_AM
