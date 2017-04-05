/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the application manager API. It exists purely as an
// implementation detail. This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include <QObject>
#include <QElapsedTimer>
#include <QThread>
#include <QMutex>
#include <QString>
#include <QHash>
#include <QVector>
#include <QQuickWindow>
#if defined(Q_OS_LINUX)
#    include <QScopedPointer>
#    include "sysfsreader.h"
#endif
#if defined(AM_MULTI_PROCESS)
#    include <QtAppManWindow/waylandwindow.h>
#endif
#include "processmonitor.h"
#include "frametimer.h"
#include "applicationmanager.h"

QT_BEGIN_NAMESPACE_AM

class Window;

class ReadingTask : public QObject
{
    Q_OBJECT

public:
    struct Results {
        int sync;

        struct Memory {
            bool read = false;
            quint32 totalVm = 0;
            quint32 totalRss = 0;
            quint32 totalPss = 0;
            quint32 textVm = 0;
            quint32 textRss = 0;
            quint32 textPss = 0;
            quint32 heapVm = 0;
            quint32 heapRss = 0;
            quint32 heapPss = 0;
        } memory;

        struct Cpu {
            bool read = false;
            qreal load = 0;
        } cpu;
    };

    ReadingTask(QMutex &mutex, Results &res);

protected:
    void timerEvent(QTimerEvent *event) override;

public slots:
    void setupTimer(bool enabled, int interval);
    void setNewPid(qint64 pid);
    void setNewReadMem(bool enabled);
    void setNewReadCpu(bool enabled);
    void reset(int sync);

    void aboutToQuit();

signals:
    void newReadingAvailable();

private:
    void cancelTimer();
    void openLoad();
    qreal readLoad();
    bool readMemory(Results::Memory &results);

    QMutex &m_mutex;
    Results &m_results;
    int m_sync = 0;

#if defined(Q_OS_LINUX)
    QScopedPointer<SysFsReader> m_statReader;
#endif
    QElapsedTimer m_elapsedTime;
    quint64 m_lastCpuUsage;

    qint64 m_pid;

    bool m_readCpu = false;
    bool m_readMem = false;

    int m_reportingInterval = -1;
    int m_reportingTimerId = 0;
};


namespace {
enum Roles {
    MemVirtual = Qt::UserRole,
    MemRss,
    MemPss,
    CpuLoad,
    FrameRate
};
}

class ProcessMonitorPrivate : public QObject
{
    Q_OBJECT
public:
    ProcessMonitor *q_ptr;
    Q_DECLARE_PUBLIC(ProcessMonitor)

    struct ModelData {
        QVariantMap vm;
        QVariantMap rss;
        QVariantMap pss;
        qreal cpuLoad = 0;
        QVariantList frameRate;
    };

    QString appId;
    qint64 pid = 0;

    int reportingInterval = -1;
    int count = 10;

    int reportPos = 0;

    bool reportMemory = false;
    bool reportCpu = false;
    bool reportFps = false;
    int cpuTail = 0;
    int memTail = 0;
    int fpsTail = 0;

    QHash<int, QByteArray> roles;
    QVector<ModelData> modelData;

    // fps
    QMap<QObject *, FrameTimer *> frameCounters;

    ReadingTask readingTask;
    ReadingTask::Results readResults;
    QThread thread;
    QMutex mutex;
    int sync = 0;

    QList<QMetaObject::Connection> connections;

    ProcessMonitorPrivate(ProcessMonitor *q);
    ~ProcessMonitorPrivate();

    void setupInterval(int interval = -1);
    void determinePid();
    const ModelData &modelDataForRow(int row) const;
    void resetModel();
    void updateModelCount(int newCount);
    void setupFrameRateMonitoring();
    void updateMonitoredWindows(const QList<QObject *> &windows);
    QList<QObject *> monitoredWindows() const;
    void resetFrameRateMonitoring();
    void clearMonitoredWindows();

signals:
    void setupTimer(bool enabled, int interval);
    void newPid(qint64 pid);
    void newReadMem(bool enabled);
    void newReadCpu(bool enabled);
    void newCount(int count);
    void reset(int sync);

public slots:
    void readingUpdate();
    void appRuntimeChanged(const QString &id, ApplicationManager::RunState state);
#if defined(AM_MULTI_PROCESS)
    void applicationWindowClosing(int index, QQuickItem *window);
#endif
    void frameUpdated();
};


QT_END_NAMESPACE_AM
