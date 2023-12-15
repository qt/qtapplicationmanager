// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <cinttypes>
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
#include "console.h"
#include "colorprint.h"
#include "qtappman_common-config_p.h"

#if defined(Q_OS_UNIX)
#  include<unistd.h>
#  if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#    define AM_PTHREAD_T_FMT "%p"
#  elif defined(Q_OS_QNX)
#    define AM_PTHREAD_T_FMT "%x"
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


struct CrashHandlerGlobal
{
    bool consoleInitialized = false; // dummy value to force instantiation in initBacktrace()
    bool printBacktrace = true;
    bool printQmlStack = true;
    bool dumpCore = true;
    int waitForGdbAttach = 0;
    int stackFramesToIgnoreOnCrash = // Make the backtrace start where the signal interrupted the normal program flow
#if defined(Q_OS_MACOS)
        12;  // XCode 14
#else
        9; // gcc/libstdc++ 13.2
#endif
    int stackFramesToIgnoreOnException = // Make the backtrace start at std::terminate
#if defined(Q_OS_MACOS)
        6;  // XCode 14
#else
        5; // gcc/libstdc++ 13.2
#endif

    char *demangleBuffer;
    size_t demangleBufferSize = 768;

    QByteArray backtraceLineOut;

    QPointer<QQmlEngine> qmlEngine;

    CrashHandlerGlobal()
    {
        demangleBuffer = static_cast<char *>(malloc(demangleBufferSize));
        backtraceLineOut.reserve(2 * int(demangleBufferSize));
    }
};

Q_GLOBAL_STATIC(CrashHandlerGlobal, chg);


void CrashHandler::setCrashActionConfiguration(const QVariantMap &config)
{
    chg()->printBacktrace = config.value(qSL("printBacktrace"), chg()->printBacktrace).toBool();
    chg()->printQmlStack = config.value(qSL("printQmlStack"), chg()->printQmlStack).toBool();
    chg()->waitForGdbAttach = config.value(qSL("waitForGdbAttach"), chg()->waitForGdbAttach).toInt() * timeoutFactor();
    chg()->dumpCore = config.value(qSL("dumpCore"), chg()->dumpCore).toBool();
    QVariantMap stackFramesToIgnore = config.value(qSL("stackFramesToIgnore")).toMap();
    chg()->stackFramesToIgnoreOnCrash = stackFramesToIgnore.value(qSL("onCrash"), chg()->stackFramesToIgnoreOnCrash).toInt();
    chg()->stackFramesToIgnoreOnException = stackFramesToIgnore.value(qSL("onException"), chg()->stackFramesToIgnoreOnException).toInt();
}

void CrashHandler::setQmlEngine(QQmlEngine *engine)
{
#if defined(QT_QML_LIB)
    chg()->qmlEngine = engine;
#else
    Q_UNUSED(engine)
#endif
}

#if defined(Q_OS_WINDOWS) || (defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID))

#  if defined(Q_OS_UNIX) || (defined(Q_OS_WINDOWS) && defined(Q_CC_MINGW))
// this will make it run before all other static constructor functions
static void initBacktrace() __attribute__((constructor(101)));


#  elif defined(Q_OS_WINDOWS) && defined(Q_CC_MSVC)
// this will make it run before all other static constructor functions
#    pragma warning(push)
#    pragma warning(disable: 4074)
#    pragma init_seg(compiler)

static void initBacktrace();

static struct InitBacktrace
{
    InitBacktrace() { initBacktrace(); }
} dummy;
#    pragma warning(pop)
#  endif

#  if defined(Q_OS_UNIX)
static void initBacktraceUnix();
#  else
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

    if (qEnvironmentVariableIntValue("AM_NO_CRASH_HANDLER") == 1)
        return;

    chg()->consoleInitialized = Console::ensureInitialized(); // enforce instantiation of both

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
        dummy += write(STDERR_FILENO, "\n", 1);
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
    Q_UNUSED(offset) // not really helpful
#if defined(Q_OS_MACOS)
    // skip those unhelpful "executable file is not an executable" messages
    if (!level && !symbol && !offset && !file && !errorCode)
        return;
