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

#include <inttypes.h>
#include <typeinfo>

#if defined(QT_QML_LIB)
#  include <QPointer>
#  include <QQmlEngine>
#  include <QtQml/private/qv4engine_p.h>
#endif

#include <QLibraryInfo>

#include "crashhandler.h"
#include "global.h"
#include "utilities.h"
#include "logging.h"

#if defined(Q_OS_UNIX)
#  include<unistd.h>
#  if defined(Q_OS_MACOS)
#    define AM_PTHREAD_T_FMT "%p"
#  else
#    define AM_PTHREAD_T_FMT "%lx"
#  endif
#elif defined(Q_OS_WINDOWS)
#  include <io.h>
#  if !defined(STDERR_FILENO)
#    define STDERR_FILENO _fileno(stderr)
#  endif
#  define write(a, b, c) _write(a, b, static_cast<unsigned int>(c))
#  if defined(Q_CC_MINGW)
#    include <cxxabi.h>
#  endif
#endif

QT_BEGIN_NAMESPACE_AM

static bool printBacktrace;
static bool printQmlStack;
static bool dumpCore;
static int waitForGdbAttach;

static char *demangleBuffer;
static size_t demangleBufferSize;

static QByteArray *backtraceLineOut;
static QByteArray *backtraceLineTmp;

static QPointer<QQmlEngine> qmlEngine;


void CrashHandler::setCrashActionConfiguration(const QVariantMap &config)
{
    printBacktrace = config.value(qSL("printBacktrace"), printBacktrace).toBool();
    printQmlStack = config.value(qSL("printQmlStack"), printQmlStack).toBool();
    waitForGdbAttach = config.value(qSL("waitForGdbAttach"), waitForGdbAttach).toInt() * timeoutFactor();
    dumpCore = config.value(qSL("dumpCore"), dumpCore).toBool();
}


void CrashHandler::setQmlEngine(QQmlEngine *engine)
{
#if defined(QT_QML_LIB)
    qmlEngine = engine;
#else
    Q_UNUSED(engine)
#endif
}

#if defined(Q_OS_WINDOWS) || (defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID))

#  if defined(Q_OS_UNIX)
// this will make it run before all other static constructor functions
static void initBacktrace() __attribute__((constructor(101)));

static void initBacktraceUnix();

#  elif defined(Q_OS_WINDOWS) && defined(Q_CC_MSVC)
// this will make it run before all other static constructor functions
#    pragma warning(push)
#    pragma warning(disable: 4074)
#    pragma init_seg(compiler)

static void initBacktrace();
static void initBacktraceWindows();

static struct InitBacktrace
{
    InitBacktrace() { initBacktrace(); }
} dummy;
#    pragma warning(pop)

#  elif defined(Q_OS_WINDOWS) && defined(Q_CC_MINGW)
// this will make it run before all other static constructor functions
static void initBacktrace() __attribute__((constructor(101)));

static void initBacktraceWindows();

#  endif

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

    demangleBufferSize = 768;
    demangleBuffer = static_cast<char *>(malloc(demangleBufferSize));

    backtraceLineOut = new QByteArray();
    backtraceLineOut->reserve(2 * int(demangleBufferSize));
    backtraceLineTmp = new QByteArray();
    backtraceLineTmp->reserve(32);

    Console::init();

#  if defined(Q_OS_UNIX)
    initBacktraceUnix();
#  elif defined(Q_OS_WINDOWS)
    initBacktraceWindows();
#  endif
}

enum LogToDestination { Console = 0, Dlt = 1 };

static void logMsg(LogToDestination logTo, const char *msg, int msgLen = -1)
{
    if (logTo == Console) {
        auto dummy = write(STDERR_FILENO, msg, msgLen >= 0 ? size_t(msgLen) : strlen(msg));
        dummy = write(STDERR_FILENO, "\n", 1);
        Q_UNUSED(dummy)
    } else if (logTo == Dlt) {
        Logging::logToDlt(QtMsgType::QtFatalMsg, QMessageLogContext(), QLatin1String(msg, msgLen));
    }
}

