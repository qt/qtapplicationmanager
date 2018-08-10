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

#include <QCoreApplication>
#include <QtAppManWindow/windowmanager.h>
#include "logging.h"
#include "applicationmanager.h"
#include "abstractruntime.h"
#include "processmonitor_p.h"

#if defined(Q_OS_MACOS)
#  include <mach/mach.h>
#elif defined(Q_OS_LINUX)
#  include <unistd.h>
#endif


QT_BEGIN_NAMESPACE_AM


ReadingTask::ReadingTask(QMutex &mutex, ReadingTask::Results &res)
    : m_mutex(mutex)
    , m_results(res)
{}

void ReadingTask::cancelTimer()
{
    if (m_reportingTimerId) {
        killTimer(m_reportingTimerId);
        m_reportingTimerId = 0;
    }
}

void ReadingTask::setupTimer(bool enabled, int interval)
{
    if (interval != -1)
        m_reportingInterval = interval;

    if (!enabled) {
        cancelTimer();
    } else {
        if (interval != -1)
            cancelTimer();

        if (!m_reportingTimerId && m_reportingInterval >= 0)
            m_reportingTimerId = startTimer(m_reportingInterval);
    }
}

void ReadingTask::setNewPid(qint64 pid)
{
    m_pid = pid;
    if (pid) {
        openLoad();
        readLoad();
    }
}

void ReadingTask::setNewReadCpu(bool enabled)
{
    m_readCpu = enabled;
    if (enabled)
        readLoad();
}

void ReadingTask::setNewReadMem(bool enabled)
{
    m_readMem = enabled;
}

void ReadingTask::reset(int sync)
{
    m_sync = sync;
}

void ReadingTask::timerEvent(QTimerEvent *event)
{
    if (event && event->timerId() == m_reportingTimerId && m_pid) {
        ReadingTask::Results results;

        if (m_readMem) {
            const QByteArray file = "/proc/" + QByteArray::number(m_pid) + "/smaps";
            if (!readMemory(file, results.memory))
                results.memory = ReadingTask::Results::Memory();
            results.memory.read =  true;
        }

        if (m_readCpu) {
            results.cpu.load = readLoad();
            results.cpu.read = true;
        }

        results.sync = m_sync;

        m_mutex.lock();
        m_results = results;
        m_mutex.unlock();

        emit newReadingAvailable();
    }
}


#if defined(Q_OS_LINUX)

void ReadingTask::openLoad()
{
    const QByteArray fileName = "/proc/" + QByteArray::number(m_pid) + "/stat";
    m_statReader.reset(new SysFsReader(fileName));
    if (!m_statReader->isOpen())
        qCWarning(LogSystem) << "Cannot read CPU load from" << fileName;
}

qreal ReadingTask::readLoad()
{
    qint64 elapsed;
    if (m_elapsedTime.isValid()) {
        elapsed = m_elapsedTime.restart();
    } else {
        elapsed = 0;
        m_elapsedTime.start();
    }

    if (m_statReader.isNull() || !m_statReader->isOpen()) {
        m_lastCpuUsage = 0.0;
        return 0.0;
    }

    QByteArray str = m_statReader->readValue();
    int pos = 0;
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

    qreal load = elapsed != 0 ? (utime + stime - m_lastCpuUsage) * 1000.0 / sysconf(_SC_CLK_TCK) / elapsed : 0.0;
    m_lastCpuUsage = utime + stime;
    return load;
}

static uint parseValue(const char *pl)
{
    while (*pl && (*pl < '0' || *pl > '9'))
        pl++;
    return static_cast<uint>(strtoul(pl, nullptr, 10));
}

