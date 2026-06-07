// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "startuptimer.h"
#include "console.h"
#include "colorprint.h"
#include "logging.h"
#include "utilities.h"
#include "unix-utilities.h"

#if defined(Q_OS_WIN)
#  include <windows.h>
#elif defined(Q_OS_LINUX)
#  include <QtCore/private/qcore_unix_p.h>
#  include <qplatformdefs.h>
#  include <sys/syscall.h>
#  include <sys/sysinfo.h>
#  if !defined(SYS_gettid)
#    define SYS_gettid __NR_gettid
#  endif
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <unistd.h>
#  include <sys/sysctl.h>
#endif

/*!
    \qmltype StartupTimer
    \inqmlmodule QtApplicationManager
    \ingroup common-singletons
    \brief A tool for startup performance analysis.

    The StartupTimer is a class for measuring the startup performance of the System UI, as well as
    applications started by the application manager.

    Using the checkpoint function, you can log the time that elapsed since the executable was
    started. In case of the System UI, this is the time since the process was forked. This is also
    true for applications that are not quick-launched. Quick-launched applications attach to a
    process that has been pre-forked before the application has been started. In this case the
    timer will be reset to the actual application start. The time is reported using a monotonic
    clock with nano-second resolution - see QElapsedTimer for more information.

    \note On Linux, the actual time between the forking of the process and the first checkpoint
          can only be obtained with 10ms resolution.

    In order to activate startup timing measurement, the \c $AM_STARTUP_TIMER environment variable
    needs to be set: if set to \c 1, a startup performance analysis will be logged in the
    \c am.startup category with level \c info. Anything other than \c 1 will be interpreted as the
    name of a file to log to instead.

    When activated, this report will always be printed for the System UI. If the application manager
    is running in multi-process mode, additional reports will also be printed for every QML
    application that is started. Note that the bar widths can only be compared within a report.

    The application manager and its QML launcher will already create a lot of checkpoints on their
    own and will also call createReport themselves after all the C++ side setup has finished. You
    can however add arbitrary checkpoints yourself using the QML API: access to the StartupTimer
    object is possible through a the \c StartupTimer root-context property in the QML engine.

    All checkpoints will also be logged to the \c am.startup logging category as \c debug messages,
    which helps to correlate the timings with log output from other components.

    This is an example output, starting the \c Minidesk example on a console with ANSI color support:

    \image startup-timer-example.png
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
     that is done by the application manager. This can be useful, if you are using some form of
     staged loading in the System UI and want to create the report at a later time.

     \note Please note that you need to set this property to \c false before the load operation of
           the main qml file is finished: ideally in the root elements \c Component.onCompleted
           handler.

    The default value is \c true.

    \sa createReport
*/

/*!
    \qmlmethod void StartupTimer::checkpoint(string name)

    Adds a new checkpoint with the elapsed time and the given \a name. Each checkpoint corresponds
    to a single item in the output created by the next call to createReport.
*/

/*!
    \qmlmethod void StartupTimer::createReport(string title)

    Outputs a report consisting of all checkpoints reported via the checkpoint function.
    The \a title will be appended to the header of the report.

    After outputting the report, all reported checkpoints will be cleared. This means that you can
    call this function multiple times and only newly reported checkpoints will be printed.
*/

QT_BEGIN_NAMESPACE_AM

QByteArray StartupTimer::formatMicroSecs(quint64 micros)
{
    int sec = 0;
    if (micros > 1000 * 1000) {
        sec = int(micros / (1000 * 1000));
        micros %= (1000 * 1000);
    }
    int msec = int(micros / 1000);
    int usec = int(micros % 1000);

    std::array<char, 20> timeBuffer;
    timeBuffer[0] = '\0';
    std::snprintf(timeBuffer.data(), timeBuffer.size(), "%d'%03d.%03d", sec, msec, usec);
    return { timeBuffer.data() };
}