static void logMsgF(LogToDestination logTo, const char *format, ...) Q_ATTRIBUTE_FORMAT_PRINTF(2, 3);
static void logMsgF(LogToDestination logTo, const char *format, ...)
{
    char buffer[512];
    va_list arglist;
    va_start(arglist, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, arglist);
    va_end(arglist);
    logMsg(logTo, buffer, qMin(len, int(sizeof(buffer)) - 1));
}

static void logBacktraceLine(LogToDestination logTo, int level, const char *symbol,
                             uintptr_t offset = 0, const char *file = nullptr, int line = -1,
                             int errorCode = 0, const char *errorString = nullptr)
{
    bool wantClickableUrl = (Console::isRunningInQtCreator && !Console::hasConsoleWindow);
    bool forceNoColor = (logTo != Console);
    bool isError = (errorString);

    backtraceLineOut->clear();
    backtraceLineTmp->setNum(level);
    backtraceLineOut->append(4 - backtraceLineTmp->size(), ' ');
    backtraceLineOut->append(*backtraceLineTmp);
    backtraceLineOut->append(": ");
    Console::colorize(*backtraceLineOut, Console::BrightFlag, forceNoColor);
    if (isError) {
        Console::colorize(*backtraceLineOut, Console::Red, forceNoColor);
        backtraceLineOut->append("ERROR: ");
        Console::colorize(*backtraceLineOut, Console::Off, forceNoColor);
        backtraceLineOut->append(errorString);
        backtraceLineTmp->setNum(errorCode);
        backtraceLineOut->append(" (");
        backtraceLineOut->append(*backtraceLineTmp);
        backtraceLineOut->append(")");
    } else {
        backtraceLineOut->append(symbol ? symbol : "?");
    }
    Console::colorize(*backtraceLineOut, Console::Off, forceNoColor);
    if (offset) {
        backtraceLineTmp->setNum(static_cast<qulonglong>(offset), 16);

        backtraceLineOut->append(" [");
        Console::colorize(*backtraceLineOut, Console::Cyan, forceNoColor);
        backtraceLineOut->append(*backtraceLineTmp);
        Console::colorize(*backtraceLineOut, Console::Off, forceNoColor);
        backtraceLineOut->append(']');
    }
    if (file) {
        bool lineValid = (line > 0 && line <= 9999999);
        if (lineValid || wantClickableUrl)
            backtraceLineTmp->setNum(lineValid ? line : 0);

        backtraceLineOut->append(" in ");
        Console::colorize(*backtraceLineOut, Console::Magenta, forceNoColor);

        static const char *filePrefix =
#  if defined(Q_OS_WINDOWS)
                "file:///";
#  else
                "file://";
#  endif

        bool isFileUrl = (strncmp(file, filePrefix, sizeof(filePrefix) - 1) == 0);
        if (!isFileUrl && wantClickableUrl)
            backtraceLineOut->append(filePrefix);
        backtraceLineOut->append((isFileUrl && !wantClickableUrl) ? file + sizeof(filePrefix) - 1 : file);
        Console::colorize(*backtraceLineOut, Console::Off, forceNoColor);

        if (lineValid || wantClickableUrl) {
            backtraceLineOut->append(':');
            Console::colorize(*backtraceLineOut, Console::BrightFlag | Console::Magenta, forceNoColor);
            backtraceLineOut->append(*backtraceLineTmp);
            Console::colorize(*backtraceLineOut, Console::Off, forceNoColor);
        }
    }

    logMsg(logTo, backtraceLineOut->constData());
}

