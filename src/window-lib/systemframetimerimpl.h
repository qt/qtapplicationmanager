// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QPointer>
#include <QtAppManSharedMain/frametimerimpl.h>
#include <QtAppManCommon/global.h>


QT_FORWARD_DECLARE_CLASS(QWaylandQuickSurface)

QT_BEGIN_NAMESPACE_AM

class SystemFrameTimerImpl : public FrameTimerImpl
{
public:
    SystemFrameTimerImpl(FrameTimer *frameTimer);

    bool connectToSystemWindow(QObject *window) override;
    void disconnectFromSystemWindow(QObject *window) override;

private:
#if defined(AM_MULTI_PROCESS)
    QMetaObject::Connection m_surfaceChangeConnection;
    QMetaObject::Connection m_redrawConnection;
#endif
};

QT_END_NAMESPACE_AM
