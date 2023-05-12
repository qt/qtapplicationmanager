/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#include <qglobal.h>

#include "systemreader.h"
#include "global.h"
#include "logging.h"

QT_BEGIN_NAMESPACE_AM

#if defined(Q_OS_LINUX)
QString g_systemRootDir(qSL("/"));
#endif

quint64 MemoryReader::s_totalValue = 0;

quint64 MemoryReader::totalValue() const
{
    return s_totalValue;
}

QT_END_NAMESPACE_AM


#if defined(Q_OS_LINUX)

#  include "sysfsreader.h"
#  include <qplatformdefs.h>
#  include <QElapsedTimer>
#  include <QFile>
#  include <QSocketNotifier>
#  include <QProcess>
#  include <QCoreApplication>
#  include <QAtomicInteger>
#  include <QOffscreenSurface>
#  include <QOpenGLContext>
#  include <QOpenGLFunctions>

#  include <sys/eventfd.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <errno.h>
#  include <stdio.h>

QT_BEGIN_NAMESPACE_AM

#if defined(Q_OS_ANDROID) && (_POSIX_C_SOURCE < 200809L)

#  include <stdarg.h>

int dprintf(int fd, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char **buffer = nullptr;
    int len = vasprintf(buffer, format, ap);
    va_end(ap);

    if (*buffer && (len > 0))
        len = QT_WRITE(fd, *buffer, len);
    if (*buffer)
        free(*buffer);
    return len;
}

#endif

QScopedPointer<SysFsReader> CpuReader::s_sysFs;

CpuReader::CpuReader()
{
    if (!s_sysFs) {
        s_sysFs.reset(new SysFsReader("/proc/stat", 256));
        if (!s_sysFs->isOpen())
            qCWarning(LogSystem) << "WARNING: could not read CPU statistics from" << s_sysFs->fileName();
    }
}

qreal CpuReader::readLoadValue()
{
    QByteArray str = s_sysFs->readValue();

    int pos = 0;
    qint64 total = 0;
    QVector<qint64> values;

    while (pos < str.size() && values.size() < 4) {
        if (!isdigit(str.at(pos))) {
            ++pos;
            continue;
        }

        char *endPtr = nullptr;
        qint64 val = strtoll(str.constData() + pos, &endPtr, 10); // check missing for over-/underflow
        values << val;
        total += val;
        pos = int(endPtr - str.constData() + 1);
    }

    if (values.size() >= 4) {
        qint64 idle = values.at(3);

        m_load = qreal(1) - (qreal(idle - m_lastIdle) / qreal(total - m_lastTotal));

        m_lastIdle = idle;
        m_lastTotal = total;
    } else {
        m_load = qreal(1);
    }

    return m_load;
}

GpuVendor::Vendor GpuVendor::s_vendor = GpuVendor::Undefined;

GpuVendor::Vendor GpuVendor::get()
{
    if (s_vendor == Undefined)
        fetch();

    return s_vendor;
}

void GpuVendor::fetch()
{
    QByteArray vendor;
#if !defined(QT_NO_OPENGL)
    auto readVendor = [&vendor](QOpenGLContext *c) {
        const GLubyte *p = c->functions()->glGetString(GL_VENDOR);
        if (p)
            vendor = QByteArray(reinterpret_cast<const char *>(p)).toLower();
    };

    if (QOpenGLContext::currentContext()) {
        readVendor(QOpenGLContext::currentContext());
    } else {
        QOpenGLContext context;
        if (context.create()) {
            QOffscreenSurface surface;
            surface.setFormat(context.format());
            surface.create();
            context.makeCurrent(&surface);
            readVendor(&context);
            context.doneCurrent();
        }
    }
#endif
    if (vendor.contains("intel"))
        s_vendor = Intel;
    else if (vendor.contains("nvidia"))
        s_vendor = Nvidia;
    else
        s_vendor = Unsupported;
}

