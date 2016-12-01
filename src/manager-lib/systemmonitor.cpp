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


/*!
    \class SystemMonitor
    \internal
*/

/*!
    \qmltype SystemMonitor
    \inqmlmodule QtApplicationManager
    \brief The SystemMonitor singleton.

    The SystemMonitor singleton type provides information about system resources and performance,
    like CPU and memory usage, I/O load and frame rate.

    The type is derived from \c QAbstractListModel, so it can be used directly as a model in an
    appropriate view.

    \target role names

    The following roles are available in this model:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c cpuLoad
        \li real
        \li The current CPU utilization in the range 0 (completely idle) to 1 (fully busy).
    \row
        \li \c memoryUsed
        \li int
        \li The amount of physical system memory used in bytes.
    \row
        \li \c memoryTotal
        \li int
        \li The total amount of physical memory (RAM) installed on the system in bytes.

    \row
        \li \c ioLoad
        \li var
        \li A map of devices registered with \l addIoLoadReporting() and their I/O load in the
            range [0, 1]. For instance the load of a registered device "sda" can be accessed
            through \c ioLoad.sda.
    \row
        \li \c averageFps
        \li real
        \li The average frame rate during the last \l reportingInterval in frames per second.
    \row
        \li \c minimumFps
        \li real
        \li The minimum frame rate during the last \l reportingInterval in frames per second.
    \row
        \li \c maximumFps
        \li real
        \li The maximum frame rate during the last \l reportingInterval in frames per second.
    \row
        \li \c fpsJitter
        \li real
        \li A measure for the average deviation from the ideal frame rate of 60 fps during the last
            \l reportingInterval.
    \endtable

    \note The model will be updated each \l reportingInterval milliseconds. The roles will only
          be populated, if the corresponding reporting parts (memory, CPU, etc.) have been enabled.

    After importing \c QtApplicationManager you could use the SystemMonitor singleton as follows:

    \qml
    import QtQuick 2.4
    import QtApplicationManager 1.0

    ListView {
        width: 200; height: 200

        model: SystemMonitor
        delegate: Text { text: averageFps }

        Component.onCompleted: {
            SystemMonitor.reportingInterval = 1000;
            SystemMonitor.fpsReportingEnabled = true;
        }
    }
    \endqml
*/

/*!
    \qmlproperty int SystemMonitor::count
    \readonly

    This property holds the number of reading points in the model. The \c count is determined by
    \l reportingRange divided by \l reportingInterval.
*/

/*!
    \qmlproperty int SystemMonitor::reportingInterval

    This property holds the interval in milliseconds between reporting updates. Note, that
    reporting will only start once this property is set. Valid values must be greater than zero.

    At least one of the reporting parts (memory, CPU load etc.) must be enabled, respectively
    registered to start the reporting.
*/

/*!
    \qmlproperty int SystemMonitor::reportingRange

    The time frame in milliseconds for which the model holds data, i.e. how far back in the past
    model data is available.

    The default value is 10000 (10 seconds).
*/


/*!
    \qmlproperty real SystemMonitor::idleLoadThreshold

    A value in the range [0, 1]. If the CPU load is greater than this threshold the \l idle
    property will be \c false, otherwise \c true. This property also influences when the
    application manager quick-launches application processes.

    The default value is read from the \l {Configuration}{configuration YAML file}
    (\c quicklaunch/idleLoad), respectively 0.1, if this configuration option is not provided.

    \sa idle
*/


/*!
    \qmlproperty int SystemMonitor::totalMemory
    \readonly

    This property holds the total amount of physical memory (RAM) installed on the system in bytes.
*/

/*!
    \qmlproperty int SystemMonitor::cpuCores
    \readonly

    This property holds the number of physical CPU cores that are installed on the system.
*/

/*!
    \qmlproperty bool SystemMonitor::memoryReportingEnabled

    A boolean value that determines whether periodic memory reporting is enabled.
*/

/*!
    \qmlproperty bool SystemMonitor::cpuLoadReportingEnabled

    A boolean value that determines whether periodic CPU load reporting is enabled.
*/

/*!
    \qmlproperty bool SystemMonitor::fpsReportingEnabled

    A boolean value that determines whether periodic frame rate reporting is enabled.
*/

