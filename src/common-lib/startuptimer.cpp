/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#include "startuptimer.h"
#include "utilities.h"

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

/*!
    \qmltype StartupTimer
    \inqmlmodule QtApplicationManager
    \brief A tool for startup performance analysis.

    The StartupTimer is a class for measuring the startup performance of the System-UI, as well as
    applications started by application-manager.

    Using the checkpoint function, you can log the time it took from forking the process until now.
    The time is reported using a monotonic clock with nano-second resolution - see QElapsedTimer
    for more information.

    \note On Linux, the actual time between the forking of the process and the first checkpoint
          can only be obtained with 10ms resolution.

    In order to activate startup timing measurement, the \c $AM_STARTUP_TIMER environment variable
    needs to be set: if set to \c 1, a startup performance analysis will be printed on the console.
    Anything other than \c 1 will be interpreted as the name of a file that is used instead of the
    console.

    When activated, this report will always be printed for the System-UI. If the application-manager
    is running in multi-process mode, additional reports will also be printed for every QML
    application that is started.

    The application-manager and its QML launcher will already create a lot of checkpoints on their
    own and will also call createReport themselves after all the C++ side setup has finished. You
    can however add arbitrary checkpoints yourself using the QML API: access to the StartupTimer
    object is possible through a the \c StartupTimer root-context property in the QML engine.

    This is an example output, starting the \c Neptune UI on a console with ANSI color support:

    \raw HTML
    <style type="text/css" id="colorstyles">
        #color-green  { color: #18b218 }
        #color-orange { color: #b26818 }
        #color-blue   { background-color: #5454ff; color: #000000 }
    </style>
    <pre>
<span id="color-orange">== STARTUP TIMING REPORT: System UI ==</span>
<span id="color-green">0'110.001</span> entered main                                <span id="color-blue">   </span>
<span id="color-green">0'110.015</span> after basic initialization                  <span id="color-blue">   </span>
<span id="color-green">0'110.311</span> after sudo server fork                      <span id="color-blue">   </span>
<span id="color-green">0'148.911</span> after application constructor               <span id="color-blue">    </span>
<span id="color-green">0'150.086</span> after command line parse                    <span id="color-blue">    </span>
<span id="color-green">0'150.154</span> after logging setup                         <span id="color-blue">    </span>
<span id="color-green">0'150.167</span> after startup-plugin load                   <span id="color-blue">    </span>
<span id="color-green">0'151.714</span> after installer setup checks                <span id="color-blue">    </span>
<span id="color-green">0'151.847</span> after runtime registration                  <span id="color-blue">    </span>
<span id="color-green">0'156.278</span> after application database loading          <span id="color-blue">    </span>
<span id="color-green">0'158.450</span> after ApplicationManager instantiation      <span id="color-blue">     </span>
<span id="color-green">0'158.477</span> after NotificationManager instantiation     <span id="color-blue">     </span>
<span id="color-green">0'158.534</span> after SystemMonitor instantiation           <span id="color-blue">     </span>
<span id="color-green">0'158.572</span> after quick-launcher setup                  <span id="color-blue">     </span>
<span id="color-green">0'159.130</span> after ApplicationInstaller instantiation    <span id="color-blue">     </span>
<span id="color-green">0'159.192</span> after QML registrations                     <span id="color-blue">     </span>
<span id="color-green">0'164.888</span> after QML engine instantiation              <span id="color-blue">     </span>
<span id="color-green">0'189.619</span> after D-Bus registrations                   <span id="color-blue">     </span>
<span id="color-green">2'167.233</span> after loading main QML file                 <span id="color-blue">                                                        </span>
<span id="color-green">2'167.489</span> after WindowManager/QuickView instantiation <span id="color-blue">                                                        </span>
<span id="color-green">2'170.423</span> after window show                           <span id="color-blue">                                                        </span>
<span id="color-green">2'359.482</span> after first frame drawn                     <span id="color-blue">                                                             </span></pre>
\endraw
*/

/*!
    \qmlmethod StartupTimer::checkpoint(string name)

    Adds a new checkpoint with the given \a name, using the current system time. Each checkpoint
    corresponds to a single item in the output created by the next call to createReport.
*/

