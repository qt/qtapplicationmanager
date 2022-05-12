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

#include <QtAppManSharedMain/frametimer.h>
#include <QtAppManCommon/global.h>

#if defined(AM_MULTI_PROCESS)
#  include <QtWaylandCompositor/QWaylandQuickSurface>
#endif

QT_BEGIN_NAMESPACE_AM

class WindowFrameTimer : public FrameTimer
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/FrameTimer 2.0")

public:
    WindowFrameTimer(QObject *parent = nullptr);

protected:
    bool connectToAppManWindow() override;
    void disconnectFromAppManWindow() override;

private:
#if defined(AM_MULTI_PROCESS)
    void disconnectFromWaylandSurface();
    void connectToWaylandSurface();
    QPointer<QWaylandQuickSurface> m_waylandSurface;
#endif
};

QT_END_NAMESPACE_AM