/*!
    \qmlproperty bool SystemMonitor::idle
    \readonly

    A boolean value that defines, whether the system is idle. If the CPU load is greater than
    \l idleLoadThreshold, this property will be set to \c false, otherwise to \c true. The value is
    evaluated every second and reflects whether the average load during the last second was below
    or above the threshold.

    \sa idleLoadThreshold
*/


/*!
    \qmlsignal SystemMonitor::memoryReportingChanged(int total, int used);

    This signal is emitted periodically when memory reporting is enabled. The frequency is defined
    by \l reportingInterval. The \a total and \a used physical system memory in bytes are provided
    as arguments.

    \sa memoryReportingEnabled
    \sa reportingInterval
*/

/*!
    \qmlsignal SystemMonitor::cpuLoadReportingChanged(real load)

    This signal is emitted periodically when CPU load reporting is enabled. The frequency is
    defined by \l reportingInterval. The \a load parameter indicates the CPU utilization in the
    range 0 (completely idle) to 1 (fully busy).

    \sa cpuLoadReportingEnabled
    \sa reportingInterval
*/

/*!
    \qmlsignal SystemMonitor::ioLoadReportingChanged(string device, real load);

    This signal is emitted periodically for each I/O device that has been registered with
    \l addIoLoadReporting. The frequency is defined by \l reportingInterval. The string \a device
    holds the name of the device that is monitored. The \a load parameter indicates the
    utilization of the \a device in the range 0 (completely idle) to 1 (fully busy).

    \sa addIoLoadReporting
    \sa removeIoLoadReporting
    \sa ioLoadReportingDevices
    \sa reportingInterval
*/

/*!
    \qmlsignal SystemMonitor::fpsReportingChanged(real average, real minimum, real maximum, real jitter);

    This signal is emitted periodically when frame rate reporting is enabled. The update frequency
    is defined by \l reportingInterval. The arguments denote the \a average, \a minimum and
    \a maximum frame rate during the last \l reportingInterval in frames per second. Additionally,
    \a jitter is a measure for the average deviation from the ideal frame rate of 60 fps.
*/


QT_BEGIN_NAMESPACE_AM

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
        m_jitter += qAbs(MicrosInSec / IdealFrameTime - MicrosInSec / frameTime);
    }

    inline void reset()
    {
        m_count = m_sum = m_max = m_jitter = 0;
        m_min = INT_MAX;
    }

    inline qreal averageFps() const
    {
        return m_sum ? MicrosInSec * m_count / m_sum : qreal(0);
    }

    inline qreal minimumFps() const
    {
        return m_max ? MicrosInSec / m_max : qreal(0);
    }

    inline qreal maximumFps() const
    {
        return m_min ? MicrosInSec / m_min : qreal(0);
    }

    inline qreal jitterFps() const
    {
        return m_count ? m_jitter / m_count :  qreal(0);
    }

private:
    int m_count = 0;
    int m_sum = 0;
    int m_min = INT_MAX;
    int m_max = 0;
    qreal m_jitter = 0.0;

    QElapsedTimer m_timer;

    static const int IdealFrameTime = 16667; // usec - could be made configurable via an env variable
    static const qreal MicrosInSec;
};

const qreal FrameTimer::MicrosInSec = qreal(1000 * 1000);

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
    qreal idleThreshold = 0.1;
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
    // Report process only on half interval to decrease overload
    bool reportProcess = false;

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
            usedAppId = qSL("");
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
            if (reportProcess) {
                for (int i = 0; i < processMonitors.size(); i++)
                    processMonitors.at(i)->readData();
            }

            reportProcess = !reportProcess;

            if (reportCpu) {
                qreal cpuVal = cpu->readLoadValue();
                emit q->cpuLoadReportingChanged(cpuVal);
                r.cpuLoad = cpuVal;
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
                qreal ioVal = it.value()->readLoadValue();
                emit q->ioLoadReportingChanged(it.key(), ioVal);
                r.ioLoad.insert(it.key(), ioVal);
            }
            if (!r.ioLoad.isEmpty())
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
            qreal idleVal = idleCpu->readLoadValue();
            bool nowIdle = (idleVal <= idleThreshold);
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