bool ReadingTask::readMemory(const QByteArray &smapsFile, ReadingTask::Results::Memory &results)
{
    struct ScopedFile {
        ~ScopedFile() { if (file) fclose(file); }
        FILE *file = nullptr;
    };

    ScopedFile sf;
    sf.file = fopen(smapsFile.constData(), "r");

    if (sf.file == nullptr)
        return false;

    const int lineLen = 100;  // we are not interested in full library paths
    char line[lineLen + 5];   // padding for highly unlikely trailing perm flags below
    char *pl;                 // pointer to chars within line
    bool ok = true;

    if (fgets(line, lineLen, sf.file) == nullptr)
        return false;

    // sanity checks
    pl = line;
    for (pl = line; pl < (line + 4) && ok; ++pl)
        ok = ((*pl >= '0' && *pl <= '9') || (*pl >= 'a' && *pl <= 'f'));
    while (strlen(line) == lineLen - 1 && line[lineLen - 2] != '\n') {
        if (Q_UNLIKELY(!fgets(line, lineLen, sf.file)))
            break;
    }
    if (fgets(line, lineLen, sf.file) == nullptr)
        return false;
    static const char strSize[] = "Size: ";
    ok = ok && !qstrncmp(line, strSize, sizeof(strSize) - 1);
    if (!ok)
        return false;

    // Determine block size
    ok = false;
    int blockLen = 0;
    while (fgets(line, lineLen, sf.file) != nullptr && !ok) {
        if (!(line[0] < '0' || line[0] > '9') && (line[0] < 'a' || line[0] > 'f'))
            ok = true;
        ++blockLen;
    }
    if (!ok || blockLen < 12 || blockLen > 32)
        return false;

    fseek(sf.file, 0, SEEK_SET);
    bool wasPrivateOnly = false;
    ok = false;

    while (true) {
        if (Q_UNLIKELY(!(fgets(line, lineLen, sf.file) != nullptr))) {
            ok = feof(sf.file);
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
            if (Q_UNLIKELY(!fgets(line, lineLen, sf.file)))
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
            if (Q_UNLIKELY(!fgets(line, lineLen, sf.file)))
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

        results.totalVm += vm;
        results.totalRss += rss;
        results.totalPss += pss;

        static const char permRXP[] = { 'r', '-', 'x', 'p' };
        static const char permRWP[] = { 'r', 'w', '-', 'p' };
        if (!memcmp(permissions, permRXP, sizeof(permissions))) {
            results.textVm += vm;
            results.textRss += rss;
            results.textPss += pss;
        } else if (!memcmp(permissions, permRWP, sizeof(permissions))
                   && !isMainStack && (vm != 8192 || hasInode || !wasPrivateOnly) // try to exclude stack
                   && !hasInode) {
            results.heapVm += vm;
            results.heapRss += rss;
            results.heapPss += pss;
        }

        static const char permP[] = { '-', '-', '-', 'p' };
        wasPrivateOnly = !memcmp(permissions, permP, sizeof(permissions));

        for (int skip = skipLen; skip; --skip) {
            if (Q_UNLIKELY(!fgets(line, lineLen, sf.file)))
                break;
        }
    }

    return ok;
}

#elif defined(Q_OS_MACOS)

void ReadingTask::openLoad()
{
}

qreal ReadingTask::readLoad()
{
    return 0.0;
}

bool ReadingTask::readMemory(const QByteArray &smapsFile, ReadingTask::Results::Memory &results)
{
    Q_UNUSED(smapsFile)
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count)) {
        qCWarning(LogSystem) << "Could not read memory data";
        return false;
    }

    results.totalRss = t_info.resident_size;
    results.totalVm = t_info.virtual_size;

    return true;
}

#else

void ReadingTask::openLoad()
{
}

qreal ReadingTask::readLoad()
{
    return 0.0;
}

bool ReadingTask::readMemory(const QByteArray &smapsFile, ReadingTask::Results::Memory &results)
{
    Q_UNUSED(smapsFile)
    Q_UNUSED(results)
    return false;
}

#endif


ProcessMonitorPrivate::ProcessMonitorPrivate(ProcessMonitor *q)
        : q_ptr(q)
{
    connect(ApplicationManager::instance(), &ApplicationManager::applicationRunStateChanged,
            this, &ProcessMonitorPrivate::appRuntimeChanged);

    readingTask = new ReadingTask(mutex, readResults);
    readingTask->moveToThread(&thread);
    connect(this, &ProcessMonitorPrivate::newPid, readingTask, &ReadingTask::setNewPid);
    connect(this, &ProcessMonitorPrivate::setupTimer, readingTask, &ReadingTask::setupTimer);
    connect(this, &ProcessMonitorPrivate::newReadCpu, readingTask, &ReadingTask::setNewReadCpu);
    connect(this, &ProcessMonitorPrivate::newReadMem, readingTask, &ReadingTask::setNewReadMem);
    connect(this, &ProcessMonitorPrivate::reset, readingTask, &ReadingTask::reset);
    connect(readingTask, &ReadingTask::newReadingAvailable, this, &ProcessMonitorPrivate::readingUpdate);
    connect(&thread, &QThread::finished, readingTask, &ReadingTask::deleteLater);
    thread.start();
}

ProcessMonitorPrivate::~ProcessMonitorPrivate()
{
    thread.quit();
    thread.wait();
}

void ProcessMonitorPrivate::appRuntimeChanged(const QString &id, Am::RunState state)
{
    if (id == appId && (state == Am::Running || state == Am::NotRunning))
        determinePid();
}

void ProcessMonitorPrivate::setupInterval(int interval)
{
    bool shouldBeOn = pid && (reportCpu || reportMemory || reportFps || cpuTail || memTail || fpsTail);
    emit setupTimer(shouldBeOn, interval);
}

