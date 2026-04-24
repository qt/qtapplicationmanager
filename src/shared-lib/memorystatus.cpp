// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <mutex>

#include <QFile>
#include <private/qobject_p.h>

#if defined(Q_OS_LINUX)
#  include <unistd.h>
#elif defined(Q_OS_WINDOWS)
#  include <windows.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <mach/mach.h>
#  include <sys/sysctl.h>
#endif

#include "logging.h"
#include "memorystatus.h"
#include "utilities.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype MemoryStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the status of the RAM.

    MemoryStatus provides information on the status of the system's RAM (random-access memory).
    Its property values are updated whenever the method update() is called.

    \note This type only provides information about the total memory usage of the system, and does
          not take into account any cgroup-specific limits or usages on Linux. If you want to
          monitor the memory usage of a specific cgroup, use the CGroupStatus type instead.

    You can use this component as a MonitorModel data source if you want to plot its previous
    values over time.

    \qml
    import QtQuick
    import QtApplicationManager
    ...
    MonitorModel {
        MemoryStatus {}
    }
    \endqml

    You can also use it alongside a Timer for instance, when you're only interested in its current
    value.

    \qml
    import QtQuick
    import QtApplicationManager
    ...
    MemoryStatus { id: memoryStatus }
    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: memoryStatus.update()
    }
    Text {
        text: "memory used: " + (memoryStatus.memoryUsed / 1e6).toFixed(0) + " MB"
    }
    \endqml
*/


class MemoryStatusPrivate : public QObjectPrivate
{
public:
    quint64 read();

    quint64 m_memoryUsed = 0u;
    static quint64 s_totalMemory;
    static quint64 s_pageSize;

#if defined(Q_OS_LINUX)
    QFile m_meminfoFile;
#endif
};

quint64 MemoryStatusPrivate::s_totalMemory = 0u;
quint64 MemoryStatusPrivate::s_pageSize = 0u;

MemoryStatus::MemoryStatus(QObject *parent)
    : QObject(*new MemoryStatusPrivate, parent)
{
    Q_D(MemoryStatus);

    std::once_flag once;
    std::call_once(once, [] {
#if defined(Q_OS_LINUX)
        long pageSize = ::sysconf(_SC_PAGESIZE);
        long physPages = ::sysconf(_SC_PHYS_PAGES);

        if ((pageSize <= 0) || (physPages <= 0)) {
            qCCritical(LogSystem) << "Cannot determine the amount of physical RAM in this machine.";
        } else {
            MemoryStatusPrivate::s_totalMemory = quint64(physPages) * quint64(pageSize);
            MemoryStatusPrivate::s_pageSize = pageSize;
        }

#elif defined(Q_OS_WINDOWS)
        ::MEMORYSTATUSEX mem;
        mem.dwLength = sizeof(mem);
        if (!::GlobalMemoryStatusEx(&mem))
            qCCritical(LogSystem) << "Cannot determine the amount of physical RAM in this machine.";
        MemoryStatusPrivate::s_totalMemory = mem.ullTotalPhys;

        ::SYSTEM_INFO sys;
        ::GetSystemInfo(&sys);
        MemoryStatusPrivate::s_pageSize = sys.dwPageSize;

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        std::array<int, 2> mib{CTL_HW, HW_MEMSIZE};
        int64_t hwMem;
        size_t hwMemSize = sizeof(hwMem);

        if (::sysctl(mib.data(), mib.size(), &hwMem, &hwMemSize, nullptr, 0) == 0)
            MemoryStatusPrivate::s_totalMemory = quint64(hwMem);

        mib[1] = HW_PAGESIZE;
        int hwPageSize;
        size_t hwPageSizeSize = sizeof(hwPageSize);

        if (::sysctl(mib.data(), mib.size(), &hwPageSize, &hwPageSizeSize, nullptr, 0) == 0)
            MemoryStatusPrivate::s_pageSize = hwPageSize;
#endif
    });

#if defined(Q_OS_LINUX)
    d->m_meminfoFile.setFileName(testRootPathPrefix() + u"/proc/meminfo");
    if (!d->m_meminfoFile.open(QIODevice::ReadOnly))
        qCCritical(LogSystem) << "Cannot read memory statistics from" << d->m_meminfoFile.fileName();
#endif

    d->m_memoryUsed = d->read(); // retrieve initial value
}

/*!
    \qmlproperty int MemoryStatus::totalMemory
    \readonly

    The total amount of physical memory (RAM) installed on the system in bytes.

    \sa memoryUsed
*/
quint64 MemoryStatus::totalMemory() const
{
    return MemoryStatusPrivate::s_totalMemory;
}

/*!
    \qmlproperty int MemoryStatus::memoryUsed
    \readonly

    The amount of physical memory (RAM) used in bytes.

    The value of this property is updated when \l update is called.

    \sa totalMemory
*/
quint64 MemoryStatus::memoryUsed() const
{
    Q_D(const MemoryStatus);
    return d->m_memoryUsed;
}

/*!
    \qmlproperty list<string> MemoryStatus::roleNames
    \readonly

    Names of the roles provided by MemoryStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList MemoryStatus::roleNames() const
{
    return { u"memoryUsed"_s };
}

/*!
    \qmlmethod void MemoryStatus::update()

    Updates the memoryUsed property.

    \sa memoryUsed
*/
void MemoryStatus::update()
{
    Q_D(MemoryStatus);

    quint64 newReading = d->read();

    if (d->m_memoryUsed != newReading) {
        d->m_memoryUsed = newReading;
        emit memoryUsedChanged();
    }
}
quint64 MemoryStatusPrivate::read()
{
#if defined(Q_OS_LINUX)
    if (!m_meminfoFile.isOpen())
        return 0u;
    m_meminfoFile.seek(0);
    const QByteArray buffer = m_meminfoFile.read(1500);

    static constexpr char memAvailable[] = "MemAvailable: ";
    if (auto i = buffer.indexOf(memAvailable); i != -1) {
        auto available = ::strtoull(buffer.data() + i + sizeof(memAvailable) - 1, nullptr, 10);
        return s_totalMemory - 1024 * available;
    } else {
        return 0u;
    }

#elif defined(Q_OS_WINDOWS)
    ::MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (!::GlobalMemoryStatusEx(&mem))
        return 0u;
    return mem.ullTotalPhys - mem.ullAvailPhys;

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    ::vm_statistics64_data_t vmStat;
    ::mach_msg_type_number_t vmStatCount = HOST_VM_INFO64_COUNT;

    if (::host_statistics64(::mach_host_self(),
                            HOST_VM_INFO64,
                            reinterpret_cast< ::host_info64_t>(&vmStat),
                            &vmStatCount)
        == 0) {
        quint64 app = vmStat.internal_page_count;
        quint64 compressed = vmStat.compressor_page_count;
        quint64 wired = vmStat.wire_count;

        return (app + compressed + wired) * s_pageSize;
    } else {
        return 0u;
    }
#else
    return 0u;
#endif
}

QT_END_NAMESPACE_AM

#include "moc_memorystatus.cpp"