/*!
    \qmlmethod object SystemMonitor::get(int index)

    Returns the model data for the reading point identified by \a index as a JavaScript object.
    See the \l {role names} for the expected object elements. The \a index must be in the range
    [0, \l count), returns an empty object if it is invalid.
*/
QVariantMap SystemMonitor::get(int row) const
{
    if (row < 0 || row >= count()) {
        qCWarning(LogSystem) << "invalid row:" << row;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        map.insert(qL1S(it.value()), data(index(row), it.key()));
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

/*!
    \qmlmethod bool SystemMonitor::setMemoryWarningThresholds(real lowWarning, real criticalWarning);

    Activates monitoring of available system memory. The arguments must define percent values (in
    the range [0, 100]) of the \l totalMemory available. The \a lowWarning argument defines the
    threshold in percent, when applications get the \l ApplicationInterface::memoryLowWarning()
    signal. The \a criticalWarning argument defines the threshold, when applications get the
    \l ApplicationInterface::memoryCriticalWarning() signal.

    Returns true, if monitoring could be started, otherwise false (e.g. if arguments are out of
    range).

    \sa totalMemory
    \sa ApplicationInterface::memoryLowWarning()
    \sa ApplicationInterface::memoryCriticalWarning()
*/
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

/*!
    \qmlmethod real SystemMonitor::memoryLowWarningThreshold()

    Returns the current threshold in percent, when a low memory warning will be sent to
    applications.

    \sa setMemoryWarningThresholds()
*/
qreal SystemMonitor::memoryLowWarningThreshold() const
{
    Q_D(const SystemMonitor);

    return d->memoryLowWarning;
}

/*!
    \qmlmethod real SystemMonitor::memoryCriticalWarningThreshold()

    Returns the current threshold in percent, when a critical memory warning will be sent to
    applications.

    \sa setMemoryWarningThresholds()
*/
qreal SystemMonitor::memoryCriticalWarningThreshold() const
{
    Q_D(const SystemMonitor);

    return d->memoryCriticalWarning;
}

void SystemMonitor::setIdleLoadThreshold(qreal loadThreshold)
{
    Q_D(SystemMonitor);

    if (loadThreshold != d->idleThreshold) {
        d->idleThreshold = loadThreshold;
    }
}

qreal SystemMonitor::idleLoadThreshold() const
{
    Q_D(const SystemMonitor);

    return d->idleThreshold;
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

/*!
    \qmlmethod bool SystemMonitor::addIoLoadReporting(string deviceName);

    Registers the device with the name \a deviceName for periodically reporting its I/O load.

    \note Currently this is only supported on Linux: the \a deviceName has to match one of the
    devices in the \c /sys/block directory.

    Returns true, if the device could be added, otherwise false (e.g. if it does not exist).
*/
bool SystemMonitor::addIoLoadReporting(const QString &deviceName)
{
    Q_D(SystemMonitor);

    if (!QFile::exists(qSL("/dev/") + deviceName))
        return false;
    if (d->ioHash.contains(deviceName))
        return false;

    IoReader *ior = new IoReader(deviceName.toLocal8Bit().constData());
    d->ioHash.insert(deviceName, ior);
    if (d->reportingInterval >= 0)
        ior->readLoadValue();   // for initialization only
    d->setupTimer();
    return true;
}

/*!
    \qmlmethod SystemMonitor::removeIoLoadReporting(string deviceName);

    Remove the device with the name \a deviceName from the list of monitored devices.
*/
void SystemMonitor::removeIoLoadReporting(const QString &deviceName)
{
    Q_D(SystemMonitor);

    delete d->ioHash.take(deviceName);
    d->setupTimer();
}

/*!
    \qmlmethod list<string> SystemMonitor::ioLoadReportingDevices()

    Returns a list of registered device names, that will report their I/O load.
*/
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
        for (auto it = d->ioHash.cbegin(); it != d->ioHash.cend(); ++it)
            it.value()->readLoadValue();   // for initialization only
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

/*!
    \qmlmethod object SystemMonitor::getProcessMonitor(string appId);

    Returns a reference to the \c ProcessMonitor type for the specific application identified by
    \a appId.
*/
QObject *SystemMonitor::getProcessMonitor(const QString &appId)
{
    Q_D(SystemMonitor);
    return d->getProcess(appId);
}

QT_END_NAMESPACE_AM
