// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QMutexLocker>
#include <QThread>
#include <QtQml/qqmlinfo.h>

#if defined(Q_OS_MACOS)
#  include <mach/mach.h>
#elif defined(Q_OS_LINUX)
#  include <unistd.h>
#endif

#include "abstractruntime.h"
#include "applicationmanager.h"
#include "logging.h"
#include "qml-utilities.h"
#include "processstatus.h"
#include "processstatus_p.h"

using namespace Qt::StringLiterals;

/*!
    \qmltype ProcessStatus
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-instantiable
    \brief Provides information about the status of an application process.

    ProcessStatus provides information about the process of a given application.

    You can use it alongside a Timer, for instance, to periodically query the status of an
    application process.

    \qml
    import QtQuick
    import QtApplicationManager
    import QtApplicationManager.SystemUI

    Item {
        id: root
        property var application: ApplicationManager.get(0)
        ...
        ProcessStatus {
            id: processStatus
            applicationId: root.application.id
        }
        Timer {
            interval: 1000
            running: root.visible && root.application.runState === Am.Running
            repeat: true
            onTriggered: processStatus.update()
        }
        Text {
            text: "PSS.total: " + (processStatus.memoryPss.total / 1e6).toFixed(0) + " MB"
        }
    }
    \endqml

    You can also use this type as a data source for MonitorModel, if you want to plot its previous
    values over time:

    \qml
    import QtQuick
    import QtApplicationManager
    import QtApplicationManager.SystemUI
    ...
    MonitorModel {
        running: true
        ProcessStatus {
            applicationId: "some.app.id"
        }
    }
    \endqml

    \target supported-keys
    The following are the keys supported in the memory properties (\c memoryVirtual, \c memoryRss,
    and \c memoryPss):

    \table
    \header
        \li Key
        \li Description
    \row
        \li \c total
        \li The total amount of memory used, in bytes.
    \row
        \li \c text
        \li The amount of memory used by the code section, in bytes.
    \row
        \li \c heap
        \li The amount of memory used by the heap, in bytes. The heap is private, dynamically
            allocated memory, for example through \c malloc or \c mmap on Linux.
    \endtable
*/

/*!
    \qmlsignal ProcessStatus::memoryReportingChanged(var memoryVirtual, var memoryRss, var memoryPss)

    This signal is emitted after \l update() has been called and the memory usage values have been
    refreshed. Each of the arguments \a memoryVirtual, \a memoryRss and \a memoryPss is a
    JavaScript object with the available properties listed in the table
    \l{supported-keys}{above}.
*/


QT_BEGIN_NAMESPACE_AM

QThread *ProcessStatusPrivate::s_workerThread = nullptr;
int ProcessStatusPrivate::s_instanceCount = 0;

ProcessStatus::ProcessStatus(QObject *parent)
    : QObject(*new ProcessStatusPrivate, parent)
{
    Q_D(ProcessStatus);

    if (ProcessStatusPrivate::s_instanceCount == 0) {
        ProcessStatusPrivate::s_workerThread = new QThread;
        ProcessStatusPrivate::s_workerThread->setObjectName(u"QtAM-ProcessStatus"_s);
        ProcessStatusPrivate::s_workerThread->start();
    }
    ++ProcessStatusPrivate::s_instanceCount;

    d->m_reader = new ProcessReader;
    d->m_reader->moveToThread(ProcessStatusPrivate::s_workerThread);

    connect(d->m_reader, &ProcessReader::updated, this, [this]() {
        Q_D(ProcessStatus);
        d->fetchReadings();
        emit cpuLoadChanged();
        emit memoryReportingChanged(d->m_memoryVirtual, d->m_memoryRss, d->m_memoryPss);
        d->m_pendingUpdate = false;
    });
    connect(this, &ProcessStatus::processIdChanged, d->m_reader, &ProcessReader::setProcessId);
    connect(this, &ProcessStatus::memoryReportingEnabledChanged, d->m_reader, &ProcessReader::enableMemoryReporting);
}

ProcessStatus::~ProcessStatus()
{
    Q_D(ProcessStatus);

    d->m_reader->deleteLater();

    --ProcessStatusPrivate::s_instanceCount;
    if (ProcessStatusPrivate::s_instanceCount == 0) {
        ProcessStatusPrivate::s_workerThread->quit();
        ProcessStatusPrivate::s_workerThread->wait();
        delete ProcessStatusPrivate::s_workerThread;
        ProcessStatusPrivate::s_workerThread = nullptr;
    }
}