static void logQmlBacktrace(LogToDestination logTo)
{
#  if defined(QT_QML_LIB)
    if (printQmlStack && qmlEngine) {
        const QV4::ExecutionEngine *qv4engine = qmlEngine->handle();
        if (qv4engine) {
            const QV4::StackTrace stackTrace = qv4engine->stackTrace();
            if (stackTrace.size()) {
                logMsg(logTo, "\n > QML backtrace:");
                for (int frame = 0; frame < stackTrace.size(); ++frame) {
                    const auto &stackFrame = stackTrace.at(frame);
                    logBacktraceLine(logTo, frame, stackFrame.function.toLocal8Bit().constData(),
                                       0, stackFrame.source.toLocal8Bit().constData(), stackFrame.line);
                }
            } else {
                logMsg(logTo, "\n > QML backtrace: empty");
            }
        }
    }
#  endif
}

#endif // defined(Q_OS_WINDOWS) || (defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID))

QT_END_NAMESPACE_AM

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)

#  include <cxxabi.h>
#  include <execinfo.h>
#  include <setjmp.h>
#  include <signal.h>
#  include <sys/syscall.h>
#  include <pthread.h>
#  include <stdio.h>

#  if defined(AM_USE_LIBBACKTRACE)
#    include <libbacktrace/backtrace.h>
#    include <libbacktrace/backtrace-supported.h>
#  endif
#  if defined(Q_OS_LINUX)
#    include <dlfcn.h>
#  elif defined(Q_OS_MACOS)
#    include <mach-o/dyld.h>
#  endif

#  include "unixsignalhandler.h"
#  include "processtitle.h"

QT_BEGIN_NAMESPACE_AM

Q_NORETURN static void crashHandler(const char *why, int stackFramesToIgnore);

static void initBacktraceUnix()
{
#  if defined(Q_OS_LINUX)
    dlopen("libgcc_s.so.1", RTLD_GLOBAL | RTLD_LAZY);
#  endif

    UnixSignalHandler::instance()->install(UnixSignalHandler::RawSignalHandler,
                                           { SIGFPE, SIGSEGV, SIGILL, SIGBUS, SIGPIPE, SIGABRT, SIGQUIT, SIGSYS },
                                           [](int sig) {
        UnixSignalHandler::instance()->resetToDefault(sig);
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "uncaught signal %d (%s)", sig, UnixSignalHandler::signalName(sig));
        // 8 means to remove 8 stack frames: this way the backtrace starts at the point where
        // the signal reception interrupted the normal program flow
        crashHandler(buffer, 8);
    });

    std::set_terminate([]() {
        char buffer [1024];

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
        } catch (const std::exception *exc) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type %s (%s)", typeName, exc->what());
        } catch (const char *exc) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type 'const char *' (%s)", exc);
        } catch (...) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type %s", typeName);
        }

        // 4 means to remove 4 stack frames: this way the backtrace starts at std::terminate
        crashHandler(buffer, 4);
    });

    // create a new process group, so that we are able to kill all children with ::kill(0, ...)
    setpgid(0, 0);
}

