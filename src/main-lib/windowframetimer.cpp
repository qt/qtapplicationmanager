/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