/*!
    \qmlmethod void ProcessStatus::update()

    Updates the cpuLoad, memoryVirtual, memoryRss, and memoryPss properties.
*/
void ProcessStatus::update()
{
    Q_D(ProcessStatus);

    if (!d->m_pendingUpdate) {
        d->m_pendingUpdate = true;
        QMetaObject::invokeMethod(d->m_reader, &ProcessReader::update);
    }
}

/*!
    \qmlproperty string ProcessStatus::applicationId

    Holds the \l{ApplicationObject::id}{ID} of the \l{ApplicationObject}{application} whose process
    is to be monitored. This ID must be one that is known to the application manager
    (\l{ApplicationManager::applicationIds}{applicationIds()} provides a list of valid IDs). There
    is one exception: if you want to monitor the System UI's process, set the ID to an empty
    string. In single-process mode, the System UI process is the only valid process, since all
    applications run within this process.

    \sa ApplicationObject
*/
QString ProcessStatus::applicationId() const
{
    Q_D(const ProcessStatus);
    return d->m_appId;
}

void ProcessStatus::setApplicationId(const QString &appId)
{
    Q_D(ProcessStatus);

    if (d->m_appId != appId || d->m_appId.isNull()) {
        if (d->m_application) {
            disconnect(d->m_application, nullptr, this, nullptr);
            d->m_application = nullptr;
        }
        d->m_appId = appId;
        if (!appId.isEmpty()) {
            int appIndex = ApplicationManager::instance()->indexOfApplication(appId);
            if (appIndex < 0) {
                qmlWarning(this) << "Invalid application ID:" << appId;
            } else {
                d->m_application = ApplicationManager::instance()->application(appIndex);
                connect(d->m_application.data(), &Application::runStateChanged,
                        this, [d](Am::RunState state) {
                    if ((state == Am::Running) || (state == Am::NotRunning))
                        d->determinePid();
                });
            }
        }
        d->determinePid();
        emit applicationIdChanged(appId);
    }
}

/*!
    \qmlproperty int ProcessStatus::processId
    \readonly

    This property holds the OS-specific process identifier (PID) that is monitored. It can be
    used by external tools, for example. The property is 0, if there is no process associated with
    the \l applicationId. In particular, if the application manager runs in single-process mode,
    only the System UI has an associated process. The System UI is always identified by an empty
    \l applicationId.
*/
qint64 ProcessStatus::processId() const
{
    Q_D(const ProcessStatus);
    return d->m_pid;
}

/*!
    \qmlproperty real ProcessStatus::cpuLoad
    \readonly

    This property holds the process' CPU utilization during the previous measurement interval, when
    update() was last called. A value of \c 0 means the process was idle; a value of \c 1 means the
    process used the equivalent of one core, which may be split across several cores.

    \sa update
*/
qreal ProcessStatus::cpuLoad() const
{
    Q_D(const ProcessStatus);
    return d->m_cpuLoad;
}

/*!
    \qmlproperty var ProcessStatus::memoryVirtual
    \readonly

    A map of the process's virtual memory usage. For example, the total amount of virtual memory
    is provided through \c memoryVirtual.total. For more information, see the table of
    \l{supported-keys}{supported keys}.

    Calling \l update() updates the value of this property.

    \sa update()
*/
QVariantMap ProcessStatus::memoryVirtual() const
{
    Q_D(const ProcessStatus);
    return d->m_memoryVirtual;
}

/*!
    \qmlproperty var ProcessStatus::memoryRss
    \readonly

    A map of the process' Resident Set Size (RSS) memory usage. This is the amount of memory that
    is actually mapped to physical RAM. For more information, see the table of
    \l{supported-keys}{supported keys}.

    Calling \l update() updates the value of this property.

    \sa update()
*/
QVariantMap ProcessStatus::memoryRss() const
{
    Q_D(const ProcessStatus);
    return d->m_memoryRss;
}

