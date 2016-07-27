/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QSysInfo>
#include <QThread>
#include <QFile>
#include <QHash>
#include <QTimerEvent>
#include <QElapsedTimer>
#include <vector>
#include <QGuiApplication>

#include "systemmonitor.h"
#include "systemmonitor_p.h"
#include "processmonitor.h"

#include "global.h"

#include "qml-utilities.h"

namespace {
enum Roles
{
    CpuLoad = Qt::UserRole + 5000,
    MemoryUsed,
    MemoryTotal,
    IoLoad,

    AverageFps = Qt::UserRole + 6000,
    MinimumFps,
    MaximumFps,
    FpsJitter
};
}
class FrameTimer
{
public:
    FrameTimer()
    { }

    void newFrame()
    {
        int frameTime = m_timer.isValid() ? qMax(1, int(m_timer.nsecsElapsed() / 1000)) : IdealFrameTime;
        m_timer.restart();

        m_count++;
        m_sum += frameTime;
        m_min = qMin(m_min, frameTime);
        m_max = qMax(m_max, frameTime);
        m_jitter += qAbs(frameTime - IdealFrameTime);
    }

    inline void reset()
    {
        m_count = m_sum = m_max = m_jitter = 0;
        m_min = INT_MAX;
    }

    inline qreal averageFps() const
    {
        return m_sum ? qreal(1000000) * m_count / m_sum : qreal(0);
    }

    inline qreal minimumFps() const
    {
        return m_max ? qreal(1000000) / m_max : qreal(0);
    }

    inline qreal maximumFps() const
    {
        return m_min ? qreal(1000000) / m_min : qreal(0);
    }

    inline qreal jitterFps() const
    {
        return m_jitter ? qreal(1000000) * m_count / m_jitter : qreal(0);

    }

private:
    int m_count = 0;
    int m_sum = 0;
    int m_min = INT_MAX;
    int m_max = 0;
    int m_jitter = 0;

    QElapsedTimer m_timer;

    static const int IdealFrameTime = 16666; // usec - could be made configurable via an env variable
};


class SystemMonitorPrivate : public QObject
{
public:
    SystemMonitorPrivate(SystemMonitor *q)
        : q_ptr(q)
    { }

    SystemMonitor *q_ptr;
    Q_DECLARE_PUBLIC(SystemMonitor)

    // idle
    qreal idleAverage = 0.1;
    CpuReader *idleCpu = 0;
    int idleTimerId = 0;
    bool isIdle = false;

    // memory thresholds
    qreal memoryLowWarning = -1;
    qreal memoryCriticalWarning = -1;
    bool hasMemoryLowWarning = false;
    bool hasMemoryCriticalWarning = false;
    MemoryThreshold *memoryThreshold = 0;

    // fps
    QHash<QObject *, FrameTimer *> frameTimer;

    QList<ProcessMonitor*> processMonitors;

    // reporting
    MemoryReader *memory = 0;
    CpuReader *cpu = 0;
    QHash<QString, IoReader *> ioHash;
    int reportingInterval = -1;
    int reportingRange = 100 * 100;
    int reportingTimerId = 0;
    bool reportCpu = false;
    bool reportMem = false;
    bool reportFps = false;

    struct Report
    {
        qreal cpuLoad = 0;
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

    ProcessMonitor *getProcess(const QString &appId)
    {
        Q_Q(SystemMonitor);

        bool singleProcess = qApp->property("singleProcessMode").toBool();
        QString usedAppId;

        if (singleProcess)
            usedAppId = "";
        else {
            usedAppId = appId;
            // Avoid creating multiple objects for aliases
            QStringList aliasBreak = appId.split('@');
            usedAppId = aliasBreak[0];
        }

        for (int i = 0; i < processMonitors.size(); i++) {
            if (processMonitors.at(i)->getAppId() == usedAppId)
                return processMonitors.at(i);
        }

        ProcessMonitor *p = new ProcessMonitor(usedAppId, q);
        processMonitors.append(p);
        return processMonitors.last();
    }

