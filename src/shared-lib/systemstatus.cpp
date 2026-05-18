// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <private/qobject_p.h>

#include "cpustatus.h"
#include "memorystatus.h"
#include "systemstatus.h"
#include "utilities.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype SystemStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the status of the system as a whole.

    SystemStatus monitors the running system, providing CPU and memory statistics as well as
    pressure stall information (PSI). It is the system-wide counterpart to \l CGroupStatus,
    which monitors a single Linux control group.

    \note CPU and memory monitoring work on Linux, Windows, and macOS. The
          \l{Linux Pressure Stall Information}{PSI} properties are only functional on Linux
          (kernel 4.20 or later with CONFIG_PSI enabled). All PSI properties return 0 or are
          non-functional on other platforms.

    \section2 Memory and CPU monitoring

    The \l memoryMax property reports the total amount of physical RAM installed on the system
    and is constant. The current memory usage and CPU load are available via \l memoryUsed and
    \l cpuLoad, which are updated on every call to \l update(). Unlike \l CGroupStatus, there is
    no system-level equivalent of \c memory.high, so a \c memoryHigh property is not exposed.

    You can use this type as a \l MonitorModel data source to record system load over time:

    \qml
    import QtQuick
    import QtApplicationManager

    MonitorModel {
        SystemStatus {}
    }
    \endqml

    Alternatively, drive updates yourself with a Timer:

    \qml
    import QtQuick
    import QtApplicationManager

    SystemStatus { id: systemStatus }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: systemStatus.update()
    }
    \endqml

    \section2 Pressure stall monitoring

    The \l cpuPSI, \l memoryPSI, and \l ioPSI properties each expose a
    \l PressureStallInformation object for event-driven \l{Linux Pressure Stall Information}
    {pressure monitoring}, backed by the kernel's system-wide interfaces at
    \c{/proc/pressure/cpu}, \c{/proc/pressure/memory}, and \c{/proc/pressure/io}. Configure the
    desired \l {PressureStallInformation::mode}{mode},
    \l {PressureStallInformation::timeWindow}{timeWindow}, and
    \l {PressureStallInformation::stallTime}{stallTime}, then connect to the
    \l {PressureStallInformation::triggered}{triggered} signal to react when the threshold is
    exceeded:

    \qml
    import QtQuick
    import QtApplicationManager

    SystemStatus {
        id: systemStatus

        Component.onCompleted: {
            cpuPSI.timeWindow = 2000  // 2 second observation window
            cpuPSI.stallTime = 200    // fire if stalled for 200ms within that window
            cpuPSI.mode = PressureStallInformation.Some
        }
    }

    Connections {
        target: systemStatus.cpuPSI
        function onTriggered() { console.log("System-wide CPU pressure detected") }
    }
    \endqml

    \sa CGroupStatus, CpuStatus, MemoryStatus, MonitorModel, PressureStallInformation
*/

class SystemStatusPrivate : public QObjectPrivate
{
public:
    CpuStatus *m_cpuStatus = nullptr;
    MemoryStatus *m_memoryStatus = nullptr;

    PressureStallInformation *m_cpuPSI = nullptr;
    PressureStallInformation *m_memoryPSI = nullptr;
    PressureStallInformation *m_ioPSI = nullptr;
};

SystemStatus::SystemStatus(QObject *parent)
    : QObject(*new SystemStatusPrivate, parent)
{
    Q_D(SystemStatus);

    d->m_cpuStatus = new CpuStatus(this);
    d->m_memoryStatus = new MemoryStatus(this);

    connect(d->m_cpuStatus, &CpuStatus::cpuLoadChanged,
            this, &SystemStatus::cpuLoadChanged);
    connect(d->m_memoryStatus, &MemoryStatus::memoryUsedChanged,
            this, &SystemStatus::memoryUsedChanged);

    d->m_cpuPSI = new PressureStallInformation(PressureStallInformation::Type::Cpu, this);
    d->m_memoryPSI = new PressureStallInformation(PressureStallInformation::Type::Memory, this);
    d->m_ioPSI = new PressureStallInformation(PressureStallInformation::Type::Io, this);

#if defined(Q_OS_LINUX)
    const QString base = testRootPathPrefix() + u"/proc/pressure/"_s;
    d->m_cpuPSI->setPressureFile(base + u"cpu"_s);
    d->m_memoryPSI->setPressureFile(base + u"memory"_s);
    d->m_ioPSI->setPressureFile(base + u"io"_s);
#endif
}