/*!
    \qmlproperty var ProcessStatus::memoryPss
    \readonly

    A map of the process' Proportional Set Size (PSS) memory usage. This is the proportional share
    of the RSS value in memoryRss. For instance, if two processes share 2 MB, then the RSS value is
    2 MB for each process; the PSS value is 1 MB for each process. As the name implies, the code
    section of shared libraries is generally shared between processes. Memory may also be shared by
    other means provided by the OS, for example, through \c mmap on Linux. For more information,
    see the table of \l{supported-keys}{supported keys}.

    Calling update() updates the value of this property.

    \sa update()
*/
QVariantMap ProcessStatus::memoryPss() const
{
    Q_D(const ProcessStatus);
    return d->m_memoryPss;
}

/*!
    \qmlproperty bool ProcessStatus::memoryReportingEnabled

    A boolean value that determines whether the memory properties are refreshed each time
    \l update() is called. The default value is \c true. In your System UI, the process of
    determining memory consumption adds additional load to the CPU, affecting the \c cpuLoad value.
    If \c cpuLoad needs to be kept accurate, consider disabling memory reporting.
*/

bool ProcessStatus::isMemoryReportingEnabled() const
{
    Q_D(const ProcessStatus);
    return d->m_memoryReportingEnabled;
}

void ProcessStatus::setMemoryReportingEnabled(bool enabled)
{
    Q_D(ProcessStatus);

    if (enabled != d->m_memoryReportingEnabled) {
        d->m_memoryReportingEnabled = enabled;
        emit memoryReportingEnabledChanged(d->m_memoryReportingEnabled);
    }
}

/*!
    \qmlproperty list<string> ProcessStatus::roleNames
    \readonly

    Names of the roles that ProcessStatus provides when used as a data source for MonitorModel.

    \sa MonitorModel
*/
QStringList ProcessStatus::roleNames() const
{
    return { u"cpuLoad"_s, u"memoryVirtual"_s, u"memoryRss"_s, u"memoryPss"_s };
}

void ProcessStatus::classBegin()
{ }

void ProcessStatus::componentComplete()
{
    if (!ensureCurrentContextIsSystemUI(this))
        return;
}


///////////////////////////////////////////////////////////////////////
// ProcessStatusPrivate
///////////////////////////////////////////////////////////////////////


void ProcessStatusPrivate::fetchReadings()
{
    QMutexLocker locker(&m_reader->mutex);

    m_cpuLoad = m_reader->cpuLoad;

    // Although smaps claims to report kB it's actually KiB (2^10 = 1024 Bytes)
    m_memoryVirtual[u"total"_s] = static_cast<quint64>(m_reader->memory.totalVm) << 10;
    m_memoryVirtual[u"text"_s] = static_cast<quint64>(m_reader->memory.textVm) << 10;
    m_memoryVirtual[u"heap"_s] = static_cast<quint64>(m_reader->memory.heapVm) << 10;
    m_memoryRss[u"total"_s] = static_cast<quint64>(m_reader->memory.totalRss) << 10;
    m_memoryRss[u"text"_s] = static_cast<quint64>(m_reader->memory.textRss) << 10;
    m_memoryRss[u"heap"_s] = static_cast<quint64>(m_reader->memory.heapRss) << 10;
    m_memoryPss[u"total"_s] = static_cast<quint64>(m_reader->memory.totalPss) << 10;
    m_memoryPss[u"text"_s] = static_cast<quint64>(m_reader->memory.textPss) << 10;
    m_memoryPss[u"heap"_s] = static_cast<quint64>(m_reader->memory.heapPss) << 10;
}

void ProcessStatusPrivate::determinePid()
{
    Q_Q(ProcessStatus);

    qint64 newId = 0;
    if (m_appId.isEmpty()) {
        newId = QCoreApplication::applicationPid();
    } else if (m_application) {
        if (ApplicationManager::instance()->isSingleProcess())
            newId = 0;
        else
            newId = m_application->currentRuntime() ? m_application->currentRuntime()->applicationProcessId() : 0;
    }
    // an unknown application ID (m_application == nullptr) leaves the pid at 0

    if (newId != m_pid) {
        m_pid = newId;
        emit q->processIdChanged(m_pid);
    }
}


///////////////////////////////////////////////////////////////////////
// ProcessReader
///////////////////////////////////////////////////////////////////////


void ProcessReader::setProcessId(qint64 pid)
{
    m_pid = pid;
    if (pid)
        openCpuLoad();
}

void ProcessReader::enableMemoryReporting(bool enabled)
{
    m_memoryReportingEnabled = enabled;
    if (!m_memoryReportingEnabled)
        memory = Memory();
}