    void setupTimer(int newInterval = -1)
    {
        bool useNewInterval = (newInterval != -1) && (newInterval != reportingInterval);
        bool shouldBeOn = reportCpu | reportMem | reportFps | !ioHash.isEmpty();

        if (useNewInterval)
            reportingInterval = newInterval;

        if (!shouldBeOn) {
            if (reportingTimerId) {
                killTimer(reportingTimerId);
                reportingTimerId = 0;
            }
        } else {
            if (reportingTimerId) {
                if (useNewInterval) {
                    killTimer(reportingTimerId);
                    reportingTimerId = 0;
                }
            }
            if (!reportingTimerId)
                reportingTimerId = startTimer(reportingInterval);
        }
    }

    void timerEvent(QTimerEvent *te)
    {
        Q_Q(SystemMonitor);

        if (te && te->timerId() == reportingTimerId) {
            Report r;
            QVector<int> roles;
            for (int i = 0; i < processMonitors.size(); i++)
                processMonitors.at(i)->readData();

            if (reportCpu) {
                QPair<int, qreal> cpuVal = cpu->readLoadValue();
                emit q->cpuLoadReportingChanged(cpuVal.first, cpuVal.second);
                r.cpuLoad = cpuVal.second;
                roles.append(CpuLoad);
            }
            if (reportMem) {
                quint64 memVal = memory->readUsedValue();
                emit q->memoryReportingChanged(memory->totalValue(), memVal);
                r.memoryUsed = memVal;
                roles.append(MemoryUsed);
                roles.append(MemoryTotal);
            }
            for (auto it = ioHash.cbegin(); it != ioHash.cend(); ++it) {
                QPair<int, qreal> ioVal = it.value()->readLoadValue();
                emit q->ioLoadReportingChanged(it.key(), ioVal.first, ioVal.second);
                r.ioLoad.insert(it.key(), ioVal.second);
            }
            if (!ioHash.isEmpty())
                roles.append(IoLoad);

            if (reportFps) {
                if (FrameTimer *ft = frameTimer.value(nullptr)) {
                    r.fpsAvg = ft->averageFps();
                    r.fpsMin = ft->minimumFps();
                    r.fpsMax = ft->maximumFps();
                    r.fpsJitter = ft->jitterFps();
                    ft->reset();
                    emit q->fpsReportingChanged(r.fpsAvg, r.fpsMin, r.fpsMax, r.fpsJitter);
                    roles.append(AverageFps);
                    roles.append(MinimumFps);
                    roles.append(MaximumFps);
                    roles.append(FpsJitter);
                }
            }

            // ring buffer handling
            // optimization: instead of sending a dataChanged for every item, we always move the
            // first item to the end and change its data only
            int size = reports.size();
            q->beginMoveRows(QModelIndex(), 0, 0, QModelIndex(), size);
            reports[reportPos++] = r;
            if (reportPos >= reports.size())
                reportPos = 0;
            q->endMoveRows();
            q->dataChanged(q->index(size - 1), q->index(size - 1), roles);
        } else if (te && te->timerId() == idleTimerId) {
            QPair<int, qreal> idleVal = idleCpu->readLoadValue();
            bool nowIdle = (idleVal.second <= idleAverage);
            if (nowIdle != isIdle)
                emit q->idleChanged(nowIdle);
            isIdle = nowIdle;
        }
    }

    const Report &reportForRow(int row) const
    {
        // convert a visual row position to an index into the internal ringbuffer

        int pos = row + reportPos;
        if (pos >= reports.size())
            pos -= reports.size();

        if (pos < 0 || pos >= reports.size())
            return reports.first();
        return reports.at(pos);
    }