#endif

    if (level > 999)
        return;

    bool wantClickableUrl = (Console::isRunningInQtCreator() && !Console::stderrIsConsoleWindow());

    QByteArray &lineOut = chg()->backtraceLineOut;
    lineOut.resize(0);

    // Only use color when logging to a console that supports it and when we do not want
    // clickable URLs (QtCreator is not parsing lines containing ANSI codes)
    ColorPrint cprt(lineOut, (logTo == Console) && Console::stderrSupportsAnsiColor() && !wantClickableUrl);

    char levelStr[5];
    qsnprintf(levelStr, sizeof(levelStr), "%4d", level);
    cprt << levelStr << ": ";

    if (errorString)
        cprt << ColorPrint::bred << "ERROR: " << ColorPrint::reset << errorString << " (" << errorCode << ')';
    else
        cprt << ColorPrint::bwhite << (symbol ? symbol : "?") << ColorPrint::reset;

    if (file) {
        static const char *filePrefix =
#  if defined(Q_OS_WINDOWS)
                "file:///";
#  else
                "file://";
#  endif
        bool isFileUrl = (strncmp(file, filePrefix, sizeof(filePrefix) - 1) == 0);

        cprt << " in " << ColorPrint::magenta;
        if (!isFileUrl && wantClickableUrl)   // we need a file:// prefix, but don't have one
            cprt << filePrefix;
        if (isFileUrl && !wantClickableUrl)   // we have a file:// prefix, but don't want one
            cprt << (file + sizeof(filePrefix) - 1);
        else
            cprt << file;
        cprt << ColorPrint::reset;

        bool lineValid = (line > 0 && line <= 9999999);
        if (lineValid || wantClickableUrl)
            cprt << ':' << ColorPrint::bmagenta << (lineValid ? line : 0) << ColorPrint::reset;
    }

    logMsg(logTo, lineOut.constData());
}

