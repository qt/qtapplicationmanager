// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <qqmlinfo.h>
#include "systemframetimerimpl.h"
#include "applicationmanagerwindow.h"
#include "frametimer.h"
#include "inprocesswindow.h"
#if defined(AM_MULTI_PROCESS)
#  include "waylandwindow.h"
#endif


QT_BEGIN_NAMESPACE_AM

SystemFrameTimerImpl::SystemFrameTimerImpl(FrameTimer *frameTimer)
    : FrameTimerImpl(frameTimer)
{ }

bool SystemFrameTimerImpl::connectToAppManWindow(QObject *window)
{
    if (!window)
        return false;

    if (auto amwin = qobject_cast<ApplicationManagerWindow *>(window)) {
        if (amwin->isInProcess()) {
            qmlWarning(frameTimer()) << "It makes no sense to measure the FPS of an application's window in single-process mode."
                                        " FrameTimer won't operate with the given window.";
            return true;
        }
    }

    if (auto *winobj = qobject_cast<Window *>(window)) {
        if (qobject_cast<InProcessWindow *>(winobj)) {
            qmlWarning(frameTimer()) << "It makes no sense to measure the FPS of a WindowObject in single-process mode."
                                        " FrameTimer won't operate with the given window.";
        }
#if defined(AM_MULTI_PROCESS)
        else if (auto *wlwin = qobject_cast<WaylandWindow *>(winobj)) {
            QObject::connect(wlwin, &WaylandWindow::waylandSurfaceChanged,
                             frameTimer(), [this, wlwin]() {
                    disconnectFromAppManWindow(nullptr);

                    m_waylandSurface = wlwin->waylandSurface();
                    if (m_waylandSurface) {
                        QObject::connect(m_waylandSurface, &QWaylandQuickSurface::redraw,
                                         frameTimer(), &FrameTimer::reportFrameSwap, Qt::UniqueConnection);
                    }
            }, Qt::UniqueConnection);
        }
#endif
        return true;
    }
    return false;
}

void SystemFrameTimerImpl::disconnectFromAppManWindow(QObject *window)
{
    Q_UNUSED(window)

#if defined(AM_MULTI_PROCESS)
    if (m_waylandSurface) {
        QObject::disconnect(m_waylandSurface, &QWaylandQuickSurface::redraw,
                            frameTimer(), &FrameTimer::reportFrameSwap);

        m_waylandSurface = nullptr;
    }
#endif
}

QT_END_NAMESPACE_AM