void ProcessMonitorPrivate::determinePid()
{
    Q_Q(ProcessMonitor);

    qint64 newId;
    if (appId.isEmpty()) {
        newId = QCoreApplication::applicationPid();
    } else {
        if (ApplicationManager::instance()->isSingleProcess()) {
            newId = 0;
        } else {
            AbstractApplication *app = ApplicationManager::instance()->fromId(appId);
            newId = (app && app->currentRuntime()) ? app->currentRuntime()->applicationProcessId() : 0;
        }
    }

    if (newId != pid) {
        pid = newId;
        setupInterval();
        emit newPid(pid);
        emit q->processIdChanged(pid);
    }
}

const ProcessMonitorPrivate::ModelData &ProcessMonitorPrivate::modelDataForRow(int row) const
{
    int pos = reportPos - row - 1;
    if (pos < 0)
        pos += modelData.size();
    if (pos < 0 || pos >= modelData.size())
        return modelData.first();
    return modelData.at(pos);
}


void ProcessMonitorPrivate::resetModel()
{
    Q_Q(ProcessMonitor);

    q->beginResetModel();
    modelData.clear();
    modelData.resize(count);
    reportPos = 0;
    q->endResetModel();

    cpuTail = memTail = fpsTail = 0;

    emit reset(++sync);
}

void ProcessMonitorPrivate::updateModelCount(int newCount)
{
    Q_Q(ProcessMonitor);

    int diff = newCount - count;
    q->beginResetModel();
    if (diff > 0) {
        modelData.insert(reportPos, diff, ModelData());
    } else {
        if (reportPos <= newCount) {
            modelData.remove(reportPos, -diff);
            if (reportPos == newCount)
                reportPos = 0;
        } else {
            modelData.remove(reportPos, count - reportPos);
            modelData.remove(0, reportPos - newCount);
            reportPos = 0;
        }
    }
    count = newCount;
    q->endResetModel();

    if (cpuTail > 0)
        cpuTail += diff;
    if (memTail > 0)
        memTail += diff;
    if (fpsTail > 0)
        fpsTail += diff;
}

void ProcessMonitorPrivate::readingUpdate()
{
    Q_Q(ProcessMonitor);

    QVector<int> roles;
    ModelData data;
    ReadingTask::Results results;

    mutex.lock();
    results = readResults;
    mutex.unlock();

    if (sync != results.sync)
        return;

    if (cpuTail > 0) {
        if (--cpuTail == 0)
            setupInterval();
        roles.append(CpuLoad);
    } else if (results.cpu.read) {
        data.cpuLoad = results.cpu.load;
        roles.append(CpuLoad);
    }

    if (results.memory.read || memTail > 0) {
        if (memTail > 0) {
            if (--memTail == 0)
                setupInterval();
            results.memory = ReadingTask::Results::Memory();
        }

        // Although smaps claims to report kB it's actually KiB (2^10 = 1024 Bytes)
        data.vm.insert(qSL("total"), quint64(results.memory.totalVm) << 10);
        data.vm.insert(qSL("text"), quint64(results.memory.textRss) << 10);
        data.vm.insert(qSL("heap"), quint64(results.memory.heapPss) << 10);
        data.rss.insert(qSL("total"), quint64(results.memory.totalRss) << 10);
        data.rss.insert(qSL("text"), quint64(results.memory.textRss) << 10);
        data.rss.insert(qSL("heap"), quint64(results.memory.heapRss) << 10);
        data.pss.insert(qSL("total"), quint64(results.memory.totalPss) << 10);
        data.pss.insert(qSL("text"), quint64(results.memory.textPss) << 10);
        data.pss.insert(qSL("heap"), quint64(results.memory.heapPss) << 10);

        roles.append({ MemVirtual, MemRss, MemPss });
    }

    if (reportFps || fpsTail > 0) {
        if (fpsTail > 0 && --fpsTail == 0)
            setupInterval();

        for (auto it = frameCounters.begin(); it != frameCounters.end(); ++it) {
            QVariantMap frameMap;
            FrameTimer *frameTimer = frameCounters.value(it.key());
            frameMap.insert(qSL("average"), frameTimer->averageFps());
            frameMap.insert(qSL("maximum"), frameTimer->maximumFps());
            frameMap.insert(qSL("minimum"), frameTimer->minimumFps());
            frameMap.insert(qSL("jitter"), frameTimer->jitterFps());
            frameTimer->reset();
            data.frameRate.append(frameMap);
        }

        emit q->frameRateReportingChanged(data.frameRate);

        roles.append(FrameRate);
    }

    int last = modelData.size() - 1;
    q->beginMoveRows(QModelIndex(), last, last, QModelIndex(), 0);
    modelData[reportPos++] = data;
    if (reportPos > last)
        reportPos = 0;
    q->endMoveRows();
    q->dataChanged(q->index(0), q->index(0), roles);

    if (reportCpu)
        emit q->cpuLoadReportingChanged(data.cpuLoad);

    if (results.memory.read || memTail > 0)
        emit q->memoryReportingChanged(data.vm, data.rss, data.pss);
}

