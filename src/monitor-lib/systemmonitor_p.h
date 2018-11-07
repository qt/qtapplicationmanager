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

#pragma once

#include <QObject>
#include <QHash>

#include "frametimer.h"
#include "global.h"
#include "systemreader.h"

QT_FORWARD_DECLARE_CLASS(QQuickWindow)

QT_BEGIN_NAMESPACE_AM

class SystemMonitor;

class SystemMonitorPrivate : public QObject // clazy:exclude=missing-qobject-macro
{
public:
    SystemMonitorPrivate(SystemMonitor *q)
        : q_ptr(q)
    { }

    static const SystemMonitorPrivate* get(const SystemMonitor *sysMon) { return sysMon->d_func(); }

    SystemMonitor *q_ptr;
    Q_DECLARE_PUBLIC(SystemMonitor)

    // idle
    qreal idleThreshold = 0.1;
    CpuReader *idleCpu = nullptr;
    int idleTimerId = 0;
    bool isIdle = false;

    // memory thresholds
    qreal memoryLowWarning = -1;
    qreal memoryCriticalWarning = -1;
    MemoryWatcher *memoryWatcher = nullptr;

    // fps
    QHash<QObject *, FrameTimer *> frameTimer;

    // reporting
    MemoryReader *memory = nullptr;
    CpuReader *cpu = nullptr;
    GpuReader *gpu = nullptr;
    QHash<QString, IoReader *> ioHash;
    int reportingInterval = 1000;
    int count = 10;
    int reportingTimerId = 0;
    bool reportCpu = false;
    bool reportGpu = false;
    bool reportMem = false;
    bool reportFps = false;
    bool windowManagerConnectionCreated = false;

    struct Report
    {
        qreal cpuLoad = 0;
        qreal gpuLoad = 0;
        qreal fpsAvg = 0;
        qreal fpsMin = 0;
        qreal fpsMax = 0;
        qreal fpsJitter = 0;
        quint64 memoryUsed = 0;
        QVariantMap ioLoad;
    };
    QVector<Report> reports;
    int reportPos = 0;

    // model
    QHash<int, QByteArray> roleNames;

    void makeNewReport();

    int latestReportPos() const;

#if !defined(AM_HEADLESS)
    void registerNewView(QQuickWindow *view);
#endif

    void setupFpsReporting();
    void setupTimer();

    void timerEvent(QTimerEvent *te) override;

    const Report &reportForRow(int row) const;

    void updateModel(bool clear);

    void setReportingInterval(int intervalInMSec);
};

QT_END_NAMESPACE_AM
