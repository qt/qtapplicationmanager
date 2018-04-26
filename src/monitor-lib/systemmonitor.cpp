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

#include <QSysInfo>
#include <QThread>
#include <QFile>
#include <QHash>
#include <QTimerEvent>
#include <QElapsedTimer>
#include <vector>
#if !defined(AM_HEADLESS)
#  include <QGuiApplication>
#  include <QQuickView>
#endif

#include "global.h"
#include "logging.h"
#include "qml-utilities.h"
#include "applicationmanager.h"
#include "systemmonitor.h"
#include "systemreader.h"
#include <QtAppManWindow/windowmanager.h>
#include "frametimer.h"


/*!
    \qmltype SystemMonitor
    \inqmlmodule QtApplicationManager
    \ingroup system-ui-singletons
    \brief The system monitoring model, giving access to a range of measurements, e.g. CPU load,
    frame rate, etc.

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
        \li \c gpuLoad
        \li real
        \li The current GPU utilization in the range 0 (completely idle) to 1 (fully busy).
            This is dependent on tools from the graphics hardware vendor and might not work on
            every system.
    \row
        \li \c memoryUsed
        \li int
        \li The amount of physical system memory used in bytes.
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

    This property holds the number of reading points that will be kept in the model. The minimum
    value that can be set is 2 and the default value is 10.
*/

/*!
    \qmlproperty int SystemMonitor::reportingInterval

    This property holds the interval in milliseconds between reporting updates. Note, that
    reporting will only start once this property is set. Setting a new value will reset the model.
    Valid values must be greater than zero.

    At least one of the reporting parts (memory, CPU load etc.) must be enabled, respectively
    registered to start the reporting.
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
    \qmlproperty int SystemMonitor::memoryUsed
    \readonly

    This property holds the amount of physical memory (RAM) used in bytes.
*/

/*!
    \qmlproperty int SystemMonitor::cpuCores
    \readonly

    This property holds the number of physical CPU cores that are installed on the system.
*/

/*!
    \qmlproperty int SystemMonitor::cpuLoad
    \readonly

    This property holds the current CPU utilization as a value ranging from 0 (inclusive, completely
    idle) to 1 (inclusive, fully busy).
*/