void ProcessMonitorPrivate::setupFrameRateMonitoring()
{
    resetFrameRateMonitoring();
#if !defined(AM_HEADLESS)
    if (reportFps) {
        if (ApplicationManager::instance()->isSingleProcess() || appId.isEmpty()) {

            for (auto it = frameCounters.begin(); it != frameCounters.end(); ++it) {
                QQuickWindow *win = qobject_cast<QQuickWindow *>(it.key());
                if (win) {
                    connections << connect(win, &QQuickWindow::frameSwapped,
                                           this, &ProcessMonitorPrivate::frameUpdated);
                }
            }
        } else {
#  if defined(AM_MULTI_PROCESS)

            for (auto it = frameCounters.begin(); it != frameCounters.end(); ++it) {
                WaylandWindow *win = qobject_cast<WaylandWindow *>(it.key());
                if (win) {
                    // Check if this is valid window for this application
                    if (win->application() && win->application()->id() == appId) {
                        connections << connect(win, &WaylandWindow::frameUpdated,
                                           this, &ProcessMonitorPrivate::frameUpdated);
                    } else {
                        qCWarning(LogSystem) << "Windows do not belong to this app";
                        return;
                    }
                }
            }

            connect(WindowManager::instance(), &WindowManager::windowContentStateChanged,
                                   this, &ProcessMonitorPrivate::onWindowContentStateChanged);
#  endif
        }
    } else {
#  if defined(AM_MULTI_PROCESS)
        disconnect(WindowManager::instance(), &WindowManager::windowContentStateChanged,
                               this, &ProcessMonitorPrivate::onWindowContentStateChanged);
#  endif
    }
#endif // !AM_HEADLESS
}

void ProcessMonitorPrivate::updateMonitoredWindows(const QList<QObject *> &windowObjs)
{
#if !defined(AM_HEADLESS)
    Q_Q(ProcessMonitor);

    clearMonitoredWindows();

    for (auto windowObj : windowObjs) {
        if (ApplicationManager::instance()->isSingleProcess() || appId.isEmpty()) {
            QQuickWindow *view = qobject_cast<QQuickWindow *>(windowObj);
            if (view) {
                FrameTimer *frameTimer = new FrameTimer();
                frameCounters.insert(view, frameTimer);
            } else
                qCWarning(LogSystem) << "In single process only QQuickWindow can be monitored";
        } else {
#  if defined(AM_MULTI_PROCESS)
            auto window = qobject_cast<Window*>(windowObj);
            if (window && window->application()->nonAliased()->id() == appId) {
                FrameTimer *frameTimer = new FrameTimer();
                frameCounters.insert(window, frameTimer);
            } else
                continue; // TODO: Complain in a warning message
#  endif
        }
    }

    emit q->monitoredWindowsChanged();
    setupFrameRateMonitoring();
#else
    Q_UNUSED(windows)
#endif
}

QList<QObject *> ProcessMonitorPrivate::monitoredWindows() const
{
    QList<QObject *> list;

#if !defined(AM_HEADLESS)
    if (ApplicationManager::instance()->isSingleProcess() || appId.isEmpty()) {
        for (auto it = frameCounters.begin(); it != frameCounters.end(); ++it) {
            list.append(it.key());
        }

        return list;
    }

#  if defined(AM_MULTI_PROCESS)
    // Return window items
    for (auto it = frameCounters.begin(); it != frameCounters.end(); ++it) {
        WaylandWindow *win = qobject_cast<WaylandWindow *>(it.key());
        if (win)
            list.append(win);
    }
#  endif
#endif
    return list;
}

#if defined(AM_MULTI_PROCESS) && !defined(AM_HEADLESS)

void ProcessMonitorPrivate::onWindowContentStateChanged(Window *window)
{
    if (window->contentState() == Window::SurfaceWithContent)
        return;

    for (auto it = frameCounters.cbegin(); it != frameCounters.cend(); ++it) {
        auto win = qobject_cast<Window *>(it.key());
        if (win && win == window) {
            frameCounters.remove(win);
            break;
        }
    }
}

#endif

void ProcessMonitorPrivate::frameUpdated()
{
    FrameTimer *frameTimer = frameCounters.value(sender());
    if (!frameTimer) {
        frameTimer = new FrameTimer();
        frameCounters.insert(sender(), frameTimer);
    }

    frameTimer->newFrame();
}

void ProcessMonitorPrivate::resetFrameRateMonitoring()
{
    for (auto connection : connections) {
        QObject::disconnect(connection);
    }

    connections.clear();
}

void ProcessMonitorPrivate::clearMonitoredWindows()
{
    qDeleteAll(frameCounters);
    frameCounters.clear();
}

QT_END_NAMESPACE_AM