static void logCrashInfo(LogToDestination logTo, const char *why, int stackFramesToIgnore)
{
    const char *title = ProcessTitle::title();
    char who[256];
    if (!title) {
        ssize_t whoLen = readlink("/proc/self/exe", who, sizeof(who) -1);
        who[std::max(ssize_t(0), whoLen)] = '\0';
        title = who;
    }

    pid_t pid = getpid();
#if defined(Q_OS_LINUX)
    long tid = syscall(SYS_gettid);
    bool isMainThread = (tid == pid);
#else
    long tid = -1;
    bool isMainThread = pthread_main_np();
#endif
    pthread_t pthreadId = pthread_self();
    char threadName[16];

    if (isMainThread)
        strcpy(threadName, "main");
    else if (pthread_getname_np(pthreadId, threadName, sizeof(threadName)))
        strcpy(threadName, "unknown");

    logMsgF(logTo, "\n*** process %s (%d) crashed ***", title, pid);
    logMsgF(logTo, "\n > why: %s", why);
    logMsgF(logTo, "\n > where: %s thread, TID: %ld, pthread ID: " AM_PTHREAD_T_FMT,
            threadName, tid, pthreadId);

    if (printBacktrace) {
        if (!QLibraryInfo::isDebugBuild()) {
            logMsgF(logTo, "\n > Your binary is built in release mode. The following backtrace might be inaccurate.");
            logMsgF(logTo, " > Please consider using a debug build for a more accurate backtrace.");
#  if defined(Q_OS_MACOS)
            logMsgF(logTo, " > E.g. by running the binary using DYLD_IMAGE_SUFFIX=_debug");
#  endif
        }

#  if defined(AM_USE_LIBBACKTRACE) && defined(BACKTRACE_SUPPORTED)
        struct btData
        {
            LogToDestination logTo;
            backtrace_state *state;
            int level;
        };

        static auto errorCallback = [](void *data, const char *msg, int errnum) {
            auto btdata = static_cast<btData *>(data);
            logBacktraceLine(btdata->logTo, btdata->level, nullptr, 0, nullptr, 0, errnum, msg);
        };

        static auto syminfoCallback = [](void *data, uintptr_t pc, const char *symname,
                                         uintptr_t symval, uintptr_t symsize) {
            Q_UNUSED(symval)
            Q_UNUSED(symsize)

            auto btdata = static_cast<btData *>(data);
            const char *name = nullptr;

            if (symname) {
                int status;
                demangleBuffer = abi::__cxa_demangle(symname, demangleBuffer, &demangleBufferSize, &status);
                name = (status == 0 && *demangleBuffer) ? demangleBuffer : symname;
            }
            logBacktraceLine(btdata->logTo, btdata->level, name, pc);
        };

        static auto fullCallback = [](void *data, uintptr_t pc, const char *filename, int lineno,
                                      const char *function) -> int {
            auto btdata = static_cast<btData *>(data);

            if (function) {
                int status;
                demangleBuffer = abi::__cxa_demangle(function, demangleBuffer, &demangleBufferSize, &status);

                logBacktraceLine(btdata->logTo, btdata->level,
                                 (status == 0 && *demangleBuffer) ? demangleBuffer : function,
                                 pc, filename, lineno);
            } else {
                backtrace_syminfo(btdata->state, pc, syminfoCallback, errorCallback, data);
            }
            return 0;
        };

        static auto simpleCallback = [](void *data, uintptr_t pc) -> int {
            auto btdata = static_cast<btData *>(data);
            backtrace_pcinfo(btdata->state, pc, fullCallback, errorCallback, data);
            btdata->level++;
            return 0;
        };

        char *executable_path = nullptr;
#    if defined(Q_OS_MACOS)
        char executable_path_buf[1024];
        uint32_t executable_path_buf_len = sizeof(executable_path_buf);
        if (_NSGetExecutablePath(executable_path_buf, &executable_path_buf_len) == 0)
            executable_path = executable_path_buf;
#    endif

        struct backtrace_state *state = backtrace_create_state(executable_path, BACKTRACE_SUPPORTS_THREADS,
                                                               errorCallback, nullptr);

        logMsg(logTo, "\n > C++ backtrace:");
        btData data = { logTo, state, 0 };
        //backtrace_print(state, stackFramesToIgnore, stderr);
        backtrace_simple(state, stackFramesToIgnore, simpleCallback, errorCallback, &data);

#  else // !defined(AM_USE_LIBBACKTRACE) && defined(BACKTRACE_SUPPORTED)
        Q_UNUSED(stackFramesToIgnore);
        void *addrArray[1024];
        int addrCount = backtrace(addrArray, sizeof(addrArray) / sizeof(*addrArray));

        if (!addrCount) {
            logMsg(logTo, " > no C++ backtrace available");
        } else {
            //backtrace_symbols_fd(addrArray, addrCount, 2);
            char **symbols = backtrace_symbols(addrArray, addrCount);

            if (!symbols) {
                logMsg(logTo, " > no symbol names available");
            } else {
                logMsg(logTo, "\n > C++ backtrace:");
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

                    const char *name = nullptr;

                    if (function && offset && end && (function != offset)) {
                        *offset = 0;
                        *end = 0;

                        int status;
                        demangleBuffer = abi::__cxa_demangle(function, demangleBuffer, &demangleBufferSize, &status);
                        name = (status == 0 && *demangleBuffer) ? demangleBuffer : function;
                    } else {
                        name = symbols[i];
                        if (function && (function == offset))
                            *(function - 1) = 0;
                    }
                    logBacktraceLine(logTo, i, name, offset ? strtoull(offset + 1, nullptr, 16) : 0);
                }
            }
        }
#  endif // defined(AM_USE_LIBBACKTRACE) && defined(BACKTRACE_SUPPORTED)
    }
    logQmlBacktrace(logTo);
}

