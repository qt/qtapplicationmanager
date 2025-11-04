// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "frametimerimpl.h"
#include "frametimer.h"


QT_BEGIN_NAMESPACE_AM

std::function<FrameTimerImpl *(FrameTimer *)> FrameTimerImpl::s_factory;

FrameTimerImpl::FrameTimerImpl(FrameTimer *frameTimer)
    : m_frameTimer(frameTimer)
{ }

void FrameTimerImpl::setFactory(const std::function<FrameTimerImpl *(FrameTimer *)> &factory)
{
    s_factory = factory;
}

FrameTimerImpl *FrameTimerImpl::create(FrameTimer *window)
{
    return s_factory ? s_factory(window) : nullptr;
}

FrameTimer *FrameTimerImpl::frameTimer()
{
    return m_frameTimer;
}

void FrameTimerImpl::reportFrameSwap()
{

}

QT_END_NAMESPACE_AM
