// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <algorithm>
#include <limits>
#include <mutex>

#include <QElapsedTimer>
#include <QFile>
#include <QThread>
#include <private/qobject_p.h>

#include "logging.h"
#include "cgroupstatus.h"
#include "utilities.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype CGroupStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the status of a Linux cgroup.

    CGroupStatus monitors a Linux cgroup v2 control group, providing both memory statistics and
    pressure stall information (PSI). It defaults to the cgroup of the current process, but you
    can point it at any other cgroup by setting the \l path property.

    If you need system-wide instead of per-cgroup information, see \l SystemStatus.

    \note This type only works on Linux systems running \l{Linux cgroup v2}{cgroup v2}. All
          properties return 0 or are non-functional on other platforms or when running under
          cgroup v1.

    \section2 Memory and CPU monitoring

    The \l memoryHigh and \l memoryMax limits are read once when \l path is set. The current
    memory usage is available via \l memoryUsed, which is updated on every call to \l update().

    Similarly, \l cpuLoad is also updated on every call to \l update(), and reflects the CPU
    utilization of the cgroup since the previous \l update() call.

    You can use this type as a \l MonitorModel data source to record memory usage over time:

    \qml
    import QtQuick
    import QtApplicationManager

    MonitorModel {
        CGroupStatus {}
    }
    \endqml

    Alternatively, drive updates yourself with a Timer:

    \qml
    import QtQuick
    import QtApplicationManager

    CGroupStatus { id: cgroupStatus }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: cgroupStatus.update()
    }
    \endqml

    \section2 Pressure stall monitoring

    The \l cpuPSI, \l memoryPSI, and \l ioPSI properties each expose a
    \l PressureStallInformation object for event-driven \l{Linux Pressure Stall Information}
    {pressure monitoring}. Configure the desired \l {PressureStallInformation::mode}{mode},
    \l {PressureStallInformation::timeWindow}{timeWindow}, and
    \l {PressureStallInformation::stallTime}{stallTime}, then connect to the
    \l {PressureStallInformation::triggered}{triggered} signal to react when the threshold is
    exceeded:

    \qml
    import QtQuick
    import QtApplicationManager

    CGroupStatus {
        id: cgroupStatus

        Component.onCompleted: {
            memoryPSI.timeWindow = 2000  // 2 second observation window
            memoryPSI.stallTime = 100    // fire if stalled for 100ms within that window
            memoryPSI.mode = PressureStallInformation.Some
        }
    }

    Connections {
        target: cgroupStatus.memoryPSI
        function onTriggered() { console.log("Memory pressure detected") }
    }
    \endqml

    \sa MemoryStatus, MonitorModel, PressureStallInformation, SystemStatus
*/


class CGroupStatusPrivate : public QObjectPrivate
{
public:
    void setControlGroup(const QString &path);
    static quint64 readCGroupValue(const QString &path);
    quint64 readMemoryUsed();
    qreal readCpuLoad();

    QString m_path { };
    QFile m_memoryCurrentFile;
    QFile m_cpuStatFile;

    quint64 m_memoryHigh = 0u;
    quint64 m_memoryMax = 0u;
    quint64 m_memoryUsed = 0u;

    quint64 m_lastCpuUsageUsec = 0u;
    QElapsedTimer m_cpuClock;        // used as a monotonic clock; never restarted
    qint64 m_lastCpuSampleNsec = -1; // no prior sample yet
    qreal m_cpuLoad = 0;

    PressureStallInformation *m_cpuPSI = nullptr;
    PressureStallInformation *m_memoryPSI = nullptr;
    PressureStallInformation *m_ioPSI = nullptr;

    static bool s_hasCGroupV2;
};

bool CGroupStatusPrivate::s_hasCGroupV2 = false;

CGroupStatus::CGroupStatus(QObject *parent)
    : QObject(*new CGroupStatusPrivate, parent)
{
    std::once_flag once;
    std::call_once(once, [] {
#if defined(Q_OS_LINUX)
        CGroupStatusPrivate::s_hasCGroupV2 = QFile::exists(
            testRootPathPrefix() + u"/sys/fs/cgroup/cgroup.controllers"_s);
#endif
    });

    Q_D(CGroupStatus);
    d->m_cpuClock.start();
    d->m_cpuPSI = new PressureStallInformation(PressureStallInformation::Type::Cpu, this);
    d->m_memoryPSI = new PressureStallInformation(PressureStallInformation::Type::Memory, this);
    d->m_ioPSI = new PressureStallInformation(PressureStallInformation::Type::Io, this);

#if defined(Q_OS_LINUX)
    if (CGroupStatusPrivate::s_hasCGroupV2) {
        QFile f(testRootPathPrefix() + u"/proc/self/cgroup"_s);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray line = f.readLine().trimmed();
            if (line.startsWith("0::/")) {
                QString group = QString::fromLocal8Bit(line.mid(4));
                d->setControlGroup(group);
            }
        }
        if (d->m_path.isEmpty())
            qCWarning(LogSystem) << "Could not determine current process's cgroup path from /proc/self/cgroup";
    }
