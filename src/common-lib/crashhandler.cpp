/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include "crashhandler.h"
#include "global.h"

#if !defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)

QT_BEGIN_NAMESPACE_AM

void CrashHandler::setCrashActionConfiguration(const QVariantMap &config)
{
    Q_UNUSED(config)
}

void CrashHandler::setQmlEngine(QQmlEngine *engine)
{
    Q_UNUSED(engine)
}

QT_END_NAMESPACE_AM

#else

#if defined(QT_QML_LIB)
#  include <QQmlEngine>
#  include <QtQml/private/qv4engine_p.h>
#  include <QtQml/private/qv8engine_p.h>
#endif

#include <cxxabi.h>
#include <execinfo.h>
#include <setjmp.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <stdio.h>

#if defined(AM_USE_LIBBACKTRACE)
#  include <libbacktrace/backtrace.h>
#  include <libbacktrace/backtrace-supported.h>
#endif

#include "unixsignalhandler.h"
#include "logging.h"
#include "utilities.h"
#include "processtitle.h"

QT_BEGIN_NAMESPACE_AM

enum PrintDestination { Console, Dlt };

static bool printBacktrace;
static bool printQmlStack;
static bool useAnsiColor;
static bool dumpCore;
static int waitForGdbAttach;

static char *demangleBuffer;
static size_t demangleBufferSize;

// this will make it run before all other static constructor functions
static void initBacktrace() __attribute__((constructor(101)));

static Q_NORETURN void crashHandler(const char *why, int stackFramesToIgnore);

void CrashHandler::setCrashActionConfiguration(const QVariantMap &config)
{
    printBacktrace = config.value(qSL("printBacktrace"), printBacktrace).toBool();
    printQmlStack = config.value(qSL("printQmlStack"), printQmlStack).toBool();
    waitForGdbAttach = config.value(qSL("waitForGdbAttach"), waitForGdbAttach).toInt() * timeoutFactor();
    dumpCore = config.value(qSL("dumpCore"), dumpCore).toBool();
}

#if defined(QT_QML_LIB)
static QQmlEngine *qmlEngine;

void CrashHandler::setQmlEngine(QQmlEngine *engine)
{
    qmlEngine = engine;
}
#endif

static void initBacktrace()
{
    // This can catch and pretty-print all of the following:

    // SIGFPE
    // volatile int i = 2;
    // int zero = 0;
    // i /= zero;

    // SIGSEGV
    // *((int *)1) = 1;

    // uncaught arbitrary exception
    // throw 42;

    // uncaught std::exception derived exception (prints what())
    // throw std::logic_error("test output");

    printBacktrace = true;
    printQmlStack = true;
    dumpCore = true;
    waitForGdbAttach = 0;

    getOutputInformation(&useAnsiColor, nullptr, nullptr);

    demangleBufferSize = 768;
    demangleBuffer = static_cast<char *>(malloc(demangleBufferSize));

    UnixSignalHandler::instance()->install(UnixSignalHandler::RawSignalHandler,
                                           { SIGFPE, SIGSEGV, SIGILL, SIGBUS, SIGPIPE, SIGABRT },
                                           [](int sig) {
        UnixSignalHandler::instance()->resetToDefault(sig);
        static char buffer[256];
        snprintf(buffer, sizeof(buffer), "uncaught signal %d (%s)", sig, UnixSignalHandler::signalName(sig));
        // 6 means to remove 6 stack frames: this way the backtrace starts at the point where
        // the signal reception interrupted the normal program flow
        crashHandler(buffer, 6);
    });

    std::set_terminate([]() {
        static char buffer [1024];

        auto type = abi::__cxa_current_exception_type();
        if (!type) {
            // 3 means to remove 3 stack frames: this way the backtrace starts at std::terminate
            crashHandler("terminate was called although no exception was thrown", 3);
        }

        const char *typeName = type->name();
        if (typeName) {
            int status;
            demangleBuffer = abi::__cxa_demangle(typeName, demangleBuffer, &demangleBufferSize, &status);
            if (status == 0 && *demangleBuffer) {
                typeName = demangleBuffer;
            }
        }
        try {
            throw;
        } catch (const std::exception &exc) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type %s (%s)", typeName, exc.what());
        } catch (...) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type %s", typeName);
        }

        // 4 means to remove 4 stack frames: this way the backtrace starts at std::terminate
        crashHandler(buffer, 4);
    });
}

static void printMsgToConsole(const char *format, ...) Q_ATTRIBUTE_FORMAT_PRINTF(1, 2);
static void printMsgToConsole(const char *format, ...)
{
    va_list arglist;
    va_start(arglist, format);
    vfprintf(stderr, format, arglist);
    va_end(arglist);
    fputs("\n", stderr);
}

