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

#ifdef _WIN32
// needed for QueryFullProcessImageNameW
#  define WINVER _WIN32_WINNT_VISTA
#  define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

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
#  include <sys/sysinfo.h>
#  if !defined(SYS_gettid)
#    define SYS_gettid __NR_gettid
#  endif
#elif defined(Q_OS_OSX)
#  include <unistd.h>
#  include <sys/sysctl.h>
#endif

/*!
    \qmltype StartupTimer
    \inqmlmodule QtApplicationManager
    \ingroup system-ui-singletons
    \ingroup app-singletons
    \brief A tool for startup performance analysis.

    The StartupTimer is a class for measuring the startup performance of the System-UI, as well as
    applications started by application-manager.

    Using the checkpoint function, you can log the time that elapsed since the executable was
    started. In case of the System-UI, this is the time since the process was forked. This is also
    true for applications that are not quick-launched. Quick-launched applications attach to a
    process that has been pre-forked before the application has been started. In this case the
    timer will be reset to the actual application start. The time is reported using a monotonic
    clock with nano-second resolution - see QElapsedTimer for more information.

    \note On Linux, the actual time between the forking of the process and the first checkpoint
          can only be obtained with 10ms resolution.

    In order to activate startup timing measurement, the \c $AM_STARTUP_TIMER environment variable
    needs to be set: if set to \c 1, a startup performance analysis will be printed on the console.
    Anything other than \c 1 will be interpreted as the name of a file that is used instead of the
    console.

    When activated, this report will always be printed for the System-UI. If the application-manager
    is running in multi-process mode, additional reports will also be printed for every QML
    application that is started. Note that the bar widths can only be compared within a report.

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
<span id="color-orange">== STARTUP TIMING REPORT: System-UI ==</span>
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
     \qmlproperty int StartupTimer::timeToFirstFrame

     Provides the time from process start until rendering of the first frame in the HMI in
     milliseconds.

     \note Rendering of the first frame takes more time than just creating the QML root
     component. Accessing this property from within the \c Component.onCompleted signal might
     be too early.
*/


/*!
     \qmlproperty int StartupTimer::systemUpTime

     Provides the system's \e up time as provided by the underlying OS, measured up until the
     initialization of the StartupTimer singleton in milliseconds.

     This is helpful in calculating the time from boot to first frame drawn by adding up the
     values of systemUpTime and timeToFirstFrame.
*/


/*!
     \qmlproperty bool StartupTimer::automaticReporting

     You can set this property to \c false, if you want to prevent the automatic report generation
     that is done by the application-manager. This can be useful, if you are using some form of
     staged loading in the System-UI and want to create the report at a later time.

     \note Please note that you need to set this property to \c false before the load operation of
           the main qml file is finished: ideally in the root elements \c Component.onCompleted
           handler.

    The default value is \c true.

    \sa createReport
*/

/*!
    \qmlmethod StartupTimer::checkpoint(string name)

    Adds a new checkpoint with the elapsed time and the given \a name. Each checkpoint corresponds
    to a single item in the output created by the next call to createReport.
*/

/*!
    \qmlmethod StartupTimer::createReport(string title)

    Outputs a report consisting of all checkpoints reported via the checkpoint function.
    The \a title will be appended to the header of the report.

    After outputting the report, all reported checkpoints will be cleared. This means that you can
    call this function multiple times and only newly reported checkpoints will be printed.
*/

QT_BEGIN_NAMESPACE_AM

struct SplitSeconds
{
    int sec;
    int msec;
    int usec;
};