#endif
}

/*! \qmlproperty string CGroupStatus::path

    The path of the cgroup this CGroupStatus instance is monitoring. This is only relevant on Linux
    systems when running cgroup v2, and will be an empty string otherwise.

    This defaults to the cgroup of the current process, but you can set it to another cgroup valid
    cgroup identifier (eg: \c{user.slice/user-1000.slice/session-2.scope}).
*/

QString CGroupStatus::path() const
{
    Q_D(const CGroupStatus);
    return d->m_path;
}

void CGroupStatus::setPath(const QString &path)
{
    Q_D(CGroupStatus);
    if (d->m_path == path)
        return;

    d->setControlGroup(path);
    emit pathChanged();
}

quint64 CGroupStatusPrivate::readCGroupValue(const QString &path)
{
#if defined(Q_OS_LINUX)
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;
    const QByteArray data = f.readLine().trimmed();
    if (data == "max")
        return std::numeric_limits<quint64>::max();
    bool ok;
    const quint64 value = data.toULongLong(&ok);
    return ok ? value : 0;
#else
    Q_UNUSED(path)
    return 0u;
#endif
}

void CGroupStatusPrivate::setControlGroup(const QString &path)
{
#if defined(Q_OS_LINUX)
    if (path == m_path)
        return;

    m_memoryCurrentFile.close();
    m_cpuStatFile.close();
    m_lastCpuUsageUsec = 0u;
    m_lastCpuSampleNsec = -1;
    m_cpuLoad = 0;

    const QString base = testRootPathPrefix() + u"/sys/fs/cgroup/" + path + u'/';
    if (!QFile::exists(base + u"memory.current")) {
        qCWarning(LogSystem) << "cgroup" << path << "is not valid (no memory.current file)";
        return;
    }

    m_path = path;
    m_memoryHigh = readCGroupValue(base + u"memory.high"_s);
    m_memoryMax = readCGroupValue(base + u"memory.max"_s);

    m_memoryCurrentFile.setFileName(base + u"memory.current");
    if (!m_memoryCurrentFile.open(QIODevice::ReadOnly)) {
        qCWarning(LogSystem) << "Cannot read cgroup memory statistics from"
                             << m_memoryCurrentFile.fileName();
        return;
    }

    m_memoryUsed = readMemoryUsed();

    m_cpuStatFile.setFileName(base + u"cpu.stat");
    if (!m_cpuStatFile.open(QIODevice::ReadOnly)) {
        qCWarning(LogSystem) << "Cannot read cgroup CPU statistics from"
                             << m_cpuStatFile.fileName();
        return;
    }

    m_cpuLoad = readCpuLoad(); // prime usage and sample timestamp

    m_cpuPSI->setPressureFile(base + u"cpu.pressure"_s);
    m_memoryPSI->setPressureFile(base + u"memory.pressure"_s);
    m_ioPSI->setPressureFile(base + u"io.pressure"_s);

#else
    Q_UNUSED(path)
#endif
}

/*! \qmlproperty int CGroupStatus::memoryHigh
    \readonly

    The memory.high soft limit (in bytes) of this cgroup, or 0 if not running under cgroup v2.
    When the cgroup file contains the string \c "max", this returns \l max.

    \note As the kernel does not have a mechanism to notify changes, this value is not updated after
         the initial read when \l path is set.
*/
quint64 CGroupStatus::memoryHigh() const
{
    Q_D(const CGroupStatus);
    return d->m_memoryHigh;
}

/*! \qmlproperty int CGroupStatus::memoryMax
    \readonly

    The memory.max hard limit (in bytes) of this cgroup, or 0 if not running under cgroup v2.
    When the cgroup file contains the string \c "max", this returns \l max.

    \note As the kernel does not have a mechanism to notify changes, this value is not updated after
         the initial read when \l path is set.
*/
quint64 CGroupStatus::memoryMax() const
{
    Q_D(const CGroupStatus);
    return d->m_memoryMax;
}

/*! \qmlproperty int CGroupStatus::memoryUsed
    \readonly

    The current memory usage (in bytes) of this cgroup, as reported by \c memory.current.
    Updated on every call to \l update(). Returns 0 if not running under cgroup v2.

    \sa update()
*/
quint64 CGroupStatus::memoryUsed() const
{
    Q_D(const CGroupStatus);
    return d->m_memoryUsed;
}

/*! \qmlproperty real CGroupStatus::cpuLoad
    \readonly

    The average CPU utilization of this cgroup over the interval between the two most recent calls
    to \l update() (or between \l path being set and the first \l update() call), as a value
    ranging from \c 0 (inclusive, completely idle) to \c 1 (inclusive, all CPU cores fully busy).

    The value is computed from the cgroup's \c cpu.stat \c usage_usec counter, divided by elapsed
    wall-clock time and normalized by the number of CPU cores on the system.

    Returns 0 if not running under cgroup v2 or if the CPU controller is not enabled for this
    cgroup.

    \sa update()
*/
qreal CGroupStatus::cpuLoad() const
{
    Q_D(const CGroupStatus);
    return d->m_cpuLoad;
}