static void printMsgToDlt(const char *format, ...) Q_ATTRIBUTE_FORMAT_PRINTF(1, 2);
static void printMsgToDlt(const char *format, ...)
{
    va_list arglist;
    va_start(arglist, format);
    Logging::logToDlt(QtMsgType::QtFatalMsg, QMessageLogContext(), QString::vasprintf(format, arglist));
    va_end(arglist);
}

static void printCrashInfo(PrintDestination dest, const char *why, int stackFramesToIgnore)
{
    using printMsgType = void (*)(const char *format, ...);
    static printMsgType printMsg;
    printMsg = (dest == Dlt) ? printMsgToDlt : printMsgToConsole;

    const char *title = ProcessTitle::title();
    char who[256];
    if (!title) {
        int whoLen = readlink("/proc/self/exe", who, sizeof(who) -1);
        who[qMax(0, whoLen)] = '\0';
        title = who;
    }

    pid_t pid = getpid();
    long tid = syscall(SYS_gettid);
    pthread_t pthreadId = pthread_self();
    char threadName[16];
    if (tid == pid)
        strcpy(threadName, "main");
    else if (pthread_getname_np(pthreadId, threadName, sizeof(threadName)))
        strcpy(threadName, "unknown");

    printMsg("\n*** process %s (%d) crashed ***", title, pid);
    printMsg("\n > why: %s", why);
    printMsg("\n > where: %s thread, TID: %d, pthread ID: %p", threadName, tid, pthreadId);

    if (printBacktrace) {
#if defined(AM_USE_LIBBACKTRACE) && defined(BACKTRACE_SUPPORTED)
        struct btData
        {
            backtrace_state *state;
            int level;
        };

        static auto printBacktraceLine = [](int level, const char *symbol, uintptr_t offset,
                                            const char *file = nullptr, int line = -1) {
            if (file) {
                printMsg(useAnsiColor ? " %3d: \x1b[1m%s\x1b[0m [\x1b[36m%" PRIxPTR "\x1b[0m]"
                                        " in \x1b[35m%s\x1b[0m:\x1b[35;1m%d\x1b[0m"
                                      : " %3d: %s [%" PRIxPTR "] in %s:%d", level, symbol, offset, file, line);
            } else {
                printMsg(useAnsiColor ? " %3d: \x1b[1m%s\x1b[0m [\x1b[36m%" PRIxPTR "\x1b[0m]"
                                      : " %3d: %s [%" PRIxPTR "]", level, symbol, offset);
            }
        };

        static auto errorCallback = [](void *data, const char *msg, int errnum) {
            const char *fmt = " %3d: ERROR: %s (%d)\n";
            if (useAnsiColor)
                fmt = " %3d: \x1b[31;1mERROR: \x1b[0;1m%s (%d)\x1b[0m";

            printMsg(fmt, static_cast<btData *>(data)->level, msg, errnum);
        };

        static auto syminfoCallback = [](void *data, uintptr_t pc, const char *symname, uintptr_t symval,
                                         uintptr_t symsize) {
            Q_UNUSED(symval)
            Q_UNUSED(symsize)

            int level = static_cast<btData *>(data)->level;
            if (symname) {
                int status;
                demangleBuffer = abi::__cxa_demangle(symname, demangleBuffer, &demangleBufferSize, &status);

                if (status == 0 && *demangleBuffer)
                    printBacktraceLine(level, demangleBuffer, pc);
                else
                    printBacktraceLine(level, symname, pc);
            } else {
                printBacktraceLine(level, nullptr, pc);
            }
        };

        static auto fullCallback = [](void *data, uintptr_t pc, const char *filename, int lineno,
                                      const char *function) -> int {
            if (function) {
                int status;
                demangleBuffer = abi::__cxa_demangle(function, demangleBuffer, &demangleBufferSize, &status);

                printBacktraceLine(static_cast<btData *>(data)->level,
                                   (status == 0 && *demangleBuffer) ? demangleBuffer : function,
                                   pc, filename ? filename : "<unknown>", lineno);
            } else {
                backtrace_syminfo (static_cast<btData *>(data)->state, pc, syminfoCallback, errorCallback, data);
            }
            return 0;
        };

        static auto simpleCallback = [](void *data, uintptr_t pc) -> int {
            backtrace_pcinfo(static_cast<btData *>(data)->state, pc, fullCallback, errorCallback, data);
            static_cast<btData *>(data)->level++;
            return 0;
        };

        struct backtrace_state *state = backtrace_create_state(nullptr, BACKTRACE_SUPPORTS_THREADS,
                                                               errorCallback, nullptr);

        printMsg("\n > backtrace:");
        btData data = { state, 0 };
        //backtrace_print(state, stackFramesToIgnore, stderr);
        backtrace_simple(state, stackFramesToIgnore, simpleCallback, errorCallback, &data);
#else // !defined(AM_USE_LIBBACKTRACE) && defined(BACKTRACE_SUPPORTED)
        Q_UNUSED(stackFramesToIgnore);
        void *addrArray[1024];
        int addrCount = backtrace(addrArray, sizeof(addrArray) / sizeof(*addrArray));

        if (!addrCount) {
            printMsg(" > no backtrace available");
        } else {
            char **symbols = backtrace_symbols(addrArray, addrCount);
            //backtrace_symbols_fd(addrArray, addrCount, 2);

            if (!symbols) {
                printMsg(" > no symbol names available");
            } else {
                printMsg("\n > backtrace:");
                for (int i = 1; i < addrCount; ++i) {
                    char *function = nullptr;
                    char *offset = nullptr;
                    char *end = nullptr;

                    for (char *ptr = symbols[i]; ptr && *ptr; ++ptr) {
                        if (!function && *ptr == '(')
                            function = ptr + 1;
                        else if (function && !offset && *ptr == '+')
                            offset = ptr;
                        else if (function && !end && *ptr == ')')
                            end = ptr;
                    }

                    if (function && offset && end && (function != offset)) {
                        *offset = 0;
                        *end = 0;

                        int status;
                        demangleBuffer = abi::__cxa_demangle(function, demangleBuffer, &demangleBufferSize, &status);

                        if (status == 0 && *demangleBuffer) {
                            printMsg(" %3d: %s [+%s]", i, demangleBuffer, offset + 1);
                        } else {
                            printMsg(" %3d: %s [+%s]", i, function, offset + 1);
                        }
                    } else  {
                        printMsg(" %3d: %s", i, symbols[i]);
                    }
                }
            }
        }
#endif // defined(AM_USE_LIBBACKTRACE) && defined(BACKTRACE_SUPPORTED)
    }

#if defined(QT_QML_LIB)
    if (printQmlStack && qmlEngine) {
        const QV4::ExecutionEngine *qv4engine = qmlEngine->handle();
        if (qv4engine) {
            const QV4::StackTrace stackTrace = qv4engine->stackTrace();
            if (stackTrace.size()) {
                printMsg("\n > qml stack:");
                int frame = 0;
                for (const auto &stackFrame : stackTrace) {
                    printMsg(useAnsiColor ? " %3d: \x1b[1m%s\x1b[0m in \x1b[35m%s\x1b[0m:\x1b[35;1m%d\x1b[0m"
                                          : " %3d: %s in %s:%d",
                             frame, stackFrame.function.toLocal8Bit().constData(),
                             stackFrame.source.toLocal8Bit().constData(), stackFrame.line);
                    frame++;
                }
            } else {
                printMsg("\n > qml stack: empty");
            }
        }
    }
#endif
}