/*! \qmlproperty int SystemStatus::memoryMax
    \readonly

    The total amount of physical memory (RAM) installed on the system, in bytes. This is the
    system-wide analog of \l CGroupStatus::memoryMax.
*/
quint64 SystemStatus::memoryMax() const
{
    Q_D(const SystemStatus);
    return d->m_memoryStatus->totalMemory();
}

/*! \qmlproperty int SystemStatus::memoryUsed
    \readonly

    The amount of physical memory currently in use system-wide, in bytes. Refreshed on every call
    to \l update().

    \sa update(), memoryMax
*/
quint64 SystemStatus::memoryUsed() const
{
    Q_D(const SystemStatus);
    return d->m_memoryStatus->memoryUsed();
}

/*! \qmlproperty real SystemStatus::cpuLoad
    \readonly

    The overall system CPU utilization between the two most recent calls to \l update(), as a
    value ranging from \c 0 (inclusive, completely idle) to \c 1 (inclusive, all CPU cores fully
    busy).

    \sa update(), cpuCores
*/
qreal SystemStatus::cpuLoad() const
{
    Q_D(const SystemStatus);
    return d->m_cpuStatus->cpuLoad();
}

/*! \qmlproperty int SystemStatus::cpuCores
    \readonly

    The number of physical CPU cores installed on the system.
*/
int SystemStatus::cpuCores() const
{
    Q_D(const SystemStatus);
    return d->m_cpuStatus->cpuCores();
}

/*! \qmlproperty PressureStallInformation SystemStatus::cpuPSI
    \readonly

    Provides access to system-wide CPU pressure stall monitoring, backed by
    \c{/proc/pressure/cpu}. Monitors situations where tasks anywhere on the system are delayed
    waiting for CPU time.

    Only functional on Linux when the kernel has PSI support enabled.

    \sa PressureStallInformation, memoryPSI, ioPSI
*/
PressureStallInformation *SystemStatus::cpuPSI() const
{
    Q_D(const SystemStatus);
    return d->m_cpuPSI;
}

/*! \qmlproperty PressureStallInformation SystemStatus::memoryPSI
    \readonly

    Provides access to system-wide memory pressure stall monitoring, backed by
    \c{/proc/pressure/memory}. Monitors situations where tasks anywhere on the system are delayed
    waiting for memory.

    Only functional on Linux when the kernel has PSI support enabled.

    \sa PressureStallInformation, cpuPSI, ioPSI
*/
PressureStallInformation *SystemStatus::memoryPSI() const
{
    Q_D(const SystemStatus);
    return d->m_memoryPSI;
}

/*! \qmlproperty PressureStallInformation SystemStatus::ioPSI
    \readonly

    Provides access to system-wide I/O pressure stall monitoring, backed by
    \c{/proc/pressure/io}. Monitors situations where tasks anywhere on the system are delayed
    waiting for I/O.

    Only functional on Linux when the kernel has PSI support enabled.

    \sa PressureStallInformation, cpuPSI, memoryPSI
*/
PressureStallInformation *SystemStatus::ioPSI() const
{
    Q_D(const SystemStatus);
    return d->m_ioPSI;
}

/*!
    \qmlproperty list<string> SystemStatus::roleNames
    \readonly

    Names of the roles provided by SystemStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList SystemStatus::roleNames() const
{
    return { u"memoryMax"_s, u"memoryUsed"_s, u"cpuLoad"_s };
}

/*!
    \qmlmethod void SystemStatus::update()

    Updates the memoryUsed and cpuLoad properties.

    \sa memoryUsed, cpuLoad
*/
void SystemStatus::update()
{
    Q_D(SystemStatus);
    d->m_cpuStatus->update();
    d->m_memoryStatus->update();
}

QT_END_NAMESPACE_AM

#include "moc_systemstatus.cpp"