/*! \qmlproperty PressureStallInformation CGroupStatus::cpuPSI
    \readonly

    Provides access to CPU pressure stall monitoring for this cgroup. Monitors situations where
    tasks are delayed waiting for CPU time.

    Only functional when running under cgroup v2.

    \sa PressureStallInformation, memoryPSI, ioPSI
*/
PressureStallInformation *CGroupStatus::cpuPSI() const
{
    Q_D(const CGroupStatus);
    return d->m_cpuPSI;
}

/*! \qmlproperty PressureStallInformation CGroupStatus::memoryPSI
    \readonly

    Provides access to memory pressure stall monitoring for this cgroup. Monitors situations where
    tasks are delayed waiting for memory to become available.

    Only functional when running under cgroup v2.

    \sa PressureStallInformation, cpuPSI, ioPSI
*/
PressureStallInformation *CGroupStatus::memoryPSI() const
{
    Q_D(const CGroupStatus);
    return d->m_memoryPSI;
}

/*! \qmlproperty PressureStallInformation CGroupStatus::ioPSI
    \readonly

    Provides access to I/O pressure stall monitoring for this cgroup. Monitors situations where
    tasks are delayed waiting for I/O operations to complete.

    Only functional when running under cgroup v2.

    \sa PressureStallInformation, cpuPSI, memoryPSI
*/
PressureStallInformation *CGroupStatus::ioPSI() const
{
    Q_D(const CGroupStatus);
    return d->m_ioPSI;
}

/*! \qmlproperty int CGroupStatus::max
    \readonly

    This property always returns the largest unsigned 64-bit integer value, which is used to
    express the \c max value in cgroup v2 files.
*/
quint64 CGroupStatus::maxValue() const
{
    return std::numeric_limits<quint64>::max();
}

/*!
    \qmlproperty list<string> CGroupStatus::roleNames
    \readonly

    Names of the roles provided by CGroupStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList CGroupStatus::roleNames() const
{
    return { u"memoryHigh"_s, u"memoryMax"_s, u"memoryUsed"_s, u"cpuLoad"_s };
}

/*!
    \qmlmethod void CGroupStatus::update()

    Updates the memoryUsed and cpuLoad properties.

    \sa memoryUsed, cpuLoad
*/
void CGroupStatus::update()
{
    Q_D(CGroupStatus);

    const quint64 newUsed = d->readMemoryUsed();
    if (d->m_memoryUsed != newUsed) {
        d->m_memoryUsed = newUsed;
        emit memoryUsedChanged();
    }

    const qreal newLoad = d->readCpuLoad();
    if (!qFuzzyCompare(d->m_cpuLoad, newLoad)) {
        d->m_cpuLoad = newLoad;
        emit cpuLoadChanged();
    }
}

quint64 CGroupStatusPrivate::readMemoryUsed()
{
#if defined(Q_OS_LINUX)
    if (!m_memoryCurrentFile.isOpen())
        return 0u;
    m_memoryCurrentFile.seek(0);
    const QByteArray buffer = m_memoryCurrentFile.read(32); // 64-bit decimal value
    return ::strtoull(buffer.constData(), nullptr, 10);
#endif
    return 0u;
}

qreal CGroupStatusPrivate::readCpuLoad()
{
#if defined(Q_OS_LINUX)
    if (!m_cpuStatFile.isOpen())
        return 0;
    m_cpuStatFile.seek(0);
    // cpu.stat is a list of "<key> <number>" lines. The kernel currently emits usage_usec first,
    // but the cgroup v2 ABI does not document a field order, so search by key.
    static constexpr QByteArrayView key("usage_usec ");
    quint64 usageUsec = 0;
    while (!m_cpuStatFile.atEnd()) {
        const QByteArray line = m_cpuStatFile.readLine(64);
        if (line.startsWith(key)) {
            usageUsec = ::strtoull(line.constData() + key.size(), nullptr, 10);
            break;
        }
    }

    // inconveniently, there is no QElapsedTimer::nsecsRestart() method
    const qint64 nowNsec = m_cpuClock.nsecsElapsed();

    qreal load = 0;
    if (m_lastCpuSampleNsec >= 0) {
        const qint64 elapsedNsec = nowNsec - m_lastCpuSampleNsec;
        if ((elapsedNsec > 0) && (usageUsec >= m_lastCpuUsageUsec)) {
            static const int cores = std::max(1, QThread::idealThreadCount());
            const qreal deltaUsageNsec = qreal(usageUsec - m_lastCpuUsageUsec) * qreal(1000);
            load = deltaUsageNsec / (qreal(elapsedNsec) * qreal(cores));
            load = std::clamp(load, qreal(0), qreal(1));
        }
    }
    m_lastCpuUsageUsec = usageUsec;
    m_lastCpuSampleNsec = nowNsec;
    return load;
#endif
    return 0;
}

QT_END_NAMESPACE_AM

#include "moc_cgroupstatus.cpp"
