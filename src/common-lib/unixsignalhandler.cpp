// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "unixsignalhandler.h"
#include "logging.h"

#include <QSocketNotifier>
#include <QCoreApplication>
#include <QTimer>

#include <cerrno>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <synchapi.h>
#else
#  include <QtCore/private/qcore_unix_p.h>
#  include <sys/mman.h>
#  if defined(Q_OS_DARWIN) // we want the real POSIX functions, not macros
#    undef sigemptyset
#    undef sigaddset
#  endif
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_QNX)
#  define AM_POSIX_SIGNALS
#endif

using namespace std::chrono_literals;

QT_BEGIN_NAMESPACE_AM

UnixSignalHandler *UnixSignalHandler::s_instance = nullptr;

UnixSignalHandler::UnixSignalHandler()
{
#if defined(AM_POSIX_SIGNALS)
    // Please note: sigaltstack is a per-thread setting only.
    // Setup alternate signal stack (to get backtrace for stack overflow)
    // Canonical size might not be suffcient to get QML backtrace, so we double it
    size_t stackSize = size_t(SIGSTKSZ) * 2 + MINSIGSTKSZ;
    stack_t sigstack;
    // ASAN and valgrind would report malloc() as a leak. In addition, we avoid the
    // signal stack being close to a possibly corrupted heap this way.
    sigstack.ss_sp = ::mmap(nullptr, stackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (sigstack.ss_sp != MAP_FAILED) {
        sigstack.ss_size = stackSize;
        sigstack.ss_flags = 0;
        ::sigaltstack(&sigstack, nullptr);
    } else {
        // this code runs before all other static constructors
        qWarning("WARNING: UnixSignalHandler failed to allocate memory for an alternate signal stack.");
    }
#endif
}

UnixSignalHandler *UnixSignalHandler::instance()
{
    if (Q_UNLIKELY(!s_instance))
        s_instance = new UnixSignalHandler();
    return s_instance;
}

const char *UnixSignalHandler::signalName(int sig)
{
#if defined(Q_OS_UNIX)
#  if  defined(__GLIBC__) && (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 32)
    return ::sigdescr_np(sig);
#  else
    return ::strsignal(sig); // not async-signal-safe
#  endif
#else
    Q_UNUSED(sig)
    return "<unknown>";
#endif
}

int UnixSignalHandler::watchdogSignal()
{
#if defined(SIGSTKFLT) // available and unused on Linux x86 and arm
    return SIGSTKFLT;
#elif defined(SIGEMT)  // available and unused on Linux mips, BSD and QNX
    return SIGEMT;
#elif defined(SIGUSR1) // Unix safe fallback
    return SIGUSR1;
#else                  // Windows
    return 0;
#endif
}

bool UnixSignalHandler::resetToDefault(int sig)
{
    return resetToDefault({ sig });
}

bool UnixSignalHandler::resetToDefault(std::initializer_list<int> sigs)
{
    bool result = true;
    for (int sig : sigs) {
        if (!isValidSignal(sig)) {
            result = false;
            continue;
        }
        if (auto sh = m_handlers[sig - 1].fetchAndStoreRelease(nullptr))
            deleteAfterGracePeriod(sh);
        ::signal(sig, SIG_DFL);
    }
    return result;
}

std::vector<int> UnixSignalHandler::reinstallIfNeeded(std::initializer_list<int> sigs)
{
    // Make sure our handler is still active and not blocked
    // Returns the list of signals that had to be reinstalled

    std::vector<int> result;

#if defined(AM_POSIX_SIGNALS)
    struct ::sigaction sigact;
    ::sigset_t unblockSet;
    ::sigemptyset(&unblockSet);
#endif

    for (int sig : sigs) {
        if (!isValidSignal(sig))
            continue;

        if (m_handlers[sig - 1].loadAcquire()) {
#if defined(AM_POSIX_SIGNALS)
            ::sigaddset(&unblockSet, sig);

            if ((::sigaction(sig, nullptr, &sigact) < 0) || (sigact.sa_sigaction != &signalHandler)) {
                sigact.sa_flags = SA_ONSTACK | SA_SIGINFO;
                sigact.sa_sigaction = &signalHandler;
                ::sigemptyset(&sigact.sa_mask);
                ::sigaction(sig, &sigact, nullptr);
                result.push_back(sig);
            }
#else
            if (::signal(sig, &signalHandler) != &signalHandler)
                result.push_back(sig);
#endif
        }
    }

#if defined(AM_POSIX_SIGNALS)
    ::sigprocmask(SIG_UNBLOCK, &unblockSet, nullptr);
#endif
    return result;
}

#if defined(AM_POSIX_SIGNALS)
void UnixSignalHandler::signalHandler(int sig, siginfo_t *info, void *)
{
    int senderPid = 0;
    if (info && (info->si_code == SI_USER || info->si_code == SI_QUEUE || info->si_code == SI_MESGQ))
        senderPid = info->si_pid;
#else
void UnixSignalHandler::signalHandler(int sig)
{
    int senderPid = 0;
#endif
    // this function is the low-level signal handler multiplexer
    auto that = UnixSignalHandler::instance();
    if (!isValidSignal(sig))
        return;
    auto sh = that->m_handlers[sig - 1].loadAcquire();
    if (!sh)
        return;
    that->m_currentSignal = sig;
    auto clearCurrentSignal = qScopeGuard([=]() { that->m_currentSignal = 0; });

    if (!sh->m_eventLoop) {
        sh->m_handler(sig, senderPid);
    } else {
#if defined(Q_OS_UNIX)
        std::array<int, 2> pipeMsg = { sig, senderPid };
        const qint64 pipeMsgSize = pipeMsg.size() * sizeof(decltype(pipeMsg)::value_type);
        (void) qt_safe_write(that->m_pipe[1], pipeMsg.data(), pipeMsgSize);
#elif defined(Q_OS_WIN)
        // we're running in a separate thread now
        that->m_winLock.lock();
        that->m_signalsForEventLoop << sig;
        that->m_winLock.unlock();
        ::PulseEvent(that->m_winEvent->handle());
#endif
    }
}

void UnixSignalHandler::deleteAfterGracePeriod(SigHandler *sh)
{
    // There could be a signal handler executing right now, so we need to delay the deletion
    if (sh)
        QTimer::singleShot(10min, this, [sh] { delete sh; });
};

bool UnixSignalHandler::install(Type handlerType, int sig, const std::function<void (int, int)> &handler)
{
    return install(handlerType, { sig }, handler);
}

bool UnixSignalHandler::install(Type handlerType, std::initializer_list<int> sigs,
                                const std::function<void(int, int)> &handler)
{
    for (int sig : sigs) {
        if (!isValidSignal(sig)) {
            qCWarning(LogSystem) << "Unix signal number" << sig << "is invalid";
            return false;
        }
    }

    if (m_currentSignal) {
        // installing a signal handler from within a signal handler is not supported
        return false;
    }

    if (handlerType == ForwardedToEventLoopHandler) {
#if defined(Q_OS_UNIX)
        if ((m_pipe[0] == -1) && qApp) {
            auto dummy = qt_safe_pipe(m_pipe.data());
            Q_UNUSED(dummy)

            auto sn = new QSocketNotifier(m_pipe[0], QSocketNotifier::Read, this);
            connect(sn, &QSocketNotifier::activated, qApp, [this]() {
                // this lambda is the "signal handler" multiplexer within the Qt event loop
                std::array<int, 2> pipeMsg = { 0, 0 };
                const qint64 pipeMsgSize = pipeMsg.size() * sizeof(decltype(pipeMsg)::value_type);

                if (qt_safe_read(m_pipe[0], pipeMsg.data(), pipeMsgSize) != pipeMsgSize) {
                    qCWarning(LogSystem) << "Error reading from signal handler:" << strerror(errno);
                    return;
                }

                if (isValidSignal(pipeMsg[0])) {
                    if (auto sh = m_handlers[pipeMsg[0] - 1].loadAcquire()) {
                        if (sh->m_handler && sh->m_eventLoop)
                            sh->m_handler(pipeMsg[0], pipeMsg[1]);
                    }
                }
            });
        }
#elif defined(Q_OS_WIN)
        if (!m_winEvent) {
            m_winEvent = new QWinEventNotifier(CreateEventW(nullptr, false, false, nullptr), this);

            connect(m_winEvent, &QWinEventNotifier::activated, qApp, [this]() {
                // this lambda is the "signal handler" multiplexer within the Qt event loop
                m_winLock.lock();
                for (int sig : std::as_const(m_signalsForEventLoop)) {
                    if (isValidSignal(sig)) {
                        if (auto sh = m_handlers[sig - 1].loadAcquire()) {
                            if (sh->m_handler && sh->m_eventLoop)
                                sh->m_handler(sig, 0);
                        }
                    }
                }
                m_signalsForEventLoop.clear();
                m_winLock.unlock();
            });
        }
#else
        qCWarning(LogSystem) << "Unix signal handling via 'ForwardedToEventLoopHandler' is not "
                                "supported on this platform";
        return false;
#endif
    }

    for (int sig : sigs) {
        auto *sh = new SigHandler(handlerType == ForwardedToEventLoopHandler, handler);

        if (auto *shExisting = m_handlers[sig - 1].fetchAndStoreRelease(sh)) {
            qCDebug(LogSystem) << "Replacing existing signal handler for signal" << sig;
            deleteAfterGracePeriod(shExisting);
        }
    }

#if defined(AM_POSIX_SIGNALS)
    struct ::sigaction sigact;
    sigact.sa_flags = SA_ONSTACK | SA_SIGINFO;
    sigact.sa_sigaction = &signalHandler;

    ::sigemptyset(&sigact.sa_mask);
    ::sigset_t unblockSet;
    ::sigemptyset(&unblockSet);

    for (int sig : sigs) {
        ::sigaddset(&unblockSet, sig);
        ::sigaction(sig, &sigact, nullptr);
    }
    ::sigprocmask(SIG_UNBLOCK, &unblockSet, nullptr);
#else
    for (int sig : sigs)
        ::signal(sig, &signalHandler);
#endif
    return true;
}

QT_END_NAMESPACE_AM

#include "moc_unixsignalhandler.cpp"