/*!
    \qmlproperty int SystemMonitor::gpuLoad
    \readonly

    This property holds the current GPU utilization as a value ranging from 0 (inclusive, completely
    idle) to 1 (inclusive, fully busy).

    \note This is dependent on tools from the graphics hardware vendor and might not work on
          every system.

    Currently, this only works on \e Linux with either \e Intel or \e NVIDIA chipsets, plus the
    tools from the respective vendors have to be installed:

    \table
    \header
        \li Hardware
        \li Tool
        \li Notes
    \row
        \li NVIDIA
        \li \c nvidia-smi
        \li The utilization will only be shown for the first GPU of the system, in case multiple GPUs
            are installed.
    \row
        \li Intel
        \li \c intel_gpu_top
        \li The binary has to be made set-UID root, e.g. via \c{sudo chmod +s $(which intel_gpu_top)},
            or the application-manager has to be run as the \c root user.
    \endtable
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
    \qmlproperty bool SystemMonitor::gpuLoadReportingEnabled

    A boolean value that determines whether periodic GPU load reporting is enabled.

    GPU load reporting is only supported on selected hardware: please see gpuLoad for more
    information.
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
    \qmlsignal SystemMonitor::memoryReportingChanged(int used);

    This signal is emitted periodically when memory reporting is enabled. The frequency is defined
    by \l reportingInterval. The \a used physical system memory in bytes is provided as argument.

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
    \qmlsignal SystemMonitor::gpuLoadReportingChanged(real load)

    This signal is emitted periodically when GPU load reporting is enabled. The frequency is
    defined by \l reportingInterval. The \a load parameter indicates the GPU utilization in the
    range 0 (completely idle) to 1 (fully busy).

    \sa gpuLoadReportingEnabled
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
    IoLoad,
    GpuLoad,

    AverageFps = Qt::UserRole + 6000,
    MinimumFps,
    MaximumFps,
    FpsJitter
};
}

class SystemMonitorPrivate : public QObject // clazy:exclude=missing-qobject-macro
{
public:
    SystemMonitorPrivate(SystemMonitor *q)
        : q_ptr(q)
    { }

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
    int reportingInterval = -1;
    int count = 10;
    int reportingRange = 100 * 100;
    bool reportingRangeSet = false;
    int reportingTimerId = 0;
    bool reportCpu = false;
    bool reportGpu = false;
    bool reportMem = false;
    bool reportFps = false;
    int cpuTail = 0;
    int gpuTail = 0;
    int memTail = 0;
    int fpsTail = 0;
    QMap<QString, int> ioTails;
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

    int latestReportPos() const
    {
        if (reportPos == 0)
            return reports.size() - 1;
        else
            return reportPos - 1;
    }

#if !defined(AM_HEADLESS)
    void registerNewView(QQuickWindow *view)
    {
        Q_Q(SystemMonitor);
        if (reportFps)
            connect(view, &QQuickWindow::frameSwapped, q, &SystemMonitor::reportFrameSwap);
    }
#endif

    void setupFpsReporting()
    {
#if !defined(AM_HEADLESS)
        Q_Q(SystemMonitor);
        if (!windowManagerConnectionCreated) {
            connect(WindowManager::instance(), &WindowManager::compositorViewRegistered, this, &SystemMonitorPrivate::registerNewView);
            windowManagerConnectionCreated = true;
        }

        for (const QQuickWindow *view : WindowManager::instance()->compositorViews()) {
            if (reportFps)
                connect(view, &QQuickWindow::frameSwapped, q, &SystemMonitor::reportFrameSwap);
            else
                disconnect(view, &QQuickWindow::frameSwapped, q, &SystemMonitor::reportFrameSwap);
        }
#endif
    }

    void setupTimer(int newInterval = -1)
    {
        bool useNewInterval = (newInterval != -1) && (newInterval != reportingInterval);
        bool shouldBeOn = reportCpu || reportGpu || reportMem || reportFps || !ioHash.isEmpty()
                          || cpuTail > 0 || memTail > 0 || fpsTail > 0 || !ioTails.isEmpty();

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
            if (!reportingTimerId && reportingInterval >= 0)
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
                r.cpuLoad = cpu->readLoadValue();
                roles.append(CpuLoad);
            } else if (cpuTail > 0) {
                --cpuTail;
                roles.append(CpuLoad);
            }

            if (reportGpu) {
                r.gpuLoad = gpu->readLoadValue();
                roles.append(GpuLoad);
            } else if (gpuTail > 0) {
                --gpuTail;
                roles.append(GpuLoad);
            }

            if (reportMem) {
                r.memoryUsed = memory->readUsedValue();
                roles.append(MemoryUsed);
            } else if (memTail > 0) {
                --memTail;
                roles.append(MemoryUsed);
            }

            for (auto it = ioHash.cbegin(); it != ioHash.cend(); ++it) {
                qreal ioVal = it.value()->readLoadValue();
                emit q->ioLoadReportingChanged(it.key(), ioVal);
                r.ioLoad.insert(it.key(), ioVal);
            }
            if (!r.ioLoad.isEmpty())
                roles.append(IoLoad);
            if (!ioTails.isEmpty()) {
                if (r.ioLoad.isEmpty())
                    roles.append(IoLoad);
                for (const auto &it : ioTails.keys()) {
                    r.ioLoad.insert(it, 0.0);
                    if (--ioTails[it] == 0)
                        ioTails.remove(it);
                }
            }

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
            } else if (fpsTail > 0){
                --fpsTail;
                roles.append(AverageFps);
                roles.append(MinimumFps);
                roles.append(MaximumFps);
                roles.append(FpsJitter);
            }

            // ring buffer handling
            // optimization: instead of sending a dataChanged for every item, we always move the
            // last item to the front and change its data only
            int last = reports.size() - 1;
            q->beginMoveRows(QModelIndex(), last, last, QModelIndex(), 0);
            reports[reportPos++] = r;
            if (reportPos > last)
                reportPos = 0;
            q->endMoveRows();
            q->dataChanged(q->index(0), q->index(0), roles);

            if (reportMem)
                emit q->memoryReportingChanged(r.memoryUsed);
            if (reportCpu)
                emit q->cpuLoadReportingChanged(r.cpuLoad);
            if (reportGpu)
                emit q->gpuLoadReportingChanged(r.gpuLoad);

            setupTimer();  // we might be able to stop this timer, when end of tail reached

        } else if (te && te->timerId() == idleTimerId) {
            qreal idleVal = idleCpu->readLoadValue();
            bool nowIdle = (idleVal <= idleThreshold);
            if (nowIdle != isIdle) {
                isIdle = nowIdle;
                emit q->idleChanged(nowIdle);
            }
        }
    }

    const Report &reportForRow(int row) const
    {
        // convert a visual row position to an index into the internal ringbuffer
        int pos = reportPos - row - 1;
        if (pos < 0)
            pos += reports.size();
        if (pos < 0 || pos >= reports.size())
            return reports.first();
        return reports.at(pos);
    }

    void updateModel(bool clear)
    {
        Q_Q(SystemMonitor);

        q->beginResetModel();

        if (reportingRangeSet) {
            // we need at least 2 items, otherwise we cannot move rows
            count = qMax(2, reportingRange / reportingInterval);
        }

        if (clear) {
            reports.clear();
            reports.resize(count);
            reportPos = 0;
            cpuTail = memTail = fpsTail = 0;
            ioTails.clear();
        } else {
            int oldCount = reports.size();
            int diff = count - oldCount;
            if (diff > 0)
                reports.insert(reportPos, diff, Report());
            else {
                if (reportPos <= count) {
                    reports.remove(reportPos, -diff);
                    if (reportPos == count)
                        reportPos = 0;
                } else {
                    reports.remove(reportPos, oldCount - reportPos);
                    reports.remove(0, reportPos - count);
                    reportPos = 0;
                }
            }

            if (cpuTail > 0)
                cpuTail += diff;
            if (memTail > 0)
                memTail += diff;
            if (fpsTail > 0)
                fpsTail += diff;
            for (const auto &it : ioTails.keys())
                ioTails[it] += diff;
        }
        q->endResetModel();
    }
};


SystemMonitor *SystemMonitor::s_instance = nullptr;


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

QObject *SystemMonitor::instanceForQml(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

SystemMonitor::SystemMonitor()
    : d_ptr(new SystemMonitorPrivate(this))
{
    Q_D(SystemMonitor);

    d->idleCpu = new CpuReader;
    d->cpu = new CpuReader;
    d->gpu = new GpuReader;
    d->memory = new MemoryReader;

    d->idleTimerId = d->startTimer(1000);

    d->roleNames.insert(CpuLoad, "cpuLoad");
    d->roleNames.insert(GpuLoad, "gpuLoad");
    d->roleNames.insert(MemoryUsed, "memoryUsed");
    d->roleNames.insert(IoLoad, "ioLoad");
    d->roleNames.insert(AverageFps, "averageFps");
    d->roleNames.insert(MinimumFps, "minimumFps");
    d->roleNames.insert(MaximumFps, "maximumFps");
    d->roleNames.insert(FpsJitter, "fpsJitter");

    d->updateModel(true);
}

SystemMonitor::~SystemMonitor()
{
    Q_D(SystemMonitor);

    delete d->idleCpu;
    delete d->memory;
    delete d->cpu;
    delete d->gpu;
    qDeleteAll(d->ioHash);
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
    case GpuLoad:
        return r.gpuLoad;
    case MemoryUsed:
        return r.memoryUsed;
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

quint64 SystemMonitor::memoryUsed() const
{
    Q_D(const SystemMonitor);
    return d->reports[d->latestReportPos()].memoryUsed;
}

int SystemMonitor::cpuCores() const
{
    return QThread::idealThreadCount();
}

qreal SystemMonitor::cpuLoad() const
{
    Q_D(const SystemMonitor);
    return d->reports[d->latestReportPos()].cpuLoad;
}

qreal SystemMonitor::gpuLoad() const
{
    Q_D(const SystemMonitor);
    return d->reports[d->latestReportPos()].gpuLoad;
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

    \note This is only supported on Linux with the cgroups memory subsystem enabled.

    \sa totalMemory
    \sa ApplicationInterface::memoryLowWarning()
    \sa ApplicationInterface::memoryCriticalWarning()
*/
bool SystemMonitor::setMemoryWarningThresholds(qreal lowWarning, qreal criticalWarning)
{
    Q_D(SystemMonitor);

    if (!qFuzzyCompare(lowWarning, d->memoryLowWarning) || !qFuzzyCompare(criticalWarning, d->memoryCriticalWarning)) {
        d->memoryLowWarning = lowWarning;
        d->memoryCriticalWarning = criticalWarning;
        if (!d->memoryWatcher) {
            d->memoryWatcher = new MemoryWatcher(this);
            connect(d->memoryWatcher, &MemoryWatcher::memoryLow,
                    ApplicationManager::instance(), &ApplicationManager::memoryLowWarning);
            connect(d->memoryWatcher, &MemoryWatcher::memoryCritical,
                    ApplicationManager::instance(), &ApplicationManager::memoryCriticalWarning);
        }
        d->memoryWatcher->setThresholds(lowWarning, criticalWarning);
        return d->memoryWatcher->startWatching();
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

    if (!qFuzzyCompare(loadThreshold, d->idleThreshold)) {
        d->idleThreshold = loadThreshold;
        emit idleLoadThresholdChanged(loadThreshold);
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
        if (!enabled)
            d->memTail = d->count;
        else
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
        if (!enabled)
            d->cpuTail = d->count;
        else
            d->setupTimer();
        emit cpuLoadReportingEnabledChanged();
    }
}

bool SystemMonitor::isCpuLoadReportingEnabled() const
{
    Q_D(const SystemMonitor);

    return d->reportCpu;
}

void SystemMonitor::setGpuLoadReportingEnabled(bool enabled)
{
    Q_D(SystemMonitor);

    if (enabled != d->reportGpu) {
        d->reportGpu = enabled;
        if (!enabled)
            d->gpuTail = d->count;
        else
            d->setupTimer();
        d->gpu->setActive(enabled);
        emit gpuLoadReportingEnabledChanged();
    }
}

bool SystemMonitor::isGpuLoadReportingEnabled() const
{
    Q_D(const SystemMonitor);

    return d->reportGpu;
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

    d->ioTails.remove(deviceName);

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
    d->ioTails.insert(deviceName, d->count);
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
        if (!enabled)
            d->fpsTail = d->count;
        else
            d->setupTimer();
        d->setupFpsReporting();
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
        d->updateModel(true);
        d->setupTimer(intervalInMSec);
        emit reportingIntervalChanged(intervalInMSec);
    }
}