class GpuTool : protected QProcess
{
public:
    GpuTool()
        : QProcess(qApp)
    {
        if (GpuVendor::get() == GpuVendor::Intel) {
            setProgram(qSL("intel_gpu_top"));
            setArguments({ qSL("-o-"), qSL("-s 1000") });
        } else if (GpuVendor::get() == GpuVendor::Nvidia) {
            setProgram(qSL("nvidia-smi"));
            setArguments({ qSL("dmon"), qSL("--select"), qSL("u") });
        }

        connect(this, static_cast<void(QProcess::*)(QProcess::ProcessError error)>(&QProcess::errorOccurred),
                this, [this](QProcess::ProcessError error) {
            if (m_refCount)
                qCWarning(LogSystem) << "GPU monitoring tool:" << program() << "caused error:" << error;
        });

        connect(this, &QProcess::readyReadStandardOutput, this, [this]() {
            while (canReadLine()) {
                const QByteArray str = readLine();
                if (str.isEmpty() || (str.at(0) == '#'))
                    continue;

                int pos = (GpuVendor::get() == GpuVendor::Intel) ? 50 : 0;
                QVector<qreal> values;

                while (pos < str.size() && values.size() < 2) {
                    if (isspace(str.at(pos))) {
                        ++pos;
                        continue;
                    }
                    char *endPtr = nullptr;
#if defined(Q_OS_ANDROID)
                    qreal val = strtod(str.constData() + pos, &endPtr); // check missing for over-/underflow
#else
                    static locale_t cLocale = newlocale(LC_ALL_MASK, "C", nullptr);
                    qreal val = strtod_l(str.constData() + pos, &endPtr, cLocale); // check missing for over-/underflow
#endif
                    values << val;
                    pos = int(endPtr - str.constData() + 1);
                }

                switch (GpuVendor::get()) {
                case GpuVendor::Intel:
                    if (values.size() > 0)
                        m_lastValue = values.at(0) / 100;
                    break;
                case GpuVendor::Nvidia:
                    if (values.size() > 1) {
                        if (qFuzzyIsNull(values.at(0)))  // hardcoded to first gfx card
                            m_lastValue = values.at(1) / 100;
                    }
                    break;
                default:
                    m_lastValue = -1;
                    break;
                }
            }
        });
    }

    void ref()
    {
        if (m_refCount.ref() && !isRunning())
            start(QIODevice::ReadOnly);
    }

    void deref()
    {
        if (!m_refCount.deref() && isRunning()) {
            kill();
            waitForFinished();
        }
    }

    bool isRunning() const
    {
        return (state() == QProcess::Running);
    }

    qreal loadValue() const
    {
        return m_lastValue;
    }

private:
    QAtomicInteger<int> m_refCount;
    qreal m_lastValue = 0;
};

GpuTool *GpuReader::s_gpuToolProcess = nullptr;

GpuReader::GpuReader()
{ }

void GpuReader::setActive(bool enabled)
{
    if (!s_gpuToolProcess)
        s_gpuToolProcess = new GpuTool();

    if (GpuVendor::get() == GpuVendor::Unsupported) {
        qCWarning(LogSystem) << "GPU monitoring is not supported on this platform.";
    } else {
        if (enabled)
            s_gpuToolProcess->ref();
        else
            s_gpuToolProcess->deref();
    }
}

bool GpuReader::isActive() const
{
    return s_gpuToolProcess ? s_gpuToolProcess->isRunning() : false;
}

qreal GpuReader::readLoadValue()
{
    return s_gpuToolProcess ? s_gpuToolProcess->loadValue() : -1;
}

// TODO: can we always expect cgroup FS to be mounted on /sys/fs/cgroup?
static const QString cGroupsMemoryBaseDir = qSL("/sys/fs/cgroup/memory/");

MemoryReader::MemoryReader() : MemoryReader(QString())
{ }

