// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QAtomicInt>
#include <QByteArray>
#include <QCoreApplication>

#include "console.h"
#include "utilities.h"

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <io.h>
#  include <QOperatingSystemVersion>
#  include <QThread>

Q_CORE_EXPORT void qWinMsgHandler(QtMsgType t, const char* str);
#else
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <signal.h>
#  if defined(Q_OS_MACOS)
#    include <libproc.h>
#  endif

#  include "unixsignalhandler.h"
#endif

struct ConsoleGlobal
{
    bool stdoutSupportsAnsiColor = false;
    bool stderrSupportsAnsiColor = false;
    bool stdoutIsConsoleWindow = false;
    bool stderrIsConsoleWindow = false;
    bool isRunningInQtCreator = false;
    QAtomicInt consoleWidthCached = 0;

    ConsoleGlobal();
};

Q_GLOBAL_STATIC(ConsoleGlobal, cg)

QT_USE_NAMESPACE_AM

ConsoleGlobal::ConsoleGlobal()
{
    enum { ColorAuto, ColorOff, ColorOn } forceColor = ColorAuto;
    const QByteArray forceColorOutput = qgetenv("AM_FORCE_COLOR_OUTPUT");
    if (forceColorOutput == "off" || forceColorOutput == "0")
        forceColor = ColorOff;
    else if (forceColorOutput == "on" || forceColorOutput == "1")
        forceColor = ColorOn;

#if defined(Q_OS_UNIX)
    if (::isatty(STDOUT_FILENO)) {
        stdoutIsConsoleWindow = true;
        stdoutSupportsAnsiColor = true;
    }
    if (::isatty(STDERR_FILENO)) {
        stderrIsConsoleWindow = true;
        stderrSupportsAnsiColor = true;
    }

#elif defined(Q_OS_WIN)
    if (HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE); (h != INVALID_HANDLE_VALUE)) {
        // enable ANSI mode on Windows 10
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            mode |= 0x04;
            if (SetConsoleMode(h, mode)) {
                stdoutSupportsAnsiColor = true;
                stdoutIsConsoleWindow = true;
            }
        }
    }
    if (HANDLE h = GetStdHandle(STD_ERROR_HANDLE); (h != INVALID_HANDLE_VALUE)) {
        // enable ANSI mode on Windows 10
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            mode |= 0x04;
            if (SetConsoleMode(h, mode)) {
                stdoutSupportsAnsiColor = true;
                stderrIsConsoleWindow = true;
            }
        }
    }
#endif

    qint64 pid = QCoreApplication::applicationPid();
    for (int level = 0; level < 5; ++level) { // only check 5 levels deep
        pid = getParentPid(pid);
        if (pid <= 1)
            break;

#if defined(Q_OS_LINUX)
        static QString checkCreator = qSL("/proc/%1/exe");
        QFileInfo fi(checkCreator.arg(pid));
        if (fi.symLinkTarget().contains(qSL("qtcreator"))) {
            isRunningInQtCreator = true;
            break;
        }
#elif defined(Q_OS_MACOS)
        static char buffer[PROC_PIDPATHINFO_MAXSIZE + 1];
        int len = proc_pidpath(pid, buffer, sizeof(buffer) - 1);
        if ((len > 0) && QByteArray::fromRawData(buffer, len).contains("Qt Creator")) {
            isRunningInQtCreator = true;
            break;
        }
#elif defined(Q_OS_WIN)
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);
        if (hProcess) {
            wchar_t exeName[1024] = { 0 };
            DWORD exeNameSize = sizeof(exeName) - 1;
            if (QueryFullProcessImageNameW(hProcess, 0, exeName, &exeNameSize)) {
                if (QString::fromWCharArray(exeName, exeNameSize).contains(qSL("qtcreator"))) {
                    isRunningInQtCreator = true;
                    break;
                }
            }
        }
#endif
    }

    if (forceColor != ColorAuto) {
        stdoutSupportsAnsiColor = stderrSupportsAnsiColor = (forceColor == ColorOn);
    } else {
        if (!stdoutSupportsAnsiColor)
            stdoutSupportsAnsiColor = isRunningInQtCreator;
        if (!stderrSupportsAnsiColor)
            stderrSupportsAnsiColor = isRunningInQtCreator;
    }

#if defined(Q_OS_UNIX) && defined(SIGWINCH)
    UnixSignalHandler::instance()->install(UnixSignalHandler::RawSignalHandler, SIGWINCH, [](int) {
        // we are in a signal handler, so we just clear the cached value in the atomic int
        cg()->consoleWidthCached = 0;
    });
#elif defined(Q_OS_WIN)
    class ConsoleThread : public QThread
    {
    public:
        ConsoleThread(QObject *parent)
            : QThread(parent)
        { }

        ~ConsoleThread()
        {
            terminate();
            wait();
        }

    protected:
        void run() override
        {
            HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
            DWORD mode = 0;
            if (!GetConsoleMode(h, &mode))
                return;
            if (!SetConsoleMode(h, mode | ENABLE_WINDOW_INPUT))
                return;

            INPUT_RECORD ir;
            DWORD irRead = 0;
            while (ReadConsoleInputW(h, &ir, 1, &irRead)) {
                if ((irRead == 1) && (ir.EventType == WINDOW_BUFFER_SIZE_EVENT))
                    cg()->consoleWidthCached = 0;
            }
        }
    };
    qAddPreRoutine([]() { (new ConsoleThread(QCoreApplication::instance()))->start(); });
#endif // Q_OS_WIN
}

QT_BEGIN_NAMESPACE_AM

bool Console::ensureInitialized()
{
    return cg();
}

bool Console::stdoutSupportsAnsiColor()
{
    return cg()->stdoutSupportsAnsiColor;
}

bool Console::stderrSupportsAnsiColor()
{
    return cg()->stderrSupportsAnsiColor;
}

bool Console::isRunningInQtCreator()
{
    return cg()->isRunningInQtCreator;
}

bool Console::stdoutIsConsoleWindow()
{
    return cg()->stdoutIsConsoleWindow;
}

bool Console::stderrIsConsoleWindow()
{
    return cg()->stderrIsConsoleWindow;
}

int Console::width()
{
    int consoleWidthCalculated = cg()->consoleWidthCached;

    if (consoleWidthCalculated <= 0) {
        if (cg()->stderrIsConsoleWindow) {
#if defined(Q_OS_UNIX)
            struct ::winsize ws;
            if ((::ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0) && (ws.ws_col > 0))
                consoleWidthCalculated = ws.ws_col;
#elif defined(Q_OS_WIN)
            HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(h, &csbi))
                consoleWidthCalculated = csbi.dwSize.X;
#endif
        }
        cg()->consoleWidthCached = consoleWidthCalculated;
    }
    if ((consoleWidthCalculated <= 0) && cg()->isRunningInQtCreator)
        return 120;
    else
        return consoleWidthCalculated;
}

QT_END_NAMESPACE_AM