int SystemMonitor::reportingInterval() const
{
    Q_D(const SystemMonitor);

    return d->reportingInterval;
}

void SystemMonitor::setCount(int count)
{
    Q_D(SystemMonitor);

    if (count != d->count) {
        d->count = count > 2 ? count : 2;
        d->updateModel(false);
        emit countChanged();
    }
}

int SystemMonitor::count() const
{
    Q_D(const SystemMonitor);

    return d->count;
}

void SystemMonitor::setReportingRange(int rangeInMSec)
{
    Q_D(SystemMonitor);

    qCWarning(LogSystem) << "Property \"reportingRange\" is deprecated, use \"count\" instead.";

    if (d->reportingRange != rangeInMSec && rangeInMSec > 0) {
        d->reportingRange = rangeInMSec;
        d->reportingRangeSet = true;
        d->updateModel(false);
        emit reportingRangeChanged(rangeInMSec);
    }
}

int SystemMonitor::reportingRange() const
{
    Q_D(const SystemMonitor);

    return d->reportingRange;
}

/*! \internal
    report a frame swap for any window. \a item is \c 0 for the System-UI
*/
void SystemMonitor::reportFrameSwap()
{
    Q_D(SystemMonitor);

    if (!d->reportFps)
        return;

    FrameTimer *frameTimer = d->frameTimer.value(nullptr);
    if (!frameTimer) {
        frameTimer = new FrameTimer();
        d->frameTimer.insert(nullptr, frameTimer);
    }

    frameTimer->newFrame();
}

QT_END_NAMESPACE_AM