MemoryReader::MemoryReader(const QString &groupPath)
    : m_groupPath(groupPath)
{
    const QString path = g_systemRootDir + cGroupsMemoryBaseDir + m_groupPath + qSL("/memory.stat");

    m_sysFs.reset(new SysFsReader(path.toLocal8Bit(), 1500));
    if (!m_sysFs->isOpen()) {
        qCWarning(LogSystem) << "WARNING: could not read memory statistics from" << m_sysFs->fileName()
                             << "(make sure that the memory cgroup is mounted)";
    }

    // initialize s_totalValue first time a MemoryReader is instantiated
    static int dummy = []() {
        long pageSize = ::sysconf(_SC_PAGESIZE);
        long physPages = ::sysconf(_SC_PHYS_PAGES);

        if (pageSize < 0 || physPages < 0)
            qCWarning(LogSystem) << "WARNING: Cannot determine the amount of physical RAM in this machine.";
        else
            s_totalValue = quint64(physPages) * quint64(pageSize);
        return 0;
    }();
    Q_UNUSED(dummy)
}

quint64 MemoryReader::groupLimit()
{
    QString path = g_systemRootDir + cGroupsMemoryBaseDir + m_groupPath + qSL("/memory.limit_in_bytes");
    QByteArray ba = SysFsReader(path.toLocal8Bit(), 41).readValue();
    return ::strtoull(ba, nullptr, 10);
}

quint64 MemoryReader::readUsedValue() const
{
    QByteArray buffer = m_sysFs->readValue();

    int i = buffer.indexOf("total_rss ");
    if (i == -1)
        return 0;
    return ::strtoull(buffer.data() + i + sizeof("total_rss ")-1, nullptr, 10);
}


IoReader::IoReader(const char *device)
    : m_sysFs(new SysFsReader(QByteArray("/sys/block/") + device + "/stat", 256))
{
    if (!m_sysFs->isOpen())
        qCWarning(LogSystem) << "WARNING: could not read I/O statistics from" << m_sysFs->fileName();
}

IoReader::~IoReader()
{ }

qreal IoReader::readLoadValue()
{
    QByteArray str = m_sysFs->readValue();

    int pos = 0;
    int total = 0;
    QVector<qint64> values;

    while (pos < str.size() && values.size() < 11) {
        if (!isdigit(str.at(pos))) {
            ++pos;
            continue;
        }

        char *endPtr = nullptr;
        qint64 val = strtoll(str.constData() + pos, &endPtr, 10); // check missing for over-/underflow
        values << val;
        total += val;
        pos = int(endPtr - str.constData() + 1);
    }

    qint64 elapsed;
    if (m_lastCheck.isValid()) {
        elapsed = m_lastCheck.restart();
    } else {
        elapsed = -1;
        m_lastCheck.start();
    }

    if (values.size() >= 11) {
        qint64 ioTime = values.at(9);

        m_load = qreal(ioTime - m_lastIoTime) / elapsed;
        m_lastIoTime = ioTime;
    } else {
        m_load = qreal(1);
    }

    return m_load;
}


MemoryThreshold::MemoryThreshold(const QList<qreal> &thresholds)
    : m_thresholds(thresholds)
{ }

MemoryThreshold::~MemoryThreshold()
{
    if (m_usageFd != -1)
        QT_CLOSE(m_usageFd);
    if (m_controlFd != -1)
        QT_CLOSE(m_controlFd);
    if (m_eventFd != -1)
        QT_CLOSE(m_eventFd);
}

QList<qreal> MemoryThreshold::thresholdPercentages() const
{
    return m_thresholds;
}

bool MemoryThreshold::isEnabled() const
{
    return m_enabled;
}

bool MemoryThreshold::setEnabled(bool enabled)
{
    MemoryReader reader;
    return setEnabled(enabled, QString(), &reader);
}