void ProcessReader::update()
{
    qreal load = readCpuLoad();

    if (m_memoryReportingEnabled) {
        Memory mem;
        bool memRead = readMemory(mem);
        QMutexLocker locker(&mutex);
        memory = memRead ? mem : Memory();
        cpuLoad = load;
    } else {
        QMutexLocker locker(&mutex);
        cpuLoad = load;
    }

    emit updated();
}

#if defined(Q_OS_LINUX)

void ProcessReader::openCpuLoad()
{
    m_statFile.setFileName(u"/proc/"_s + QString::number(m_pid) + u"/stat"_s);

    if (!m_statFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        qCritical("Couldn't open file for reading: %s (%s)",
                  qPrintable(m_statFile.fileName()), qPrintable(m_statFile.errorString()));
    }

    m_lastCpuUsage = 0;
    m_elapsedTime.invalidate();
}

qreal ProcessReader::readCpuLoad()
{
    qint64 elapsed;
    if (m_elapsedTime.isValid()) {
        elapsed = m_elapsedTime.restart();
    } else {
        elapsed = 0;
        m_elapsedTime.start();
    }

    if (!m_statFile.isOpen()) {
        m_lastCpuUsage = 0.0;
        return 0.0;
    }

    m_statFile.seek(0);
    const QByteArray str = m_statFile.readAll();
    qsizetype pos = 0;
    int blanks = 0;
    while (pos < str.size() && blanks < 13) {
        if (isblank(str.at(pos)))
            ++blanks;
        ++pos;
    }

    char *endPtr = nullptr;
    quint64 utime = strtoull(str.constData() + pos, &endPtr, 10); // check missing for overflow
    pos = int(endPtr - str.constData() + 1);
    quint64 stime = strtoull(str.constData() + pos, nullptr, 10); // check missing for overflow

    qreal load = elapsed ? (qreal(utime + stime - m_lastCpuUsage) * 1000 / qreal(sysconf(_SC_CLK_TCK)) / qreal(elapsed))
                         : 0.0;
    m_lastCpuUsage = utime + stime;
    return load;
}


bool ProcessReader::readMemory(Memory &mem)
{
    const QByteArray smapsFile = "/proc/" + QByteArray::number(m_pid) + "/smaps";
    return readSmaps(smapsFile, mem);
}

static uint parseValue(const char *pl) {
    while (*pl && (*pl < '0' || *pl > '9'))
        pl++;
    return static_cast<uint>(strtoul(pl, nullptr, 10));
}

