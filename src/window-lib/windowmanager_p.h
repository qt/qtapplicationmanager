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

#include <QVector>
#include <QMap>
#include <QHash>

#include <QtAppManWindow/windowmanager.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)

QT_BEGIN_NAMESPACE_AM

class WindowManagerPrivate
{
public:
    int findWindowBySurfaceItem(QQuickItem *quickItem) const;

#if defined(AM_MULTI_PROCESS)
    int findWindowByWaylandSurface(QWaylandSurface *waylandSurface) const;

    WaylandCompositor *waylandCompositor = nullptr;
    QVector<int> extraWaylandSockets;

    static QString applicationId(Application *app, WindowSurface *windowSurface);
#endif

    QHash<int, QByteArray> roleNames;

    // All windows, regardless of content state, that haven't been released (hence destroyed) yet.
    QVector<Window *> allWindows;

    // Windows that are actually exposed by the model to the QML side.
    // Only windows whose content state is different than Window::NoSurface are
    // kept here.
    QVector<Window *> windowsInModel;

    bool shuttingDown = false;
    bool slowAnimations = false;

    QList<QQuickWindow *> views;
    QString waylandSocketName;
    QQmlEngine *qmlEngine;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
