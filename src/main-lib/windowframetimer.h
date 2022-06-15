// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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