static void crashHandler(const char *why, int stackFramesToIgnore)
{
    // We also need to reset all the "crash" signals plus SIGINT for three reasons:
    //  1) avoid recursions
    //  2) SIGABRT to re-enable standard abort() handling
    //  3) SIGINT, so that you can Ctrl+C the app if the crash handler ends up freezing
    UnixSignalHandler::instance()->resetToDefault({ SIGFPE, SIGSEGV, SIGILL, SIGBUS,
                                                    SIGPIPE, SIGABRT, SIGINT, SIGQUIT, SIGSYS });

    logCrashInfo(Console, why, stackFramesToIgnore);

    if (waitForGdbAttach > 0) {
        logMsgF(Console, "\n > the process will be suspended for %d seconds and you can attach a debugger"
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
            logMsg(Console, "\n > no gdb attached\n");
        }
    }

    if (Logging::deferredMessages()) {
        logMsg(Console, "\n > Accumulated logging output\n");
        Logging::completeSetup();
    }

#  if defined(Q_OS_LINUX)
    if (Logging::isDltEnabled())
        logCrashInfo(Dlt, why, stackFramesToIgnore);
#  endif

    // make sure to terminate our sub-process as well, but not ourselves
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);

    if (dumpCore) {
        logMsg(Console, "\n > the process will be aborted (core dumped)\n");
        abort();
    }

    _exit(-1);
}

QT_END_NAMESPACE_AM

#elif defined(Q_OS_WINDOWS)

#  include <windows.h>
#  include <dbghelp.h>
#  if defined(AM_USE_STACKWALKER)
#    include <stackwalker.h>
#  endif

QT_BEGIN_NAMESPACE_AM

static DWORD mainThreadId = GetCurrentThreadId();

#  if defined(AM_USE_STACKWALKER)

class AMStackWalker : public StackWalker
{
public:
    AMStackWalker(LogToDestination logTo, int stackFramesToIgnore)
        : StackWalker(RetrieveLine | RetrieveSymbol)
        , logToDestination(logTo)
        , ignoreUpToLevel(stackFramesToIgnore)
    { }
    void OnOutput(LPCSTR) override { }
    void OnDbgHelpErr(LPCSTR errorString, DWORD lastError, DWORD64 address) override
    {
        // ignore SymGetLineFromAddr64 "errors" - they are not really errors, but just missing
        // debug symbol information.
        if (strcmp(errorString, "SymGetLineFromAddr64") || (lastError != 487))
            logBacktraceLine(logToDestination, 0, nullptr, address, nullptr, 0, lastError, errorString);
    }
    void OnSymInit(LPCSTR, DWORD, LPCSTR) override { }
    void OnLoadModule(LPCSTR, LPCSTR, DWORD64, DWORD, DWORD, LPCSTR, LPCSTR, ULONGLONG) override { }
    void OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry) override
    {
        if (eType == firstEntry)
            level = 0;
        else
            ++level;
        if ((eType == lastEntry) || (entry.offset == 0) || (level < ignoreUpToLevel))
            return;

        const char *symbol = entry.undFullName[0] ? entry.undFullName
                                                  : entry.undName[0] ? entry.undName
                                                                     : entry.name[0] ? entry.name
                                                                                     : nullptr;
        const char *file = entry.lineFileName[0] ? entry.lineFileName : nullptr;

        logBacktraceLine(logToDestination, level - ignoreUpToLevel, symbol, entry.offset,
                         file, entry.lineNumber);
    }

