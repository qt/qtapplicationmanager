/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "startuptimer.h"

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <io.h>
#  define isatty _isatty
#elif defined(Q_OS_LINUX)
#  include <time.h>
#  include <qplatformdefs.h>
#  include <sys/syscall.h>
#elif defined(Q_OS_OSX)
#  include <unistd.h>
#  include <sys/sysctl.h>
#endif


StartupTimer::StartupTimer()
{
    QByteArray useTimer = qgetenv("AM_STARTUP_TIMER");
    if (useTimer.isNull())
        return;
    else if (useTimer.isEmpty() || useTimer == "1")
        m_output = stderr;
    else
        m_output = fopen(useTimer, "w");

#if defined(Q_OS_WIN)
    // Windows reports FILETIMEs in 100nsec steps: divide by 10 to get usec
    auto winFileTimeToUSec = [](const FILETIME &filetime) {
        return ((quint64(filetime.dwHighDateTime) << 32) | quint64(filetime.dwLowDateTime)) / 10;
    };

    FILETIME creationTime, dummy, now;
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &dummy, &dummy, &dummy)) {
        GetSystemTimeAsFileTime(&now);
        m_processCreation = (winFileTimeToUSec(now) - winFileTimeToUSec(creationTime));
        m_initialized = true;
    } else {
        qWarning("StartupTimer: could not get process creation time");
    }
#elif defined(Q_OS_LINUX)
    // Linux is stupid: there's only one way to get your own process' start time with a high
    // resolution: using the async netlink protocol to get a 'taskstat', but this is highly complex
    // and requires root privileges.
    // And then there's /proc/self/task/<gettid>/stat which gives you the "jiffies" at creation
    // time, but no reference to compare it to (plus its resolution is very coarse).
    // The following implements the idea from the Mozilla team to get a stable reference for this
    // jiffie value: https://blog.mozilla.org/tglek/2011/01/14/builtin-startup-measurement/
    // This will give you roughly a 10ms resolution for the time from process creation to entering
    // main(), depending on your kernel's HZ value.

    // really bool (*)(quint32 *result), but casting the lambda does not work
    auto readJiffiesFromProc = [](void *resultPtr) -> void * {
        void *result = 0;

        QByteArray file = "/proc/self/task/" + QByteArray::number((int) syscall(SYS_gettid)) + "/stat";
        int fd = QT_OPEN(file, O_RDONLY);
        if (fd >= 0) {
            char buffer[1024];
            ssize_t bytesRead = QT_READ(fd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = 0;
                for (int field = 0, pos = 0; pos < bytesRead; ) {
                    if (buffer[pos++] == ' ') {
                        if (++field == 21) {
                            *reinterpret_cast<quint32 *>(resultPtr) = strtoul(buffer + pos, 0, 10);
                            result = reinterpret_cast<void *>(1);
                            break;
                        }
                    }
                }
            }
            QT_CLOSE(fd);
        }
        return result;
    };

    quint32 threadJiffies;
    pthread_t pt;
    void *threadJiffiesOk = 0;

    // using clone() with CLONE_VFORK would be more efficient, but it messes up the NPTL internal state
    if ((pthread_create(&pt, 0, readJiffiesFromProc, &threadJiffies) == 0)
            && (pthread_join(pt, &threadJiffiesOk) == 0)
            && threadJiffiesOk) {

        quint32 processJiffies = 0;
        if (readJiffiesFromProc(&processJiffies)) {
            int clkTck = sysconf(_SC_CLK_TCK);
            if (clkTck > 0) {
                m_processCreation = (threadJiffies - processJiffies) * 1000*1000 / clkTck;
                m_initialized = true;
            } else {
                qWarning("StartupTimer: could not get _SC_CLK_TCK");
            }
        } else {
            qWarning("StartupTimer: could not read process creation jiffies");
        }
    } else {
        qWarning("StartupTimer: could not read thread creation jiffies");
    }

#elif defined(Q_OS_OSX)
    int mibNames[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    size_t procInfoSize;

    if (sysctl(mibNames, sizeof(mibNames) / sizeof(mibNames[0]), 0, &procInfoSize, 0, 0) == 0) {
        kinfo_proc *procInfo = (kinfo_proc *) malloc(procInfoSize);

        if (sysctl(mibNames, sizeof(mibNames) / sizeof(mibNames[0]), procInfo, &procInfoSize, 0, 0) == 0) {
            struct timeval now;

            if (gettimeofday(&now, 0) == 0) {
                m_processCreation = (quint64(now.tv_sec) * 1000000 + now.tv_usec)
                        - (procInfo->kp_proc.p_un.__p_starttime.tv_sec * 1000000 + procInfo->kp_proc.p_un.__p_starttime.tv_usec);
                m_initialized = true;
            }
        } else {
            qWarning("StartupTimer: could not get kinfo_proc from kernel");
        }
        free(procInfo);
    } else {
        qWarning("StartupTimer: could not get size of kinfo_proc buffer");
    }

#else
    qWarning("StartupTimer: not implemented on this platform");
    m_initialized = false;
#endif

    if (m_initialized) {
        m_timer.start();
        checkpoint("entered main");
    }
}

StartupTimer::~StartupTimer()
{
    if (m_output && m_output != stderr)
        fclose(m_output);
}

void StartupTimer::checkpoint(const char *name)
{
    if (m_initialized) {
        qint64 delta = m_timer.nsecsElapsed();
        m_checkpoints << qMakePair(delta / 1000 + m_processCreation, name);
    }
}

void StartupTimer::createReport() const
{
    if (m_output) {
        bool colorSupport = isatty(fileno(m_output));

        if (colorSupport) {
            fprintf(m_output, "\n\033[33m== STARTUP TIMING REPORT ==\033[0m\n");
        } else {
            fprintf(m_output, "\n== STARTUP TIMING REPORT ==\n");
        }

        static const int cols = 120;
        static const int barCols = 60;

        int delta = m_checkpoints.last().first;
        qreal usecPerCell = delta / barCols;
        int secondsLength = QByteArray::number(delta / 1000000).length();

        for (int i = 0; i < m_checkpoints.size(); ++i) {
            quint64 usec = m_checkpoints.at(i).first;
            const QByteArray text = m_checkpoints.at(i).second;
            int sec = 0;
            int cells = usec / usecPerCell;
            QByteArray bar(cells, colorSupport ? ' ' : '#');
            QByteArray spacing(cols - cells - 2 - secondsLength - 8 - text.length(), ' ');

            if (usec > 1000*1000) {
                sec = usec / (1000*1000);
                usec %= (1000*1000);
            }
            int msec = usec / 1000;
            usec %= 1000;

            if (colorSupport) {
                fprintf(m_output, "\033[32m%d'%03d.%03d\033[0m %s %s\033[44m %s\033[0m\n", sec, msec, int(usec), text.constData(), spacing.constData(), bar.constData());
            } else {
                fprintf(m_output, "%d'%03d.%03d %s %s#%s\n", sec, msec, int(usec), text.constData(), spacing.constData(), bar.constData());
            }
        }
        fprintf(m_output, "\n");
    }
}