static SplitSeconds splitMicroSecs(quint64 micros)
{
    SplitSeconds ss;

    ss.sec = 0;
    if (micros > 1000 * 1000) {
        ss.sec = int(micros / (1000 * 1000));
        micros %= (1000 * 1000);
    }
    ss.msec = int(micros / 1000);
    ss.usec = micros % 1000;

    return ss;
}

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

    // Get system up time
    // Resource https://msdn.microsoft.com/en-us/library/windows/desktop/ms724411(v=vs.85).aspx
    if (m_initialized) {
        m_systemUpTime = GetTickCount64();
        emit systemUpTimeChanged(m_systemUpTime);
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

        QByteArray file = "/proc/self/task/" + QByteArray::number(static_cast<int>(syscall(SYS_gettid))) + "/stat";
        int fd = QT_OPEN(file, O_RDONLY);
        if (fd >= 0) {
            char buffer[1024];
            ssize_t bytesRead = QT_READ(fd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = 0;
                for (int field = 0, pos = 0; pos < bytesRead; ) {
                    if (buffer[pos++] == ' ') {
                        if (++field == 21) {
                            *reinterpret_cast<quint32 *>(resultPtr) = quint32(strtoul(buffer + pos, nullptr, 10));
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
            long int clkTck = sysconf(_SC_CLK_TCK);
            if (clkTck > 0) {
                m_processCreation = quint64(threadJiffies - processJiffies) * 1000*1000 / quint64(clkTck);
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

    // Checking the system up time
    if (m_initialized) {
        int fd = QT_OPEN("/proc/uptime", O_RDONLY);
        if (fd >= 0) {
            char buffer[32];
            ssize_t bytesRead = QT_READ(fd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = 0;
                m_systemUpTime = quint64(strtod(buffer, nullptr) * 1000);
                emit systemUpTimeChanged(m_systemUpTime);
            }
            QT_CLOSE(fd);
        }
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

    // Get system up time
    if (m_initialized) {
        struct timeval bootTime;
        size_t bootTimeLen = sizeof(bootTime);
        int mibNames[2] = { CTL_KERN, KERN_BOOTTIME };
        if (sysctl(mibNames, sizeof(mibNames) / sizeof(mibNames[0]), &bootTime, &bootTimeLen, nullptr, 0) == 0 ) {
            m_systemUpTime = (time(nullptr) - bootTime.tv_sec) * 1000; // we don't need more precision on macOS
            emit systemUpTimeChanged(m_systemUpTime);
        }
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
    if (!s_instance)
        s_instance = new StartupTimer();
    return s_instance;
}

StartupTimer::~StartupTimer()
{
    if (m_output && m_output != stderr)
        fclose(m_output);

    s_instance = nullptr;
}

void StartupTimer::checkpoint(const char *name)
{
    if (Q_LIKELY(m_initialized)) {
        qint64 delta = m_timer.nsecsElapsed();
        m_checkpoints << qMakePair(quint64(delta / 1000) + m_processCreation, name);
    }
}

void StartupTimer::checkpoint(const QString &name)
{
    if (Q_LIKELY(m_initialized)) {
        QByteArray ba = name.toLocal8Bit();
        checkpoint(ba.constData());
    }
}

void StartupTimer::checkFirstFrame()
{
    if (Q_LIKELY(m_initialized)) {
        QByteArray ba = "after first frame drawn";
        m_timeToFirstFrame = quint64(m_timer.nsecsElapsed() / 1000) + m_processCreation;
        m_checkpoints << qMakePair(m_timeToFirstFrame, ba);
        emit timeToFirstFrameChanged(m_timeToFirstFrame);
    }
}

void StartupTimer::reset()
{
    if (m_initialized) {
        SplitSeconds delta = splitMicroSecs(quint64(m_timer.nsecsElapsed() / 1000) + m_processCreation);
        m_timer.restart();
        m_checkpoints.clear();
        m_processCreation = 0;

        const QString text = QString::asprintf("started %d'%03d.%03d after process launch",
                                                         delta.sec, delta.msec, delta.usec);
        m_checkpoints << qMakePair(0, text.toLocal8Bit().constData());
    }
}

bool StartupTimer::automaticReporting() const
{
    return m_automaticReporting;
}

void StartupTimer::setAutomaticReporting(bool enableAutomaticReporting)
{
    if (m_automaticReporting != enableAutomaticReporting) {
        m_automaticReporting = enableAutomaticReporting;
        emit automaticReportingChanged(enableAutomaticReporting);
    }
}

void StartupTimer::createReport(const QString &title)
{
    if (m_output && !m_checkpoints.isEmpty()) {
        bool ansiColorSupport = false;
        if (m_output == stderr)
            getOutputInformation(&ansiColorSupport, nullptr, nullptr);

        const char *format = "\n== STARTUP TIMING REPORT: %s ==\n";
        if (ansiColorSupport)
            format = "\n\033[33m== STARTUP TIMING REPORT: %s ==\033[0m\n";
        fprintf(m_output, format, title.toLocal8Bit().data());

        static const int barCols = 60;

        quint64 delta = m_checkpoints.isEmpty() ? 0 : m_checkpoints.last().first;
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
            int cells = int(usec / usecPerCell);
            QByteArray bar(cells, ansiColorSupport ? ' ' : '#');
            QByteArray spacing(maxTextLen - text.length(), ' ');
            SplitSeconds ss = splitMicroSecs(usec);

            const char *format = "%d'%03d.%03d %s %s#%s\n";
            if (ansiColorSupport)
                format = "\033[32m%d'%03d.%03d\033[0m %s %s\033[44m %s\033[0m\n";

            fprintf(m_output, format, ss.sec, ss.msec, ss.usec,
                    text.constData(), spacing.constData(), bar.constData());
        }

        fflush(m_output);
        m_checkpoints.clear();
    }
}

quint64 StartupTimer::timeToFirstFrame() const
{
    return m_timeToFirstFrame / 1000;
}

quint64 StartupTimer::systemUpTime() const
{
    return m_systemUpTime;
}

QT_END_NAMESPACE_AM
