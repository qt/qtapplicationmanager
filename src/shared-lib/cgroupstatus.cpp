// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <cerrno>
#include <algorithm>
#include <chrono>
#include <limits>
#include <mutex>

#include <QElapsedTimer>
#include <QFile>
#include <QThread>
#include <QTimer>
#include <QSocketNotifier>
#include <private/qobject_p.h>

#if defined(Q_OS_LINUX)
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include "exception.h"
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

    \sa MemoryStatus, MonitorModel, PressureStallInformation
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

class PressureStallInformationPrivate : public QObjectPrivate
{
public:
#if defined(Q_OS_LINUX)
    ~PressureStallInformationPrivate()
    {
        if (m_eventFd != -1)
            ::close(m_eventFd);
    }

    int m_eventFd = -1;
#endif

    void startReconfigure();
    void finishReconfigure();

    PressureStallInformation::Mode m_mode = PressureStallInformation::Mode::Off;
    std::chrono::milliseconds m_timeWindow { };
    std::chrono::milliseconds m_stallTime { };
    QTimer m_reconfigureTimer;
    QSocketNotifier m_notifier { QSocketNotifier::Exception };

    PressureStallInformation::Type m_cgroupType { };
    CGroupStatus *m_cgroupStatus = nullptr;
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
    d->m_cpuPSI = new PressureStallInformation(this, PressureStallInformation::Type::Cpu);
    d->m_memoryPSI = new PressureStallInformation(this, PressureStallInformation::Type::Memory);
    d->m_ioPSI = new PressureStallInformation(this, PressureStallInformation::Type::Io);

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

/*!
    \qmltype PressureStallInformation
    \inqmlmodule QtApplicationManager
    \ingroup common-non-instantiable
    \brief Provides Linux PSI (Pressure Stall Information) monitoring for a cgroup resource.

    This type cannot be instantiated directly. Instances are accessed via the
    \l CGroupStatus::cpuPSI, \l CGroupStatus::memoryPSI, and \l CGroupStatus::ioPSI properties.

    \l{Linux Pressure Stall Information}{Linux PSI} monitoring (available since kernel 4.20 with
    \l{Linux cgroup v2}{cgroup v2}) detects resource pressure by tracking the time tasks spend
    stalled waiting for a resource. Set \l mode to \c Some or \c Full and configure \l timeWindow
    and \l stallTime to define the trigger threshold. The \l triggered signal fires whenever the
    accumulated stall time within \l timeWindow exceeds \l stallTime.

    \sa CGroupStatus
*/

/*! \qmlsignal PressureStallInformation::triggered()

    Emitted when the accumulated stall time within \l timeWindow exceeds \l stallTime. Monitoring
    must be active, i.e. \l mode must be set to either \c Some or \c Full.

    \sa mode, timeWindow, stallTime
*/

PressureStallInformation::PressureStallInformation(CGroupStatus *cgroupStatus, Type type)
    : QObject(*new PressureStallInformationPrivate, cgroupStatus)
{
    Q_D(PressureStallInformation);
    d->m_cgroupType = type;
    d->m_cgroupStatus = cgroupStatus;

    connect(&d->m_notifier, &QSocketNotifier::activated,
        this, &PressureStallInformation::triggered);

    d->m_reconfigureTimer.setSingleShot(true);
    connect(&d->m_reconfigureTimer, &QTimer::timeout,
            this, [d]() { d->finishReconfigure(); });
}

/*! \qmlproperty enumeration PressureStallInformation::mode

    Controls whether PSI monitoring is enabled and which stall condition is observed.

    \value PressureStallInformation.Off
        PSI monitoring is disabled. This is the default.
    \value PressureStallInformation.Some
        Triggers when at least one task is stalled on the resource.
    \value PressureStallInformation.Full
        Triggers when all non-idle tasks are simultaneously stalled on the resource.

    \sa timeWindow, stallTime, triggered
*/
PressureStallInformation::Mode PressureStallInformation::mode() const
{
    Q_D(const PressureStallInformation);
    return d->m_mode;
}

/*! \qmlproperty enumeration PressureStallInformation::type

    The type of resource this PressureStallInformation instance monitors.

    \value PressureStallInformation.Cpu
        Monitors CPU pressure (tasks stalled waiting for CPU time).
    \value PressureStallInformation.Memory
        Monitors memory pressure (tasks stalled waiting for memory to become available).
    \value PressureStallInformation.Io
        Monitors I/O pressure (tasks stalled waiting for I/O operations to complete).

    This is a read-only property that is set when the instance is created. It can be used to
    distinguish between the different PSI types when accessing them via CGroupStatus.
*/
PressureStallInformation::Type PressureStallInformation::type() const
{
    Q_D(const PressureStallInformation);
    return d->m_cgroupType;
}

void PressureStallInformation::setMode(Mode mode)
{
    Q_D(PressureStallInformation);
    if (d->m_mode == mode)
        return;

    d->m_mode = mode;
    d->startReconfigure();
    emit modeChanged();
}

/*! \qmlproperty int PressureStallInformation::timeWindow

    The observation time window in milliseconds over which stall time is accumulated.
    When the accumulated stall time within this window exceeds \l stallTime, the \l triggered
    signal is emitted.

    \note Unprivileged users can only set time windows in multiples of 2000ms due to kernel
          limitations.

    \sa stallTime, triggered
*/
quint64 PressureStallInformation::timeWindow() const
{
    Q_D(const PressureStallInformation);
    return d->m_timeWindow.count();
}

void PressureStallInformation::setTimeWindow(quint64 timeWindow)
{
    Q_D(PressureStallInformation);
    const std::chrono::milliseconds newValue(timeWindow);
    if (d->m_timeWindow == newValue)
        return;

    d->m_timeWindow = newValue;
    d->startReconfigure();
    emit timeWindowChanged();
}

/*! \qmlproperty int PressureStallInformation::stallTime

    The stall time threshold in milliseconds. When the accumulated stall time within \l timeWindow
    exceeds this value, the \l triggered signal is emitted.

    \sa timeWindow, triggered
*/
quint64 PressureStallInformation::stallTime() const
{
    Q_D(const PressureStallInformation);
    return d->m_stallTime.count();
}

void PressureStallInformation::setStallTime(quint64 stallTime)
{
    Q_D(PressureStallInformation);
    const std::chrono::milliseconds newValue(stallTime);
    if (d->m_stallTime == newValue)
        return;

    d->m_stallTime = newValue;
    d->startReconfigure();
    emit stallTimeChanged();
}

void PressureStallInformationPrivate::startReconfigure()
{
#if defined(Q_OS_LINUX)
    if (m_eventFd != -1) {
        ::close(m_eventFd);
        m_eventFd = -1;
        m_notifier.setSocket(-1);
    }

    // make sure to reconfigure only once, even if multiple parameters are changed at once
    m_reconfigureTimer.start();
#endif
}

void PressureStallInformationPrivate::finishReconfigure()
{
#if defined(Q_OS_LINUX)
    // reconfigure the PSI monitoring with the new parameters
    Q_ASSERT(m_eventFd == -1);

    if (m_mode == PressureStallInformation::Mode::Off)
        return;

    const auto typeString = [this]() -> QString {
        switch (m_cgroupType) {
        case PressureStallInformation::Type::Cpu:    return u"cpu"_s;
        case PressureStallInformation::Type::Memory: return u"memory"_s;
        case PressureStallInformation::Type::Io:     return u"io"_s;
        default: Q_UNREACHABLE_RETURN(u""_s);
        }
    }();

    const QString path = u"%1/sys/fs/cgroup/%2/%3.pressure"_s
                             .arg(testRootPathPrefix())
                             .arg(m_cgroupStatus->path())
                             .arg(typeString);

    try {
        m_eventFd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);

        if (m_eventFd == -1) {
            throw Exception(errno, "Failed to open PSI fd for %1 pressure monitoring at %2")
                .arg(typeString).arg(path);
        }

        QByteArray data = ((m_mode == PressureStallInformation::Mode::Some) ? "some " : "full ")
                          + QByteArray::number(1000 * m_stallTime.count()) + " "
                          + QByteArray::number(1000 * m_timeWindow.count());

        if (::write(m_eventFd, data.constData(), data.size()) != data.size()) {
            QByteArray extra;
            if ((errno == EINVAL) && m_stallTime.count() && m_timeWindow.count()
                    && (m_timeWindow.count() % 2000) && (::geteuid() != 0)) {
                extra =" (this is most likely due to unprivileged users only being allowed to set"
                       " time windows in multiples of 2000ms)";
            }
            throw Exception(errno, "Failed to write PSI trigger data (%3) for %1 pressure monitoring to %2%4")
                .arg(typeString).arg(path).arg(data).arg(extra);
        }

        m_notifier.setSocket(m_eventFd);
        m_notifier.setEnabled(true);

    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        if (m_eventFd != -1) {
            ::close(m_eventFd);
            m_eventFd = -1;
        }
    }
#endif
}

QT_END_NAMESPACE_AM

#include "moc_cgroupstatus.cpp"
