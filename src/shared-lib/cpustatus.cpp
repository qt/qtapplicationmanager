// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QFile>
#include <QThread>
#include <private/qobject_p.h>

#include "logging.h"
#include "cpustatus.h"
#include "utilities.h"


#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <mach/mach.h>
#  include <sys/sysctl.h>
#endif

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype CpuStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the CPU status.

    As the name implies, CpuStatus provides information on the status of the CPU. Its property
    values are updated whenever the method update() is called.

    You can use this component as a MonitorModel data source if you want to plot its previous
    values over time.

    \qml
    import QtQuick
    import QtApplicationManager
    ...
    MonitorModel {
        CpuStatus {}
    }
    \endqml

    You can also use it alongside a Timer for instance, when you're only interested in its current
    value.

    \qml
    import QtQuick
    import QtApplicationManager
    ...
    CpuStatus { id: cpuStatus }
    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: cpuStatus.update()
    }
    Text {
        property string loadPercent: Number(cpuStatus.cpuLoad * 100).toLocaleString(Qt.locale("en_US"), 'f', 1)
        text: "cpuLoad: " + loadPercent + "%"
    }
    \endqml
*/

class CpuStatusPrivate : public QObjectPrivate
{
public:
    qreal read();

    qint64 m_lastIdle = 0;
    qint64 m_lastTotal = 0;
    qreal m_cpuLoad = 0;

#if defined(Q_OS_LINUX)
    QFile m_statFile;
#endif
};

CpuStatus::CpuStatus(QObject *parent)
    : QObject(*new CpuStatusPrivate, parent)
{
    Q_D(CpuStatus);

#if defined(Q_OS_LINUX)
    d->m_statFile.setFileName(testRootPathPrefix() + u"/proc/stat");
    if (!d->m_statFile.open(QIODevice::ReadOnly))
        qCWarning(LogSystem) << "Cannot read CPU statistics from" << d->m_statFile.fileName();
#endif

    d->m_cpuLoad = d->read();
}

/*!
    \qmlproperty real CpuStatus::cpuLoad
    \readonly

    Holds the overall system's CPU utilization at the point when update() was last called, as a
    value ranging from \c 0 (inclusive, completely idle) to \c 1 (inclusive, fully busy).

    \sa update
*/
qreal CpuStatus::cpuLoad() const
{
    Q_D(const CpuStatus);
    return d->m_cpuLoad;
}

/*!
    \qmlproperty int CpuStatus::cpuCores
    \readonly

    The number of physical CPU cores that are installed on the system.
*/
int CpuStatus::cpuCores() const
{
    return QThread::idealThreadCount();
}

/*!
    \qmlmethod void CpuStatus::update()

    Updates the cpuLoad property.

    \sa cpuLoad
*/
void CpuStatus::update()
{
    Q_D(CpuStatus);

    qreal newReading = d->read();

    if (!qFuzzyCompare(newReading, d->m_cpuLoad)) {
        d->m_cpuLoad = newReading;
        emit cpuLoadChanged();
    }
}

qreal CpuStatusPrivate::read()
{
    auto totalIdle = [this]() -> std::optional<std::pair<qint64, qint64>> {
#if defined(Q_OS_LINUX)
        if (!m_statFile.isOpen())
            return { };
        m_statFile.seek(0);
        const QByteArray buffer = m_statFile.readAll();

        qsizetype pos = 0;
        qint64 total = 0;
        QVector<qint64> values;

        while (pos < buffer.size() && values.size() < 4) {
            if (!::isdigit(buffer.at(pos))) {
                ++pos;
                continue;
            }

            char *endPtr = nullptr;
            qint64 val = ::strtoll(buffer.constData() + pos,
                                   &endPtr,
                                   10); // check missing for over-/underflow
            values << val;
            total += val;
            pos = qsizetype(endPtr - buffer.constData() + 1);
        }

        if (values.size() < 4)
            return { };

        qint64 idle = values.at(3);
        return { { total, idle } };

#elif defined(Q_OS_WIN)
        Q_UNUSED(this)

        auto winFileTimeToInt64 = [](const ::FILETIME &filetime) {
            return ((quint64(filetime.dwHighDateTime) << 32) | quint64(filetime.dwLowDateTime));
        };

        ::FILETIME winIdle, winKernel, winUser;
        if (!::GetSystemTimes(&winIdle, &winKernel, &winUser))
            return { };
        return { { winFileTimeToInt64(winKernel) + winFileTimeToInt64(winUser),
                   winFileTimeToInt64(winIdle) } };

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        Q_UNUSED(this)

        ::natural_t cpuCount = 0;
        ::processor_cpu_load_info_t cpuLoadInfo;
        ::mach_msg_type_number_t cpuLoadInfoCount = 0;

        if (::host_processor_info(::mach_host_self(),
                                  PROCESSOR_CPU_LOAD_INFO,
                                  &cpuCount,
                                  reinterpret_cast<::processor_info_array_t *>(&cpuLoadInfo),
                                  &cpuLoadInfoCount) != 0) {
            return { };
        }

        qint64 idle = 0, total = 0;

        for (natural_t i = 0; i < cpuCount; ++i) {
            idle += cpuLoadInfo[i].cpu_ticks[CPU_STATE_IDLE];
            total += cpuLoadInfo[i].cpu_ticks[CPU_STATE_USER]
                     + cpuLoadInfo[i].cpu_ticks[CPU_STATE_SYSTEM]
                     + cpuLoadInfo[i].cpu_ticks[CPU_STATE_IDLE]
                     + cpuLoadInfo[i].cpu_ticks[CPU_STATE_NICE];
        }
        ::vm_deallocate(::mach_task_self(),
                        reinterpret_cast<::vm_address_t>(cpuLoadInfo),
                        cpuLoadInfoCount);

        return { { total, idle } };

#else
        return { };
#endif
    }();

    if (!totalIdle)
        return 0;

    qint64 total = totalIdle->first;
    qint64 idle = totalIdle->second;

    if (total == m_lastTotal) // this can happen in unit tests: prevent division by zero
        return { };

    qreal newLoad = qreal(1) - (qreal(idle - m_lastIdle) / qreal(total - m_lastTotal));

    m_lastIdle = idle;
    m_lastTotal = total;

    return newLoad;
}

/*!
    \qmlproperty list<string> CpuStatus::roleNames
    \readonly

    Names of the roles provided by CpuStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList CpuStatus::roleNames() const
{
    return { u"cpuLoad"_s };
}

QT_END_NAMESPACE_AM

#include "moc_cpustatus.cpp"