static void logQmlBacktrace(LogToDestination logTo)
{
#  if defined(QT_QML_LIB)
    if (chg()->printQmlStack && chg()->qmlEngine) {
        const QV4::ExecutionEngine *qv4engine = chg()->qmlEngine->handle();
        if (qv4engine) {
            // Both stackTrace() and toLocal8Bit() are allocating memory. We can't do anything about
            // stackTrace(), so replacing toLocal8Bit with QStringEncoder and stack based char
            // buffers doesn't really help.

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

#  if defined(Q_OS_QNX)
#    include <process.h>
#    include <backtrace.h>
#    include <QtCore/private/qcore_unix_p.h>
#  else
#    include <execinfo.h>
#    include <sys/syscall.h>
#  endif
#  include <cxxabi.h>
#  include <csetjmp>
#  if defined(Q_OS_MACOS) && defined(setjmp)
#    undef setjmp // macOS defines this as a self-recursive macro in C++ mode
#  endif
#  include <csignal>
#  include <pthread.h>
#  include <cstdio>
#  include <cstdlib>

#  if QT_CONFIG(am_libbacktrace)
#    include <libbacktrace/backtrace.h>
#    include <libbacktrace/backtrace-supported.h>
#  endif
#  if defined(Q_OS_LINUX)
#    include <dlfcn.h>
#  elif defined(Q_OS_MACOS)
#    include <mach-o/dyld.h>
#  endif
#  if defined(QT_AM_COVERAGE)
extern "C" {
#    include <gcov.h>
}
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
        crashHandler(buffer, chg()->stackFramesToIgnoreOnCrash);
    });

    std::set_terminate([]() {
        char buffer [1024];

        auto type = abi::__cxa_current_exception_type();
        if (!type)
            crashHandler("terminate was called although no exception was thrown", 0);

        const char *typeName = type->name();
        if (typeName) {
            int status = 1;
            chg()->demangleBuffer = abi::__cxa_demangle(typeName, chg()->demangleBuffer,
                                                        &chg()->demangleBufferSize, &status);
            if (status == 0 && *chg()->demangleBuffer) {
                typeName = chg()->demangleBuffer;
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

        crashHandler(buffer, chg()->stackFramesToIgnoreOnException);
    });

    // create a new process group, so that we are able to kill all children with ::kill(0, ...)
    setpgid(0, 0);
}

static void logCrashInfo(LogToDestination logTo, const char *why, int stackFramesToIgnore)
{
    const char *title = ProcessTitle::title();
    char who[256];
    if (!title) {
#if !defined(Q_OS_QNX)
        ssize_t whoLen = readlink("/proc/self/exe", who, sizeof(who) - 1);
#else
        int fd = qt_safe_open("/proc/self/exefile", O_RDONLY);
        ssize_t whoLen = 0;
        if (fd != -1) {
            whoLen = qt_safe_read(fd, who, sizeof(who) - 1);
            close(fd);
        }
#endif
        who[std::max(ssize_t(0), whoLen)] = '\0';
        title = who;
    }

    pid_t pid = getpid();
#if defined(Q_OS_LINUX)
    long tid = syscall(SYS_gettid);
    bool isMainThread = (tid == pid);
#elif defined(Q_OS_QNX)
    long tid = gettid();
    bool isMainThread = (tid == 1);
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

    if (chg()->printBacktrace) {
        if (!QLibraryInfo::isDebugBuild()) {
            logMsgF(logTo, "\n > Your binary is built in release mode. The following backtrace might be inaccurate.");
            logMsgF(logTo, " > Please consider using a debug build for a more accurate backtrace.");
#  if defined(Q_OS_MACOS)
            logMsgF(logTo, " > E.g. by running the binary using DYLD_IMAGE_SUFFIX=_debug");
#    if defined(Q_PROCESSOR_ARM_64)
            logMsgF(logTo, " > Backtraces on macOS/arm64 may not be available.");
#    endif
#  endif
        }

#  if QT_CONFIG(am_libbacktrace) && defined(BACKTRACE_SUPPORTED)
        struct btData
        {
            LogToDestination logTo;
            int level;
            backtrace_state *state;
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
                int status = 1;
                chg()->demangleBuffer = abi::__cxa_demangle(symname, chg()->demangleBuffer,
                                                            &chg()->demangleBufferSize, &status);
                name = (status == 0 && *chg()->demangleBuffer) ? chg()->demangleBuffer : symname;
            }
            logBacktraceLine(btdata->logTo, btdata->level, name, pc);
        };

        static auto fullCallback = [](void *data, uintptr_t pc, const char *filename, int lineno,
                                      const char *function) -> int {
            auto btdata = static_cast<btData *>(data);

            if (function) {
                int status = 1;
                chg()->demangleBuffer = abi::__cxa_demangle(function, chg()->demangleBuffer,
                                                            &chg()->demangleBufferSize, &status);

                logBacktraceLine(btdata->logTo, btdata->level,
                                 (status == 0 && *chg()->demangleBuffer) ? chg()->demangleBuffer
                                                                         : function,
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
        btData data = { logTo, 0, state };
        //backtrace_print(state, stackFramesToIgnore, stderr);
        backtrace_simple(state, stackFramesToIgnore, simpleCallback, errorCallback, &data);

#  else // QT_CONFIG(am_libbacktrace) && defined(BACKTRACE_SUPPORTED)
#    if !defined(Q_OS_QNX)
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
                for (int i = 1 + stackFramesToIgnore; i < addrCount; ++i) {
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
                        chg()->demangleBuffer = abi::__cxa_demangle(function, chg()->demangleBuffer,
                                                                    &chg()->demangleBufferSize, &status);
                        name = (status == 0 && *chg()->demangleBuffer) ? chg()->demangleBuffer
                                                                       : function;
                    } else {
                        name = symbols[i];
                        if (function && (function == offset))
                            *(function - 1) = 0;
                    }
                    logBacktraceLine(logTo, i - 1 - stackFramesToIgnore, name,
                                     offset ? strtoull(offset + 1, nullptr, 16) : 0);
                }
            }
        }
#    else
        Q_UNUSED(stackFramesToIgnore);
        constexpr int frames = 20;
        bt_addr_t addrs[frames];
        bt_memmap_t memmap;
        char out[1024];
        char fmt[] = "%a: %f (%o) + %l";

        bt_get_backtrace(&bt_acc_self, addrs, frames);
        bt_load_memmap(&bt_acc_self, &memmap);
        bt_sprnf_addrs(&memmap, addrs, frames, fmt, out, sizeof(out), nullptr);

        logMsg(logTo, "\n > C++ backtrace:");
        logMsg(logTo, out);
#    endif
#  endif // QT_CONFIG(am_libbacktrace) && defined(BACKTRACE_SUPPORTED)
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

    if (chg()->waitForGdbAttach > 0) {
        logMsgF(Console, "\n > the process will be suspended for %d seconds and you can attach a debugger"
                         " to it via\n\n   gdb -p %d\n", chg()->waitForGdbAttach, getpid());
        static jmp_buf jmpenv;
        signal(SIGALRM, [](int) {
            longjmp(jmpenv, 1);
        });
        if (!setjmp(jmpenv)) {
            alarm(static_cast<unsigned int>(chg()->waitForGdbAttach));

            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGALRM);
            sigsuspend(&mask);
        } else {
            logMsg(Console, "\n > no gdb attached\n");
        }
    }

    if (Logging::hasDeferredMessages()) {
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

#if defined(QT_AM_COVERAGE)
    __gcov_dump();
#endif

    if (chg()->dumpCore) {
        logMsg(Console, "\n > the process will be aborted (core dumped)\n");
        abort();
    }

    _exit(-1);
}

QT_END_NAMESPACE_AM

#elif defined(Q_OS_WINDOWS)

#  include <windows.h>
#  include <dbghelp.h>
#  if QT_CONFIG(am_stackwalker)
#    include <stackwalker.h>
#  endif

QT_BEGIN_NAMESPACE_AM

static DWORD mainThreadId = GetCurrentThreadId();

#  if QT_CONFIG(am_stackwalker)

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

#  endif // QT_CONFIG(am_stackwalker)

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

    if (chg()->printBacktrace && context) {
#  if QT_CONFIG(am_stackwalker)
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
#  endif // QT_CONFIG(am_stackwalker)
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

    if (Logging::hasDeferredMessages()) {
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
                chg()->demangleBuffer = abi::__cxa_demangle(typeName, chg()->demangleBuffer,
                                                            &chg()->demangleBufferSize, &status);
                if (status == 0 && *chg()->demangleBuffer) {
                    typeName = chg()->demangleBuffer;
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