bool ProcessReader::readSmaps(const QByteArray &smapsFile, Memory &mem)
{
    FILE *sf = nullptr;
    auto closeFile = qScopeGuard([&]() { if (sf) fclose(sf); });

    sf = fopen(smapsFile.constData(), "r");
    if (!sf)
        return false;

    static const int lineLen = 100;  // we are not interested in full library paths
    char line[lineLen + 5];   // padding for highly unlikely trailing perm flags below
    char *pl = nullptr;       // pointer to chars within line
    bool ok = true;

    if (!fgets(line, lineLen, sf))
        return false;

    // sanity checks
    for (pl = line; pl < (line + 4) && ok; ++pl)
        ok = ((*pl >= '0' && *pl <= '9') || (*pl >= 'a' && *pl <= 'f'));
    while (strlen(line) == lineLen - 1 && line[lineLen - 2] != '\n') {
        if (Q_UNLIKELY(!fgets(line, lineLen, sf)))
            break;
    }
    if (!fgets(line, lineLen, sf))
        return false;
    static const char strSize[] = "Size: ";
    ok = ok && !qstrncmp(line, strSize, sizeof(strSize) - 1);
    if (!ok)
        return false;

    // Determine block size
    ok = false;
    int blockLen = 0;
    while (fgets(line, lineLen, sf) && !ok) {
        if ((line[0] >= '0' && line[0] <= '9') || (line[0] >= 'a' && line[0] <= 'f'))
            ok = true;
        ++blockLen;
    }
    if (!ok || blockLen < 12 || blockLen > 32)
        return false;

    fseek(sf, 0, SEEK_SET);
    bool wasPrivateOnly = false;
    ok = false;

    while (true) {
        if (Q_UNLIKELY(!fgets(line, lineLen, sf))) {
            ok = feof(sf);
            break;
        }

        // Determine permission flags
        pl = line;
        while (*pl && *pl != ' ')
            ++pl;
        char permissions[4];
        memcpy(permissions, ++pl, sizeof(permissions));

        // Determine inode
        int spaceCount = 0;
        while (*pl && spaceCount < 3) {
            if (*pl == ' ')
                ++spaceCount;
            ++pl;
        }
        bool hasInode = (*pl != '0');

        // Determine library name
        while (*pl && *pl != ' ')
            ++pl;
        while (*pl && *pl == ' ')
            ++pl;

        static const char strStack[] = "stack]";
        bool isMainStack = (Q_UNLIKELY(*pl == '['
                            && !qstrncmp(pl + 1, strStack, sizeof(strStack) - 1)));
        // Skip rest of library path
        while (strlen(line) == lineLen - 1 && line[lineLen - 2] != '\n') {
            if (Q_UNLIKELY(!fgets(line, lineLen, sf)))
                break;
        }

        int skipLen = blockLen;
        uint vm = 0;
        uint rss = 0;
        uint pss = 0;
        const int sizeTag = 0x01;
        const int rssTag  = 0x02;
        const int pssTag  = 0x04;
        const int allTags = sizeTag | rssTag | pssTag;
        int foundTags = 0;

        while (foundTags < allTags && skipLen > 0) {
            skipLen--;
            if (Q_UNLIKELY(!fgets(line, lineLen, sf)))
                break;
            pl = line;

            static const char strSize[] = "ize:";
            static const char strXss[] = "ss:";

            switch (*pl) {
            case 'S':
                if (!qstrncmp(pl + 1, strSize, sizeof(strSize) - 1)) {
                    foundTags |= sizeTag;
                    vm = parseValue(pl + sizeof(strSize));
                }
                break;
            case 'R':
                if (!qstrncmp(pl + 1, strXss, sizeof(strXss) - 1)) {
                    foundTags |= rssTag;
                    rss = parseValue(pl + sizeof(strXss));
                }
                break;
            case 'P':
                if (!qstrncmp(pl + 1, strXss, sizeof(strXss) - 1)) {
                    foundTags |= pssTag;
                    pss = parseValue(pl + sizeof(strXss));
                }
                break;
            }
        }

        if (foundTags < allTags)
            break;

        mem.totalVm += vm;
        mem.totalRss += rss;
        mem.totalPss += pss;

        static const char permRXP[] = { 'r', '-', 'x', 'p' };
        static const char permRWP[] = { 'r', 'w', '-', 'p' };
        if (!memcmp(permissions, permRXP, sizeof(permissions))) {
            mem.textVm += vm;
            mem.textRss += rss;
            mem.textPss += pss;
        } else if (!memcmp(permissions, permRWP, sizeof(permissions))
                   && !isMainStack && (vm != 8192 || hasInode || !wasPrivateOnly) // try to exclude stack
                   && !hasInode) {
            mem.heapVm += vm;
            mem.heapRss += rss;
            mem.heapPss += pss;
        }

        static const char permP[] = { '-', '-', '-', 'p' };
        wasPrivateOnly = !memcmp(permissions, permP, sizeof(permissions));

        for (int skip = skipLen; skip; --skip) {
            if (Q_UNLIKELY(!fgets(line, lineLen, sf)))
                break;
        }
    }

    return ok;
}

bool ProcessReader::testReadSmaps(const QByteArray &smapsFile)
{
    memory = Memory();
    return readSmaps(smapsFile, memory);
}

#elif defined(Q_OS_MACOS)

void ProcessReader::openCpuLoad()
{
}

qreal ProcessReader::readCpuLoad()
{
    return 0.0;
}

bool ProcessReader::readMemory(Memory &mem)
{
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&t_info), &t_info_count) != 0) {
        qCWarning(LogSystem) << "Could not read memory data";
        return false;
    }

    mem.totalRss = t_info.resident_size;
    mem.totalVm = t_info.virtual_size;

    return true;
}

#else

void ProcessReader::openCpuLoad()
{
}

qreal ProcessReader::readCpuLoad()
{
    return 0.0;
}

bool ProcessReader::readMemory(Memory &mem)
{
    Q_UNUSED(mem)
    return false;
}

#endif

QT_END_NAMESPACE_AM

#include "moc_processstatus.cpp"
