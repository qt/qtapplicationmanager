// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "unixsignalhandler.h"
#include "logging.h"

#include <QSocketNotifier>
#include <QCoreApplication>

#include <errno.h>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <synchapi.h>
#else
#  include <sys/mman.h>
#endif


QT_BEGIN_NAMESPACE_AM

// sigmask() is not available on Windows
UnixSignalHandler::am_sigmask_t UnixSignalHandler::am_sigmask(int sig)
{
    return ((am_sigmask_t(1)) << (((sig) - 1) % int(8 * sizeof(am_sigmask_t))));
}

UnixSignalHandler *UnixSignalHandler::s_instance = nullptr;

UnixSignalHandler::UnixSignalHandler()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_QNX)
    // Setup alternate signal stack (to get backtrace for stack overflow)
    // Canonical size might not be suffcient to get QML backtrace, so we double it
    size_t stackSize = SIGSTKSZ * 2;
    stack_t sigstack;
    // ASAN and valgrind would report malloc() as a leak. In addition, we avoid the
    // signal stack being close to a possibly corrupted heap this way.
    sigstack.ss_sp = mmap(nullptr, stackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (sigstack.ss_sp != MAP_FAILED) {
        sigstack.ss_size = stackSize;
        sigstack.ss_flags = 0;
        sigaltstack(&sigstack, nullptr);
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
#if  __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 32
    return sigdescr_np(sig);
#else
    return strsignal(sig); // not async-signal-safe
#endif
#else
    Q_UNUSED(sig)
    return "<unknown>";
#endif
}

void UnixSignalHandler::resetToDefault(int sig)
{
    auto sigs = { sig };
    resetToDefault(sigs);
}

void UnixSignalHandler::resetToDefault(const std::initializer_list<int> &sigs)
{
    for (int sig : sigs) {
        m_resetSignalMask |= am_sigmask(sig);
        signal(sig, SIG_DFL);
    }
}

bool UnixSignalHandler::install(Type handlerType, int sig, const std::function<void (int)> &handler)
{
    auto sigs = { sig };
    return install(handlerType, sigs, handler);
}

bool UnixSignalHandler::install(Type handlerType, const std::initializer_list<int> &sigs,
                                const std::function<void(int)> &handler)
{
    auto sigHandler = [](int sig) {
        // this lambda is the low-level signal handler multiplexer
        auto that = UnixSignalHandler::instance();
        that->m_currentSignal = sig;

        for (const auto &h : std::as_const(that->m_handlers)) {
            if ((h.m_signal == sig) && !h.m_disabled) {
                if (!h.m_qt) {
                    h.m_handler(sig);
                } else {
#if defined(Q_OS_UNIX)
                    auto dummy = write(that->m_pipe[1], &sig, sizeof(int));
                    Q_UNUSED(dummy)
#elif defined(Q_OS_WIN)
                    // we're running in a separate thread now
                    that->m_winLock.lock();
                    that->m_signalsForEventLoop << sig;
                    that->m_winLock.unlock();
                    PulseEvent(that->m_winEvent->handle());
#endif
                }
            }
        }
        if (that->m_resetSignalMask) {
            // Someone called resetToDefault - now's a good time to handle it.
            // We can not remove the entries in the list, because that would (a) allocate and (b)
            // step on code that might be iterating over the list in the "Forwarded" handler.

            for (const auto &h : std::as_const(that->m_handlers)) {
                if (that->m_resetSignalMask & am_sigmask(h.m_signal))
                    h.m_disabled = true;
            }
            that->m_resetSignalMask = 0;
        }
        that->m_currentSignal = 0;
    };

    if (m_currentSignal) {
        // installing a signal handler from within a signal handler shouldn't be necessary
        return false;
    }

    if (handlerType == ForwardedToEventLoopHandler) {
#if defined(Q_OS_UNIX)
        if ((m_pipe[0] == -1) && qApp) {
            auto dummy = pipe(m_pipe);
            Q_UNUSED(dummy)

            auto sn = new QSocketNotifier(m_pipe[0], QSocketNotifier::Read, this);
            connect(sn, &QSocketNotifier::activated, qApp, [this]() {
                // this lambda is the "signal handler" multiplexer within the Qt event loop
                int sig = 0;

                if (read(m_pipe[0], &sig, sizeof(int)) != sizeof(int)) {
                    qCWarning(LogSystem) << "Error reading from signal handler:" << strerror(errno);;
                    return;
                }

                for (const auto &h : std::as_const(m_handlers)) {
                    if (h.m_qt && (h.m_signal == sig) && !h.m_disabled)
                        h.m_handler(sig);
                }
            });
        }
#elif defined(Q_OS_WIN)
        if (!m_winEvent) {
            m_winEvent = new QWinEventNotifier(CreateEventW(nullptr, false, false, nullptr), this);

            connect(m_winEvent, &QWinEventNotifier::activated, qApp, [this]() {
                // this lambda is the "signal handler" multiplexer within the Qt event loop
                m_winLock.lock();
                for (const int &sig : std::as_const(m_signalsForEventLoop)) {
                    for (const auto &h : std::as_const(m_handlers)) {
                        if (h.m_qt && (h.m_signal == sig) && !h.m_disabled)
                            h.m_handler(sig);
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

    // This is UB! We cannot guarantee that the signal handler is not currently executing and
    // iterating over this list. In practice, this is a none-issue in the AM however, because all
    // install() calls are done right at startup time.
    // To do it right, we would need a lock-free list structure for m_handlers.
    for (int sig : sigs)
        m_handlers.emplace_back(sig, handlerType == ForwardedToEventLoopHandler, handler);

#if defined(Q_OS_UNIX) && !defined(Q_OS_QNX)
    struct sigaction sigact;
    sigact.sa_flags = SA_ONSTACK;
    sigact.sa_handler = sigHandler;

    sigemptyset(&sigact.sa_mask);
    sigset_t unblockSet;
    sigemptyset(&unblockSet);

    for (int sig : sigs) {
        sigaddset(&unblockSet, sig);
        sigaction(sig, &sigact, nullptr);
        m_resetSignalMask &= ~am_sigmask(sig); // cancel unfinished reset
    }
    sigprocmask(SIG_UNBLOCK, &unblockSet, nullptr);
#else
    for (int sig : sigs)
        signal(sig, sigHandler);
#endif
    return true;
}

UnixSignalHandler::SigHandler::SigHandler(int signal, bool qt, const std::function<void (int)> &handler)
    : m_signal(signal), m_qt(qt), m_handler(handler)
{ }


QT_END_NAMESPACE_AM

#include "moc_unixsignalhandler.cpp"