static void crashHandler(const char *why, int stackFramesToIgnore)
{
    // We also need to reset all the "crash" signals plus SIGINT for three reasons:
    //  1) avoid recursions
    //  2) SIGABRT to re-enable standard abort() handling
    //  3) SIGINT, so that you can Ctrl+C the app if the crash handler ends up freezing
    UnixSignalHandler::instance()->resetToDefault({ SIGFPE, SIGSEGV, SIGILL, SIGBUS, SIGPIPE, SIGABRT, SIGINT });

    printCrashInfo(Console, why, stackFramesToIgnore);

    if (waitForGdbAttach > 0) {
        fprintf(stderr, "\n > the process will be suspended for %d seconds and you can attach a debugger"
                        " to it via\n\n   gdb -p %d\n", waitForGdbAttach, getpid());
        static jmp_buf jmpenv;
        signal(SIGALRM, [](int) {
            longjmp(jmpenv, 1);
        });
        if (!setjmp(jmpenv)) {
            alarm(static_cast<unsigned int>(waitForGdbAttach));

            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGALRM);
            sigsuspend(&mask);
        } else {
            fprintf(stderr, "\n > no gdb attached\n");
        }
    }

    if (Logging::isDltEnabled()) {
        useAnsiColor = false;
        printCrashInfo(Dlt, why, stackFramesToIgnore);
    }

    if (dumpCore) {
        fprintf(stderr, "\n > the process will be aborted (core dumped)\n\n");
        abort();
    }

    _exit(-1);
}

QT_END_NAMESPACE_AM

#endif // !Q_OS_LINUX || defined(Q_OS_ANDROID)