StartupTimer::StartupTimer()
{
    ::atexit([]() { delete s_instance; });

    QByteArray useTimer = qgetenv("AM_STARTUP_TIMER");

    if (useTimer.isNull()) {
        return;
    } else if (useTimer.isEmpty() || useTimer == "1") {
        m_outputToLogger = true;
        updateLoggingCategory();
    } else {
        m_output = ::fopen(useTimer.constData(), "w");
    }

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
    if (m_initialized)
        m_systemUpTime = GetTickCount64();

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
        Unix::Fd fd { qt_safe_open(file.constData(), O_RDONLY | O_CLOEXEC) };
        if (fd) {
            std::array<char, 1024> buffer;
            ssize_t bytesRead = qt_safe_read(fd.get(), buffer.data(), buffer.size() - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = 0;
                for (int field = 0, pos = 0; pos < bytesRead; ) {
                    if (buffer.at(pos++) == ' ') {
                        if (++field == 21) {
                            const QByteArrayView fieldView { buffer.data() + pos, bytesRead - pos };
                            auto jiffies = fieldView.mid(0, fieldView.indexOf(' ')).toUInt();
                            *static_cast<quint32 *>(resultPtr) = jiffies;
                            result = reinterpret_cast<void *>(1);
                            break;
                        }
                    }
                }
            }
        }
        return result;
    };

    quint32 threadJiffies = 0;
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
        Unix::Fd fd { qt_safe_open("/proc/uptime", O_RDONLY | O_CLOEXEC) };
        if (fd) {
            std::array<char, 32> buffer;
            ssize_t bytesRead = qt_safe_read(fd.get(), buffer.data(), buffer.size() - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = 0;
                const QByteArrayView fieldView { buffer.data(), bytesRead };
                auto uptime = fieldView.mid(0, fieldView.indexOf(' ')).toDouble();
                m_systemUpTime = quint64(uptime * 1000);
            }
        }
    }

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    std::array<int, 4> mib { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    kinfo_proc procInfo;
    size_t procInfoSize = sizeof(procInfo);

    if (sysctl(mib.data(), mib.size(), &procInfo, &procInfoSize, nullptr, 0) == 0) {
        struct timeval now;

        if (gettimeofday(&now, nullptr) == 0) {
            m_processCreation = (quint64(now.tv_sec) * 1000000 + quint64(now.tv_usec))
                                - (quint64(procInfo.kp_proc.p_un.__p_starttime.tv_sec) * 1000000
                                   + quint64(procInfo.kp_proc.p_un.__p_starttime.tv_usec));
            m_initialized = true;
        }
    }

    // Get system up time
    if (!m_initialized) {
        qWarning("StartupTimer: could not get kinfo_proc from kernel");
    } else {
        std::array<int, 2> mib { CTL_KERN, KERN_BOOTTIME };
        struct timeval bootTime;
        size_t bootTimeSize = sizeof(bootTime);

        if (sysctl(mib.data(), mib.size(), &bootTime, &bootTimeSize, nullptr, 0) == 0)
            m_systemUpTime = quint64(time(nullptr) - bootTime.tv_sec) * 1000; // we don't need more precision on macOS
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
    if (m_output)
        ::fclose(m_output);

    s_instance = nullptr;
}

void StartupTimer::checkpoint(const char *name)
{
    if (Q_LIKELY(m_initialized)) {
        qint64 delta = m_timer.nsecsElapsed();
        m_checkpoints << std::make_pair(quint64(delta / 1000) + m_processCreation, name);

        qCDebug(LogStartupTimer) << "Checkpoint:" << name;
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
        m_checkpoints << std::make_pair(m_timeToFirstFrame, ba);
        emit timeToFirstFrameChanged(m_timeToFirstFrame);
    }
}

void StartupTimer::reset()
{
    if (m_initialized) {
        m_timer.start();
        m_checkpoints.clear();
        m_processCreation = 0;
    }
}

void StartupTimer::createAutomaticReport(const QString &title)
{
    if (m_automaticReporting)
        createReport(title);
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

void StartupTimer::updateLoggingCategory()
{
    if (m_outputToLogger)
        const_cast<QLoggingCategory &>(LogStartupTimer()).setEnabled(QtInfoMsg, true);
}

void StartupTimer::createReport(const QString &title)
{
    if ((m_output || m_outputToLogger) && !m_checkpoints.isEmpty()) {
        bool ansiColorSupport = m_outputToLogger
                                && Logging::isLoggingToStderr()
                                && Console::stderrSupportsAnsiColor();
        const char cellChar = ansiColorSupport ? ' ' : '#';

        QByteArray buffer;
        ColorPrint cprt(buffer, ansiColorSupport);

        cprt << '\n' << ColorPrint::yellow << "== STARTUP TIMING REPORT: " << title << " =="
             << ColorPrint::reset << '\n';

        static const int barCols = 20;

        quint64 usecLast = 0;
        quint64 usecMax = m_checkpoints.isEmpty() ? 0 : m_checkpoints.constLast().first;
        quint64 usecDeltaMax = 0;
        qsizetype textLenMax = 0;

        for (const auto &[usecTotal, text] : std::as_const(m_checkpoints)) {
            qsizetype textLen = text.length();
            if (textLen > textLenMax)
                textLenMax = textLen;
            auto usecDelta = usecTotal - usecLast;
            if (usecDelta > usecDeltaMax)
                usecDeltaMax = usecDelta;
            usecLast = usecTotal;
        }

        quint64 usecPerCell = usecMax ? (usecMax / barCols) : 1;
        quint64 usecDeltaPerCell = usecDeltaMax ? (usecDeltaMax / barCols) : 1;

        cprt << ColorPrint::bcyan << "Abs.  [s'ms.us]   Rel. Checkpoint"
             << ColorPrint::repeat(textLenMax - 9, ' ') << "> Abs. graph"
             << ColorPrint::repeat(barCols * 2 - 21, ' ') << "Rel. graph <\n"
             << ColorPrint::reset;

        usecLast = 0;
        for (const auto &[usecTotal, text] : std::as_const(m_checkpoints)) {
            auto cells = qsizetype(usecTotal / usecPerCell);
            QByteArray timeString = formatMicroSecs(usecTotal);
            auto usecDelta = usecTotal - usecLast;
            auto deltaCells = qsizetype(usecDelta / usecDeltaPerCell);
            QByteArray deltaTimeString = formatMicroSecs(usecDelta);
            usecLast = usecTotal;

            cprt << ColorPrint::bgreen << timeString << ColorPrint::reset
                 << " (+" << ColorPrint::byellow << deltaTimeString << ColorPrint::reset << ") "
                 << text << ColorPrint::repeat(textLenMax - text.length() + 1, ' ')
                 << ColorPrint::greenBg << ColorPrint::green
                 << ColorPrint::repeat(cells + 1, cellChar)
                 << ColorPrint::reset
                 << ColorPrint::repeat(barCols - cells + 1 + barCols - deltaCells, ' ')
                 << ColorPrint::yellowBg << ColorPrint::yellow
                 << ColorPrint::repeat(deltaCells + 1, cellChar)
                 << ColorPrint::reset << '\n';
        }
        m_checkpoints.clear();

        if (m_outputToLogger) {
            qCInfo(LogStartupTimer) << buffer.constData();
        } else if (m_output) {
            ::fputs(buffer.constData(), m_output);
            ::fflush(m_output);
#if defined(Q_OS_UNIX)
            ::fsync(::fileno(m_output));
#endif
        }
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

#include "moc_startuptimer.cpp"