bool MemoryThreshold::setEnabled(bool enabled, const QString &groupPath, MemoryReader *reader)
{
    if (m_enabled == enabled)
        return true;

    if (enabled && !m_initialized) {
        quint64 limit = groupPath.isEmpty() ? reader->totalValue() : reader->groupLimit();
        const QString cGroup = cGroupsMemoryBaseDir + groupPath;

        m_eventFd = ::eventfd(0, EFD_CLOEXEC);

        if (m_eventFd >= 0) {
            const QString usagePath = cGroup + qL1S("/memory.usage_in_bytes");
            m_usageFd = QT_OPEN(usagePath.toLocal8Bit().constData(), QT_OPEN_RDONLY);

            if (m_usageFd >= 0) {
                const QString eventControlPath = cGroup + qSL("/cgroup.event_control");
                m_controlFd = QT_OPEN(eventControlPath.toLocal8Bit().constData(), QT_OPEN_WRONLY);

                if (m_controlFd >= 0) {
                    bool registerOk = true;

                    for (qreal percent : qAsConst(m_thresholds)) {
                        quint64 mem = quint64(limit * percent) / 100;
                        registerOk = registerOk && (dprintf(m_controlFd, "%d %d %llu", m_eventFd, m_usageFd, mem) > 0);
                    }

                    if (registerOk) {
                        m_notifier = new QSocketNotifier(m_eventFd, QSocketNotifier::Read, this);
                        connect(m_notifier, &QSocketNotifier::activated, this, &MemoryThreshold::readEventFd);
                        return m_initialized = m_enabled = true;
                    } else {
                        qWarning() << "Could not register memory limit event handlers";
                    }

                    QT_CLOSE(m_controlFd);
                    m_controlFd = -1;
                } else {
                    qWarning() << "Cannot open" << eventControlPath;
                }

                QT_CLOSE(m_usageFd);
                m_usageFd = -1;
            } else {
                qWarning() << "Cannot open" << usagePath;
            }

            QT_CLOSE(m_eventFd);
            m_eventFd = -1;
        } else {
            qWarning() << "Cannot create an eventfd";
        }

        return false;
    } else {
        m_enabled = enabled;
        m_notifier->setEnabled(enabled);

        return true;
    }
}

void MemoryThreshold::readEventFd()
{
    if (m_eventFd >= 0) {
        int handled = 0;
        quint64 counter;

        ssize_t r = QT_READ(m_eventFd, &counter, sizeof(counter));
        if (r == sizeof(counter)) {
            handled++;
        } else if (r < 0) {
            if (errno == EWOULDBLOCK)
                return;
             qWarning() << "Error reading from eventFD:" << strerror(errno);
        } else if (r == 0) {
             return;
        } else {
             qWarning() << "Short read" << r << "on eventFD!";
        }

        emit thresholdTriggered();
    }
}

MemoryWatcher::MemoryWatcher(QObject *parent)
    : QObject(parent)
{ }

void MemoryWatcher::setThresholds(qreal warning, qreal critical)
{
    m_warning = warning;
    m_critical = critical;
}

bool MemoryWatcher::startWatching(const QString &groupPath)
{
    if (m_warning < 0.0 || m_warning > 100.0 || m_critical < 0.0 || m_critical > 100.0) {
        qCWarning(LogSystem) << "Memory threshold out of range [0..100]" << m_warning << m_critical;
        return false;
    }

    hasMemoryLowWarning = false;
    hasMemoryCriticalWarning = false;

    m_reader.reset(new MemoryReader(groupPath));
    m_memLimit = groupPath.isEmpty() ? m_reader->totalValue() : m_reader->groupLimit();

    m_threshold.reset(new MemoryThreshold({m_warning, m_critical}));
    connect(m_threshold.data(), &MemoryThreshold::thresholdTriggered, this, &MemoryWatcher::checkMemoryConsumption);
    return m_threshold->setEnabled(true, groupPath, m_reader.data());
}