private:
    LogToDestination logToDestination;
    int level = 0;
    int ignoreUpToLevel = 0;
};

#  endif // defined(AM_USE_STACKWALKER)

static void logCrashInfo(LogToDestination logTo, const char *why, int stackFramesToIgnore, CONTEXT *context)
{
    WCHAR title[256];
    DWORD titleLen = sizeof(title) / sizeof(*title);
    if (QueryFullProcessImageName(GetCurrentProcess(), /*PROCESS_NAME_NATIVE*/0, title, &titleLen)) {
        title[qMax(DWORD(0), titleLen)] = '\0';
    }

    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();
    WCHAR threadName[256];
    // Qt uses the the legacy RaiseException mechanism to attach the thread name instead of the
    // modern setThreadDescription() call, but this name is only visible to debuggers.
    if (tid == mainThreadId) {
        wcscpy(threadName, L"main");
    } else {
        wcscpy(threadName, L"unknown");

        // GetThreadDescription is only available from Windows 10, 1607 onwards, regardless of the
        // SDK used to compile this code
        typedef HRESULT(WINAPI* GetThreadDescriptionType)(HANDLE hThread, PWSTR *description);
        if (auto GetThreadDescriptionFunc = reinterpret_cast<GetThreadDescriptionType>(
                    reinterpret_cast<QFunctionPointer>(
                        GetProcAddress(::GetModuleHandle(L"kernel32.dll"),
                                       "GetThreadDescription")))) {
            WCHAR *desc = nullptr;
            if (SUCCEEDED(GetThreadDescriptionFunc(GetCurrentThread(), &desc)))
                wcscpy_s(threadName, sizeof(threadName) / sizeof(*threadName), desc);
        }
    }

    logMsgF(logTo, "\n*** process %S (%lu) crashed ***", title, pid);
    logMsgF(logTo, "\n > why: %s", why);
    logMsgF(logTo, "\n > where: %S thread, TID: %lu", threadName, tid);

    if (printBacktrace && context) {
#  if defined(AM_USE_STACKWALKER)
        logMsg(logTo, "\n > C++ backtrace:");
        AMStackWalker sw(logTo, stackFramesToIgnore);
        sw.ShowCallstack(GetCurrentThread(), context);
#  else
        logMsg(logTo, "\n > C++ backtrace: cannot be generated"
#    if defined(Q_CC_MSVC)
                      ", StackWalker is not enabled"
#    elif defined(Q_CC_MINGW)
                      ", MinGW is not supported"
#    endif
               );
        Q_UNUSED(context)
        Q_UNUSED(stackFramesToIgnore)
#  endif // defined(AM_USE_STACKWALKER)
    }
    logQmlBacktrace(logTo);
}


#  if defined(Q_CC_MSVC)
// From Dr. Dobbs: https://www.drdobbs.com/visual-c-exception-handling-instrumentat/184416600
// These types are known to the compiler without declaration.

// This had been adapted for 64bit and 32bit platforms
// all pointers are not real pointers on 64bit Windows, but 32bit offsets relative to HINSTANCE!

struct _PMD
{
    qint32 mdisp;
    qint32 pdisp;
    qint32 vdisp;
};

// information of a type that can be caught by a catch block
struct _s__CatchableType
{
    quint32 properties;    // type properties bit flags: 1 - type is a pointer
    quint32 /* type info * */ pType;
    _PMD thisDisplacement; // displacement of this from the beginning of the object
    qint32 sizeOrOffset;   // size of the type
    quint32 /* void (*)() */ copyFunction; // != 0 if the type has a copy constructor
};

// a counted array of the types that can be caught by a catch block
struct _s__CatchableTypeArray
{
    qint32 nCatchableTypes;
#  pragma warning(push)
#  pragma warning(disable: 4200)
    quint32 /* const _s__CatchableType * */ arrayOfCatchableTypes[];
#  pragma warning(pop)

};