    void updateModel()
    {
        Q_Q(SystemMonitor);

        q->beginResetModel();
        // we need at least 2 items, otherwise we cannot move rows
        reports.resize(qMax(2, reportingRange / reportingInterval));
        reportPos = 0;
        q->endResetModel();
    }
};


SystemMonitor *SystemMonitor::s_instance = 0;


SystemMonitor *SystemMonitor::createInstance()
{
    if (Q_UNLIKELY(s_instance))
        qFatal("SystemMonitor::createInstance() was called a second time.");

    qmlRegisterSingletonType<SystemMonitor>("QtApplicationManager", 1, 0, "SystemMonitor",
                                                 &SystemMonitor::instanceForQml);
    return s_instance = new SystemMonitor();
}

SystemMonitor *SystemMonitor::instance()
{
    if (!s_instance)
        qFatal("SystemMonitor::instance() was called before createInstance().");
    return s_instance;
}

QObject *SystemMonitor::instanceForQml(QQmlEngine *qmlEngine, QJSEngine *)
{
    if (qmlEngine)
        retakeSingletonOwnershipFromQmlEngine(qmlEngine, instance());
    return instance();
}

SystemMonitor::SystemMonitor()
    : d_ptr(new SystemMonitorPrivate(this))
{
    Q_D(SystemMonitor);

    d->idleCpu = new CpuReader;
    d->cpu = new CpuReader;
    d->memory = new MemoryReader;

    d->idleTimerId = d->startTimer(1000);

    connect(this, &QAbstractItemModel::rowsInserted, this, &SystemMonitor::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &SystemMonitor::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &SystemMonitor::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &SystemMonitor::countChanged);

    d->roleNames.insert(CpuLoad, "cpuLoad");
    d->roleNames.insert(MemoryUsed, "memoryUsed");
    d->roleNames.insert(MemoryTotal, "memoryTotal");
    d->roleNames.insert(IoLoad, "ioLoad");
    d->roleNames.insert(AverageFps, "averageFps");
    d->roleNames.insert(MinimumFps, "minimumFps");
    d->roleNames.insert(MaximumFps, "maximumFps");
    d->roleNames.insert(FpsJitter, "fpsJitter");

    d->updateModel();
}

SystemMonitor::~SystemMonitor()
{
    Q_D(SystemMonitor);

    delete d->idleCpu;
    delete d->memory;
    delete d->cpu;
    qDeleteAll(d->ioHash);
    delete d->memoryThreshold;
    delete d;
}

int SystemMonitor::rowCount(const QModelIndex &parent) const
{
    Q_D(const SystemMonitor);

    if (parent.isValid())
        return 0;
    return d->reports.count();
}

QVariant SystemMonitor::data(const QModelIndex &index, int role) const
{
    Q_D(const SystemMonitor);

    if (index.parent().isValid() || !index.isValid() || index.row() < 0 || index.row() >= d->reports.size())
        return QVariant();

    const SystemMonitorPrivate::Report &r = d->reportForRow(index.row());

    switch (role) {
    case CpuLoad:
        return r.cpuLoad;
    case MemoryUsed:
        return r.memoryUsed;
    case MemoryTotal:
        return totalMemory();
    case IoLoad:
        return r.ioLoad;
    case AverageFps:
        return r.fpsAvg;
    case MinimumFps:
        return r.fpsMin;
    case MaximumFps:
        return r.fpsMax;
    case FpsJitter:
        return r.fpsJitter;
    }
    return QVariant();
}

QHash<int, QByteArray> SystemMonitor::roleNames() const
{
    Q_D(const SystemMonitor);

    return d->roleNames;
}

QVariantMap SystemMonitor::get(int row) const
{
    if (row < 0 || row >= count()) {
        qCWarning(LogSystem) << "invalid row:" << row;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        map.insert(it.value(), data(index(row), it.key()));
    return map;
}


quint64 SystemMonitor::totalMemory() const
{
    Q_D(const SystemMonitor);

    return d->memory->totalValue();
}

int SystemMonitor::cpuCores() const
{
    return QThread::idealThreadCount();
}

bool SystemMonitor::setMemoryWarningThresholds(qreal lowWarning, qreal criticalWarning)
{
    Q_D(SystemMonitor);

    if (lowWarning != d->memoryLowWarning || criticalWarning != d->memoryCriticalWarning) {
        delete d->memoryThreshold;
        d->memoryLowWarning = lowWarning;
        d->memoryCriticalWarning = criticalWarning;
        QList<qreal> thresholds { lowWarning, criticalWarning };
        d->memoryThreshold = new MemoryThreshold(thresholds);

        connect(d->memoryThreshold, &MemoryThreshold::thresholdTriggered, this, [this, d]() {
            quint64 memTotal = d->memory->totalValue();
            quint64 memUsed = d->memory->readUsedValue();

            qreal factor = memUsed / memTotal;
            bool nowMemoryCritical = (factor > d->memoryCriticalWarning);
            bool nowMemoryLow = (factor > d->memoryLowWarning);
            if (nowMemoryCritical && !d->hasMemoryCriticalWarning)
                emit memoryCriticalWarning();
            if (nowMemoryLow && !d->hasMemoryLowWarning)
                emit memoryLowWarning();
            d->hasMemoryCriticalWarning = nowMemoryCritical;
            d->hasMemoryLowWarning = nowMemoryLow;
        });

        return d->memoryThreshold->setEnabled(true);
    }
    return true;
}

qreal SystemMonitor::memoryLowWarningThreshold() const
{
    Q_D(const SystemMonitor);

    return d->memoryLowWarning;
}

qreal SystemMonitor::memoryCriticalWarningThreshold() const
{
    Q_D(const SystemMonitor);

    return d->memoryCriticalWarning;
}

void SystemMonitor::setIdleLoadAverage(qreal loadAverage)
{
    Q_D(SystemMonitor);

    if (loadAverage != d->idleAverage) {
        d->idleAverage = loadAverage;
    }
}

qreal SystemMonitor::idleLoadAverage() const
{
    Q_D(const SystemMonitor);

    return d->idleAverage;
}

bool SystemMonitor::isIdle() const
{
    Q_D(const SystemMonitor);

    return d->isIdle;
}

void SystemMonitor::setMemoryReportingEnabled(bool enabled)
{
    Q_D(SystemMonitor);

    if (enabled != d->reportMem) {
        d->reportMem = enabled;
        d->setupTimer();
        emit memoryReportingEnabledChanged();
    }
}

bool SystemMonitor::isMemoryReportingEnabled() const
{
    Q_D(const SystemMonitor);

    return d->reportMem;
}

void SystemMonitor::setCpuLoadReportingEnabled(bool enabled)
{
    Q_D(SystemMonitor);

    if (enabled != d->reportCpu) {
        d->reportCpu = enabled;
        d->setupTimer();
        emit cpuLoadReportingEnabledChanged();
    }
}

bool SystemMonitor::isCpuLoadReportingEnabled() const
{
    Q_D(const SystemMonitor);

    return d->reportCpu;
}

bool SystemMonitor::addIoLoadReporting(const QString &deviceName)
{
    Q_D(SystemMonitor);

    if (!QFile::exists(qSL("/dev/") + deviceName))
        return false;
    if (d->ioHash.contains(deviceName))
        return false;

    IoReader *ior = new IoReader(deviceName.toLocal8Bit().constData());
    d->ioHash.insert(deviceName, ior);
    d->setupTimer();
    return true;
}

void SystemMonitor::removeIoLoadReporting(const QString &deviceName)
{
    Q_D(SystemMonitor);

    delete d->ioHash.take(deviceName);
    d->setupTimer();
}

QStringList SystemMonitor::ioLoadReportingDevices() const
{
    Q_D(const SystemMonitor);

    return d->ioHash.keys();
}

void SystemMonitor::setFpsReportingEnabled(bool enabled)
{
    Q_D(SystemMonitor);

    if (enabled != d->reportFps) {
        d->reportFps = enabled;
        d->setupTimer();
        emit fpsReportingEnabledChanged();
    }
}

bool SystemMonitor::isFpsReportingEnabled() const
{
    Q_D(const SystemMonitor);

    return d->reportFps;
}

void SystemMonitor::setReportingInterval(int intervalInMSec)
{
    Q_D(SystemMonitor);

    if (d->reportingInterval != intervalInMSec && intervalInMSec > 0) {
        d->setupTimer(intervalInMSec);
        d->updateModel();
    }
}

int SystemMonitor::reportingInterval() const
{
    Q_D(const SystemMonitor);

    return d->reportingInterval;
}

void SystemMonitor::setReportingRange(int rangeInMSec)
{
    Q_D(SystemMonitor);

    if (d->reportingRange != rangeInMSec && rangeInMSec > 0) {
        d->reportingRange = rangeInMSec;
        d->updateModel();
    }
}

int SystemMonitor::reportingRange() const
{
    Q_D(const SystemMonitor);

    return d->reportingRange;
}

/*! \internal
    report a frame swap for any window. \a item is \c 0 for the system-ui
*/
void SystemMonitor::reportFrameSwap(QObject *item)
{
    Q_D(SystemMonitor);

    if (!d->reportFps)
        return;

    FrameTimer *frameTimer = d->frameTimer.value(item);
    if (!frameTimer) {
        frameTimer = new FrameTimer();
        d->frameTimer.insert(item, frameTimer);
        if (item)
            connect(item, &QObject::destroyed, this, [d](QObject *o) { delete d->frameTimer.take(o); });
    }

    frameTimer->newFrame();
}

QObject *SystemMonitor::getProcessMonitor(const QString &appId)
{
    Q_D(SystemMonitor);
    return d->getProcess(appId);
}