void MemoryWatcher::checkMemoryConsumption()
{
    qreal percentUsed = m_reader->readUsedValue() / m_memLimit * 100.0;
    bool nowMemoryCritical = (percentUsed >= m_critical);
    bool nowMemoryLow = (percentUsed >= m_warning);
    if (nowMemoryCritical  && !hasMemoryCriticalWarning)
        emit memoryCritical();
    if (nowMemoryLow && !hasMemoryLowWarning)
        emit memoryLow();
    hasMemoryCriticalWarning = nowMemoryCritical;
    hasMemoryLowWarning = nowMemoryLow;
}

QMap<QByteArray, QByteArray> fetchCGroupProcessInfo(qint64 pid)
{
    QMap<QByteArray, QByteArray> result;

    auto cgroupPath = QString(qSL("%1/proc/%2/cgroup"))
        .arg(g_systemRootDir)
        .arg(pid);

    QFile file(cgroupPath);
    if (!file.open(QIODevice::ReadOnly))
        return result;

    char textBuffer[250];
    while (file.readLine(textBuffer, sizeof(textBuffer)) > 0) {
        // NB: we don't want the ending '\n' character, hence the -1
        auto fields = QByteArray::fromRawData(textBuffer, strlen(textBuffer)-1).split(':');
        if (fields.size() == 3) {
            result[fields[1]] = fields[2];
        }
    }

    return result;
}

QT_END_NAMESPACE_AM

#elif defined(Q_OS_WIN)

#include <windows.h>

QT_BEGIN_NAMESPACE_AM

CpuReader::CpuReader()
{ }

qreal CpuReader::readLoadValue()
{
    auto winFileTimeToInt64 = [](const FILETIME &filetime) {
        return ((quint64(filetime.dwHighDateTime) << 32) | quint64(filetime.dwLowDateTime));
    };

    FILETIME winIdle, winKernel, winUser;
    if (GetSystemTimes(&winIdle, &winKernel, &winUser)) {
        qint64 idle = winFileTimeToInt64(winIdle);
        qint64 total = winFileTimeToInt64(winKernel) + winFileTimeToInt64(winUser);

        m_load = qreal(1) - (qreal(idle - m_lastIdle) / qreal(total - m_lastTotal));

        m_lastIdle = idle;
        m_lastTotal = total;
    } else {
        m_load = qreal(1);
    }
    return m_load;
}

MemoryReader::MemoryReader()
{
    if (!s_totalValue) {
        MEMORYSTATUSEX mem;
        mem.dwLength = sizeof(mem);
        if (!GlobalMemoryStatusEx(&mem)) {
            qCCritical(LogSystem) << "Cannot determine the amount of physical RAM in this machine.";
            exit(42);
        }
        s_totalValue = mem.ullTotalPhys;
    }
}

quint64 MemoryReader::readUsedValue() const
{
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (!GlobalMemoryStatusEx(&mem))
        return 0;
    return mem.ullTotalPhys - mem.ullAvailPhys;
}

QT_END_NAMESPACE_AM

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)

#include <mach/mach.h>
#include <sys/sysctl.h>

QT_BEGIN_NAMESPACE_AM

CpuReader::CpuReader()
{ }

qreal CpuReader::readLoadValue()
{
    natural_t cpuCount = 0;
    processor_cpu_load_info_t cpuLoadInfo;
    mach_msg_type_number_t cpuLoadInfoCount = 0;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpuCount,
                            reinterpret_cast<processor_info_array_t *>(&cpuLoadInfo),
                            &cpuLoadInfoCount) == KERN_SUCCESS) {
        qint64 idle = 0, total = 0;

        for (natural_t i = 0; i < cpuCount; ++i) {
            idle += cpuLoadInfo[i].cpu_ticks[CPU_STATE_IDLE];
            total += cpuLoadInfo[i].cpu_ticks[CPU_STATE_USER] \
                    + cpuLoadInfo[i].cpu_ticks[CPU_STATE_SYSTEM] \
                    + cpuLoadInfo[i].cpu_ticks[CPU_STATE_IDLE] \
                    + cpuLoadInfo[i].cpu_ticks[CPU_STATE_NICE];
        }
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpuLoadInfo), cpuLoadInfoCount);

        m_load = qreal(1) - (qreal(idle - m_lastIdle) / qreal(total - m_lastTotal));

        m_lastIdle = idle;
        m_lastTotal = total;
    } else {
        m_load = qreal(1);
    }
    return m_load;
}