// this structure holds the information about the exception object being thrown
struct _s__ThrowInfo
{
    quint32 attributes; // 1: ptr to const obj / 2: ptr to volatile obj
    quint32 /* void * */ pmfnUnwind; // exception object destructor
    quint32 /* void * */ pForwardCompat;
    // array of sub-types of the object or other types to which it could be implicitly cast.
    // Used to find the matching catch block
    const quint32 /* const _s__CatchableTypeArray * */ pCatchableTypeArray;
};
#  endif // defined(Q_CC_MSVC)

#  define EXCEPTION_CPP_EXCEPTION    0xE06D7363  // internal MSVC value
#  define EXCEPTION_MINGW_EXCEPTION  0xE014e9aa  // custom AM value

static LONG WINAPI windowsExceptionFilter(EXCEPTION_POINTERS *ep)
{
    static char buffer [1024];
    bool suppressBacktrace = false;
    int stackFramesToIgnore = 0;

    switch (ep->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION        : strcpy(buffer, "access violation"); break;
    case EXCEPTION_DATATYPE_MISALIGNMENT   : strcpy(buffer, "datatype misalignment"); break;
    case EXCEPTION_BREAKPOINT              : strcpy(buffer, "breakpoint"); break;
    case EXCEPTION_SINGLE_STEP             : strcpy(buffer, "single step"); break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED   : strcpy(buffer, "array bounds exceeded"); break;
    case EXCEPTION_FLT_DENORMAL_OPERAND    : strcpy(buffer, "denormal float operand"); break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO      : strcpy(buffer, "float divide-by-zero"); break;
    case EXCEPTION_FLT_INEXACT_RESULT      : strcpy(buffer, "inexact float result"); break;
    case EXCEPTION_FLT_INVALID_OPERATION   : strcpy(buffer, "invalid float operation"); break;
    case EXCEPTION_FLT_OVERFLOW            : strcpy(buffer, "float overflow"); break;
    case EXCEPTION_FLT_STACK_CHECK         : strcpy(buffer, "float state check"); break;
    case EXCEPTION_FLT_UNDERFLOW           : strcpy(buffer, "float underflow"); break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO      : strcpy(buffer, "integer divide-by-zero"); break;
    case EXCEPTION_INT_OVERFLOW            : strcpy(buffer, "integer overflow"); break;
    case EXCEPTION_PRIV_INSTRUCTION        : strcpy(buffer, "private instruction"); break;
    case EXCEPTION_IN_PAGE_ERROR           : strcpy(buffer, "in-page error"); break;
    case EXCEPTION_ILLEGAL_INSTRUCTION     : strcpy(buffer, "illegal instruction"); break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: strcpy(buffer, "noncontinuable exception"); break;
    case EXCEPTION_STACK_OVERFLOW          : strcpy(buffer, "stack overflow"); suppressBacktrace = true; break;
    case EXCEPTION_INVALID_DISPOSITION     : strcpy(buffer, "invalid disposition"); break;
    case EXCEPTION_GUARD_PAGE              : strcpy(buffer, "guard page"); break;
    case EXCEPTION_INVALID_HANDLE          : strcpy(buffer, "invalid handle"); break;
#  if defined(Q_CC_MINGW)
    case EXCEPTION_MINGW_EXCEPTION: {
        if ((ep->ExceptionRecord->NumberParameters == 1)
                && (ep->ExceptionRecord->ExceptionInformation[0])) {
            strcpy_s(buffer, sizeof(buffer),
                     reinterpret_cast<const char *>(ep->ExceptionRecord->ExceptionInformation[0]));
        }
        break;
    }
#  elif defined(Q_CC_MSVC)
    case EXCEPTION_CPP_EXCEPTION: {
        // MSVC bug: std::current_exception is null, even though the standard says it shouldn't
        // be in this situation (it even is null in a set_terminate() handler).
        // So we have to use the compiler internal structure and functions to (a) get the type-name
        // of the exception and to (b) re-throw it in order to print the what() of a std::exception
        // derived instance.

        // (a) get the type_info

        std::type_info *type = nullptr;
        bool is64bit = (sizeof(void *) == 8);
        if (ep->ExceptionRecord->NumberParameters == (is64bit ? 4 : 3)) {
            auto hInstance = (is64bit ? reinterpret_cast<char *>(ep->ExceptionRecord->ExceptionInformation[3])
                                      : nullptr);

            // since all "pointers" are 32bit values even on 64bit Windows, we have to add the
            // hInstance segment pointer to each of the 32bit pointers to get a real 64bit address
            if (!is64bit || hInstance) {
                const auto *ti = reinterpret_cast<_s__ThrowInfo *>(ep->ExceptionRecord->ExceptionInformation[2]);
                if (ti) {
                    auto cta = reinterpret_cast<_s__CatchableTypeArray *>(hInstance + ti->pCatchableTypeArray);
                    if (cta && (cta->nCatchableTypes > 0) && (cta->nCatchableTypes < 100)) {
                        auto ct = reinterpret_cast<_s__CatchableType *>(hInstance + cta->arrayOfCatchableTypes[0]);
                        if (ct)
                            type = reinterpret_cast<std::type_info *>(hInstance + ct->pType);
                   }
                }
            }
        }
        const char *typeName = type ? type->name() : "<unknown type>";

        // (b) re-throw and catch the exception
        try {
            RaiseException(EXCEPTION_CPP_EXCEPTION,
                           EXCEPTION_NONCONTINUABLE,
                           ep->ExceptionRecord->NumberParameters,
                           ep->ExceptionRecord->ExceptionInformation);
        } catch (const std::exception &exc) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type %s (%s)", typeName, exc.what());
        } catch (const std::exception *exc) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type %s (%s)", typeName, exc->what());
        } catch (const char *exc) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type 'const char *' (%s)", exc);
        } catch (...) {
            snprintf(buffer, sizeof(buffer), "uncaught exception of type %s", typeName);
        }

        stackFramesToIgnore = 2; // CxxThrowException + RaiseException
        break;
    }
