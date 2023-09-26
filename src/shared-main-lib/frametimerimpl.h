// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <functional>
#include <QtAppManCommon/global.h>


QT_BEGIN_NAMESPACE_AM

class FrameTimer;

class FrameTimerImpl
{
public:
    static void setFactory(const std::function<FrameTimerImpl *(FrameTimer *)> &factory);

    static FrameTimerImpl *create(FrameTimer *timer);

    virtual ~FrameTimerImpl() = default;

    FrameTimer *frameTimer();

    virtual bool connectToAppManWindow(QObject *window) = 0;
    virtual void disconnectFromAppManWindow(QObject *window) = 0;

    void reportFrameSwap();

protected:
    FrameTimerImpl(FrameTimer *frameTimer);

    FrameTimer *m_frameTimer = nullptr;

    static std::function<FrameTimerImpl *(FrameTimer *)> s_factory;
    Q_DISABLE_COPY_MOVE(FrameTimerImpl)
};

QT_END_NAMESPACE_AM