int MemoryReader::s_pageSize = 0;

MemoryReader::MemoryReader()
{
    if (!s_totalValue) {
        int mib[2] = { CTL_HW, HW_MEMSIZE };
        int64_t hwMem;
        size_t hwMemSize = sizeof(hwMem);

        if (sysctl(mib, sizeof(mib) / sizeof(*mib), &hwMem, &hwMemSize, nullptr, 0) == KERN_SUCCESS)
            s_totalValue = quint64(hwMem);

        mib[1] = HW_PAGESIZE;
        int hwPageSize;
        size_t hwPageSizeSize = sizeof(hwPageSize);

        if (sysctl(mib, sizeof(mib) / sizeof(*mib), &hwPageSize, &hwPageSizeSize, nullptr, 0) == KERN_SUCCESS)
            s_pageSize = hwPageSize;
    }
}

quint64 MemoryReader::readUsedValue() const
{
    vm_statistics64_data_t vmStat;
    mach_msg_type_number_t vmStatCount = HOST_VM_INFO64_COUNT;

    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vmStat), &vmStatCount) == KERN_SUCCESS) {
        quint64 app = vmStat.internal_page_count;
        quint64 compressed = vmStat.compressor_page_count;
        quint64 wired = vmStat.wire_count;

        return (app + compressed + wired) * quint64(s_pageSize);
    } else {
        return 0;
    }
}


QT_END_NAMESPACE_AM

#else // Q_OS_...

QT_BEGIN_NAMESPACE_AM

CpuReader::CpuReader()
{ }

qreal CpuReader::readLoadValue()
{
    return qreal(1);
}

MemoryReader::MemoryReader()
{ }

quint64 MemoryReader::readUsedValue() const
{
    return 0;
}

QT_END_NAMESPACE_AM

#endif  // defined(Q_OS_...)

#if !defined(Q_OS_LINUX)

QT_BEGIN_NAMESPACE_AM

GpuReader::GpuReader()
{ }

void GpuReader::setActive(bool /*enable*/)
{ }

bool GpuReader::isActive() const
{
    return false;
}

qreal GpuReader::readLoadValue()
{
    return 0;
}

IoReader::IoReader(const char *device)
{
    Q_UNUSED(device)
}

IoReader::~IoReader()
{ }

qreal IoReader::readLoadValue()
{
    return qreal(1);
}

MemoryThreshold::MemoryThreshold(const QList<qreal> &thresholds)
{
    Q_UNUSED(thresholds)
    Q_UNUSED(m_initialized)
    Q_UNUSED(m_enabled)
}

MemoryThreshold::~MemoryThreshold()
{ }

QList<qreal> MemoryThreshold::thresholdPercentages() const
{
    return QList<qreal>();
}

bool MemoryThreshold::isEnabled() const
{
    return false;
}

bool MemoryThreshold::setEnabled(bool enabled)
{
    Q_UNUSED(enabled)
    return false;
}

MemoryWatcher::MemoryWatcher(QObject *parent)
    : QObject(parent)
{
    Q_UNUSED(m_memLimit)
    Q_UNUSED(hasMemoryLowWarning)
    Q_UNUSED(hasMemoryCriticalWarning)
}

void MemoryWatcher::setThresholds(qreal warning, qreal critical)
{
    m_warning = warning;
    m_critical = critical;
}

bool MemoryWatcher::startWatching(const QString &groupPath)
{
    Q_UNUSED(groupPath)
    return false;
}

void MemoryWatcher::checkMemoryConsumption()
{ }

QT_END_NAMESPACE_AM

#endif // !defined(Q_OS_LINUX)

#include "moc_systemreader.cpp"
