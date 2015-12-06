/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QSysInfo>
#include <QThread>
#include <QFile>
#include <QHash>
#include <QTimerEvent>

#include "systemmonitor.h"
#if defined(Q_OS_LINUX)
#  include "systemmonitor_linux.h"
#else
#  include "systemmonitor_dummy.h"
#endif

#include "global.h"

#include "qml-utilities.h"

namespace {
enum Roles
{
    CpuLoad = Qt::UserRole + 5000,
    MemoryUsed,
    MemoryTotal,
    IoLoad
};
}

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

    // reporting
    MemoryReader *memory = 0;
    CpuReader *cpu = 0;
    QHash<QString, IoReader *> ioHash;
    int reportingInterval = 100;
    int reportingRange = 100 * 100;
    int reportingTimerId = 0;
    bool reportCpu = false;
    bool reportMem = false;

    struct Report
    {
        qreal cpuLoad = 0;
        quint64 memoryUsed = 0;
        QVariantMap ioLoad;
    };
    QVector<Report> reports;
    int reportPos = 0;

    // model
    QHash<int, QByteArray> roleNames;

    void setupTimer(int newInterval = -1)
    {
        bool useNewInterval = (newInterval != -1) && (newInterval != reportingInterval);
        bool shouldBeOn = reportCpu | reportMem | !ioHash.isEmpty();

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
        q->endResetModel();
    }
};


SystemMonitor *SystemMonitor::s_instance = 0;


SystemMonitor *SystemMonitor::createInstance()
{
    if (s_instance)
        qFatal("SystemMonitor::createInstance() was called a second time.");
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
    d->idleCpu->open();
    d->cpu = new CpuReader;
    d->cpu->open();
    d->memory = new MemoryReader;
    d->memory->open();

    d->idleTimerId = d->startTimer(1000);

    connect(this, &QAbstractItemModel::rowsInserted, this, &SystemMonitor::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &SystemMonitor::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &SystemMonitor::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &SystemMonitor::countChanged);

    d->roleNames.insert(CpuLoad, "cpuLoad");
    d->roleNames.insert(MemoryUsed, "memoryUsed");
    d->roleNames.insert(MemoryTotal, "memoryTotal");
    d->roleNames.insert(IoLoad, "ioLoad");

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

void SystemMonitor::enableMemoryReporting(bool enabled)
{
    Q_D(SystemMonitor);

    if (enabled != d->reportMem) {
        d->reportMem = enabled;
        d->setupTimer();
    }
}

bool SystemMonitor::isMemoryReportingEnabled() const
{
    Q_D(const SystemMonitor);

    return d->reportMem;
}

void SystemMonitor::enableCpuLoadReporting(bool enabled)
{
    Q_D(SystemMonitor);

    if (enabled != d->reportCpu) {
        d->reportCpu = enabled;
        d->setupTimer();
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
    ior->open();
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

void SystemMonitor::setReportingInterval(int intervalInMSec)
{
    Q_D(SystemMonitor);

    if (d->reportingInterval != intervalInMSec && intervalInMSec > 0) {
        d->reportingInterval = intervalInMSec;
        d->setupTimer();
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
