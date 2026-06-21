// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <cerrno>
#include <chrono>

#include <QSocketNotifier>
#include <QTimer>
#include <private/qobject_p.h>

#if defined(Q_OS_LINUX)
#  include <QtCore/private/qcore_unix_p.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include "unix-utilities.h"
#endif

#include "exception.h"
#include "logging.h"
#include "pressurestallinformation.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

class PressureStallInformationPrivate : public QObjectPrivate
{
public:
#if defined(Q_OS_LINUX)
    Unix::Fd m_eventFd;
#endif

    void startReconfigure();
    void finishReconfigure();
    void setActive(bool active);

    PressureStallInformation::Mode m_mode = PressureStallInformation::Mode::Off;
    bool m_active = false;
    std::chrono::milliseconds m_timeWindow { };
    std::chrono::milliseconds m_stallTime { };
    QTimer m_reconfigureTimer;
    QSocketNotifier m_notifier { QSocketNotifier::Exception };

    PressureStallInformation::Type m_type { };
    QString m_pressureFile;

    Q_DECLARE_PUBLIC(PressureStallInformation)
};

/*!
    \qmltype PressureStallInformation
    \inqmlmodule QtApplicationManager
    \ingroup common-non-instantiable
    \brief Provides Linux PSI (Pressure Stall Information) monitoring for a resource.

    This type cannot be instantiated directly. Instances are accessed via the
    \l CGroupStatus::cpuPSI, \l CGroupStatus::memoryPSI, \l CGroupStatus::ioPSI,
    \l SystemStatus::cpuPSI, \l SystemStatus::memoryPSI, and \l SystemStatus::ioPSI properties.

    \l{Linux Pressure Stall Information}{Linux PSI} monitoring (available since kernel 4.20 with
    \l{Linux cgroup v2}{cgroup v2}, and system-wide via \c{/proc/pressure/}) detects resource
    pressure by tracking the time tasks spend stalled waiting for a resource. Set \l mode to
    \c Some or \c Full and configure \l timeWindow and \l stallTime to define the trigger
    threshold. The \l triggered signal fires whenever the accumulated stall time within
    \l timeWindow exceeds \l stallTime.

    \sa CGroupStatus, SystemStatus
*/

/*! \qmlsignal PressureStallInformation::triggered()

    Emitted when the accumulated stall time within \l timeWindow exceeds \l stallTime. Monitoring
    must be active, i.e. \l mode must be set to either \c Some or \c Full.

    \sa mode, timeWindow, stallTime
*/

PressureStallInformation::PressureStallInformation(Type type, QObject *parent)
    : QObject(*new PressureStallInformationPrivate, parent)
{
    Q_D(PressureStallInformation);
    d->m_type = type;

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
    distinguish between the different PSI types when accessing them via CGroupStatus or
    SystemStatus.
*/
PressureStallInformation::Type PressureStallInformation::type() const
{
    Q_D(const PressureStallInformation);
    return d->m_type;
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

/*! \qmlproperty bool PressureStallInformation::active
    \readonly

    Holds whether PSI monitoring is currently armed, i.e. the kernel trigger has been successfully
    set up and the \l triggered signal can fire. This becomes \c true once \l mode is set to
    \c Some or \c Full and the trigger could be installed on the underlying pressure file, and
    \c false again when \l mode is set to \c Off or when arming the trigger fails (for example,
    because the pressure file is unavailable or the requested \l timeWindow is rejected by the
    kernel).

    \sa mode, triggered
*/
bool PressureStallInformation::isActive() const
{
    Q_D(const PressureStallInformation);
    return d->m_active;
}

void PressureStallInformation::setPressureFile(const QString &path)
{
    Q_D(PressureStallInformation);
    if (d->m_pressureFile == path)
        return;

    d->m_pressureFile = path;
    // if monitoring is currently active, point the kernel at the new file
    if (d->m_mode != Mode::Off)
        d->startReconfigure();
}

void PressureStallInformationPrivate::setActive(bool active)
{
    Q_Q(PressureStallInformation);
    if (m_active == active)
        return;
    m_active = active;
    emit q->activeChanged();
}

void PressureStallInformationPrivate::startReconfigure()
{
#if defined(Q_OS_LINUX)
    m_eventFd.reset();
    m_notifier.setSocket(-1);
    setActive(false);

    // make sure to reconfigure only once, even if multiple parameters are changed at once
    m_reconfigureTimer.start();
#endif
}

void PressureStallInformationPrivate::finishReconfigure()
{
#if defined(Q_OS_LINUX)
    // reconfigure the PSI monitoring with the new parameters
    Q_ASSERT(!m_eventFd);

    if (m_mode == PressureStallInformation::Mode::Off)
        return;
    if (m_pressureFile.isEmpty())
        return;

    const auto typeString = [this]() -> QString {
        switch (m_type) {
        case PressureStallInformation::Type::Cpu:    return u"cpu"_s;
        case PressureStallInformation::Type::Memory: return u"memory"_s;
        case PressureStallInformation::Type::Io:     return u"io"_s;
        default: Q_UNREACHABLE_RETURN(u""_s);
        }
    }();

    try {
        m_eventFd.reset(qt_safe_open(m_pressureFile.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC));

        if (!m_eventFd) {
            throw Exception(errno, "Failed to open PSI fd for %1 pressure monitoring at %2")
                .arg(typeString).arg(m_pressureFile);
        }

        QByteArray data = ((m_mode == PressureStallInformation::Mode::Some) ? "some " : "full ")
                          + QByteArray::number(1000 * m_stallTime.count()) + " "
                          + QByteArray::number(1000 * m_timeWindow.count())
                          + '\n'; // needed, because the kernel will clobber the last character

        if (qt_safe_write(m_eventFd.get(), data.constData(), data.size()) != data.size()) {
            QByteArray extra;
            if ((errno == EINVAL) && m_stallTime.count() && m_timeWindow.count()
                    && (m_timeWindow.count() % 2000) && (::geteuid() != 0)) {
                extra =" (this is most likely due to unprivileged users only being allowed to set"
                       " time windows in multiples of 2000ms)";
            }
            throw Exception(errno, "Failed to write PSI trigger data (%3) for %1 pressure monitoring to %2%4")
                .arg(typeString).arg(m_pressureFile).arg(data).arg(extra);
        }

        m_notifier.setSocket(m_eventFd.get());
        m_notifier.setEnabled(true);
        setActive(true);

    } catch (const Exception &e) {
        qCWarning(LogSystem) << e.what();
        m_eventFd.reset();
    }
#endif
}

QT_END_NAMESPACE_AM

#include "moc_pressurestallinformation.cpp"
