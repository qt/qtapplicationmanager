/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include "frametimer.h"

QT_BEGIN_NAMESPACE_AM

const qreal FrameTimer::MicrosInSec = qreal(1000 * 1000);

FrameTimer::FrameTimer()
{ }

void FrameTimer::newFrame()
{
    int frameTime = m_timer.isValid() ? qMax(1, int(m_timer.nsecsElapsed() / 1000)) : IdealFrameTime;
    m_timer.restart();

    m_count++;
    m_sum += frameTime;
    m_min = qMin(m_min, frameTime);
    m_max = qMax(m_max, frameTime);
    m_jitter += qAbs(MicrosInSec / IdealFrameTime - MicrosInSec / frameTime);
}

void FrameTimer::reset()
{
    m_count = m_sum = m_max = 0;
    m_jitter = 0;
    m_min = std::numeric_limits<int>::max();
}

qreal FrameTimer::averageFps() const
{
    return m_sum ? MicrosInSec * m_count / m_sum : qreal(0);
}

qreal FrameTimer::minimumFps() const
{
    return m_max ? MicrosInSec / m_max : qreal(0);
}

qreal FrameTimer::maximumFps() const
{
    return m_min ? MicrosInSec / m_min : qreal(0);
}

qreal FrameTimer::jitterFps() const
{
    return m_count ? m_jitter / m_count :  qreal(0);
}

QT_END_NAMESPACE_AM