/*!
    \qmlmethod StartupTimer::createReport(string title)

    Outputs a report consisting of all checkpoints reported via the checkpoint function.
    The \a title will be appended to the header of the report.

    After outputting the report, all reported checkpoints will be cleared. This means that you can
    call this function multiple times and only newly reported checkpoints will be printed.
*/

QT_BEGIN_NAMESPACE_AM

StartupTimer::StartupTimer()
{
    ::atexit([]() { delete s_instance; });

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
        void *result = nullptr;

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
    void *threadJiffiesOk = nullptr;

    // using clone() with CLONE_VFORK would be more efficient, but it messes up the NPTL internal state
    if ((pthread_create(&pt, nullptr, readJiffiesFromProc, &threadJiffies) == 0)
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

    if (sysctl(mibNames, sizeof(mibNames) / sizeof(mibNames[0]), nullptr, &procInfoSize, nullptr, 0) == 0) {
        kinfo_proc *procInfo = (kinfo_proc *) malloc(procInfoSize);

        if (sysctl(mibNames, sizeof(mibNames) / sizeof(mibNames[0]), procInfo, &procInfoSize, nullptr, 0) == 0) {
            struct timeval now;

            if (gettimeofday(&now, nullptr) == 0) {
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

    if (m_initialized)
        m_timer.start();
}

StartupTimer *StartupTimer::s_instance = new StartupTimer();

StartupTimer *StartupTimer::instance()
{
    return s_instance;
}

StartupTimer::~StartupTimer()
{
    createReport();

    if (m_output && m_output != stderr)
        fclose(m_output);

    s_instance = nullptr;
}

void StartupTimer::checkpoint(const char *name)
{
    if (Q_LIKELY(m_initialized)) {
        qint64 delta = m_timer.nsecsElapsed();
        m_checkpoints << qMakePair(delta / 1000 + m_processCreation, name);
    }
}

void StartupTimer::checkpoint(const QString &name)
{
    QByteArray ba = name.toLocal8Bit();
    checkpoint(ba.constData());
}

void StartupTimer::createReport(const QString &title)
{
    if (m_output) {
        bool ansiColorSupport = false;
        if (m_output == stderr)
            getOutputInformation(&ansiColorSupport, nullptr, nullptr);

        if (!m_reportCreated && !m_checkpoints.isEmpty()) {
            const char *format = "\n== STARTUP TIMING REPORT: %s ==\n";
            if (ansiColorSupport)
                format = "\n\033[33m== STARTUP TIMING REPORT: %s ==\033[0m\n";
            fprintf(m_output, format, title.toLocal8Bit().data());
        }

        static const int barCols = 60;

        int delta = m_checkpoints.isEmpty() ? 0 : m_checkpoints.last().first;
        qreal usecPerCell = delta / barCols;

        int maxTextLen = 0;
        for (int i = 0; i < m_checkpoints.size(); ++i) {
            int textLen = m_checkpoints.at(i).second.length();
            if (textLen > maxTextLen)
                maxTextLen = textLen;
        }

        for (int i = 0; i < m_checkpoints.size(); ++i) {
            quint64 usec = m_checkpoints.at(i).first;
            const QByteArray text = m_checkpoints.at(i).second;
            int sec = 0;
            int cells = usec / usecPerCell;
            QByteArray bar(cells, ansiColorSupport ? ' ' : '#');
            QByteArray spacing(maxTextLen - text.length(), ' ');

            if (usec > 1000*1000) {
                sec = usec / (1000*1000);
                usec %= (1000*1000);
            }
            int msec = usec / 1000;
            usec %= 1000;

            const char *format = "%d'%03d.%03d %s %s#%s\n";
            if (ansiColorSupport)
                format = "\033[32m%d'%03d.%03d\033[0m %s %s\033[44m %s\033[0m\n";

            fprintf(m_output, format, sec, msec, int(usec), text.constData(), spacing.constData(), bar.constData());
        }
        fflush(m_output);

        m_checkpoints.clear();
        m_reportCreated = true;
    }
}

QT_END_NAMESPACE_AM