#  endif // defined(Q_CC_MSVC)
    default:
        snprintf(buffer, sizeof(buffer), "unknown Windows exception, code: %lx",
                 ep->ExceptionRecord->ExceptionCode);
        break;
    }

    logCrashInfo(Console, buffer, stackFramesToIgnore,
                 suppressBacktrace ? nullptr : ep->ContextRecord);

    if (Logging::deferredMessages()) {
        logMsg(Console, "\n > Accumulated logging output\n");
        Logging::completeSetup();
    }

    TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void initBacktraceWindows()
{
    // create a "process group", so that child process are automatically killed if this process dies
    HANDLE hJob = CreateJobObject(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    memset(&limits, 0, sizeof(limits));
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    AssignProcessToJobObject(hJob, GetCurrentProcess());

    // handle Windows' Structured Exceptions
    SetUnhandledExceptionFilter(windowsExceptionFilter);

#if defined(Q_CC_MINGW)
    // MinGW does handle the exceptions like gcc does on Unix, instead of using a structured
    // Windows exception, so we have to adapt:

    std::set_terminate([]() {
        char buffer [1024];

        auto type = abi::__cxa_current_exception_type();
        if (!type) {
            strcpy(buffer, "terminate was called although no exception was thrown");
        } else {
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
            } catch (const std::exception *exc) {
                snprintf(buffer, sizeof(buffer), "uncaught exception of type %s (%s)", typeName, exc->what());
            } catch (const char *exc) {
                snprintf(buffer, sizeof(buffer), "uncaught exception of type 'const char *' (%s)", exc);
            } catch (...) {
                snprintf(buffer, sizeof(buffer), "uncaught exception of type %s", typeName);
            }
        }
        ULONG_PTR args[1] = { reinterpret_cast<ULONG_PTR>(buffer) };
        RaiseException(EXCEPTION_MINGW_EXCEPTION, EXCEPTION_NONCONTINUABLE, 1, args);
    });
#  endif // defined(Q_CC_MINGW)
}

QT_END_NAMESPACE_AM

#endif // defined(Q_OS_WINDOWS)

