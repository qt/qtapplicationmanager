/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include "unixsignalhandler.h"
#include "logging.h"

#include <QSocketNotifier>
#include <QCoreApplication>

#include <errno.h>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <synchapi.h>
#endif


QT_BEGIN_NAMESPACE_AM

// sigmask() is not available on Windows
UnixSignalHandler::am_sigmask_t UnixSignalHandler::am_sigmask(int sig)
{
    return ((am_sigmask_t(1)) << (((sig) - 1) % int(8 * sizeof(am_sigmask_t))));
}

UnixSignalHandler *UnixSignalHandler::s_instance = nullptr;

#if defined(Q_OS_UNIX)
// make it clear in the valgrind backtrace that this is a deliberate leak
static void *malloc_valgrind_ignore(size_t size)
{
    return malloc(size);
}

UnixSignalHandler::UnixSignalHandler()
    : QObject()
    , m_pipe { -1, -1 }
{
    // Setup alternate signal stack (to get backtrace for stack overflow)
    stack_t sigstack;
    // valgrind will report this as leaked: nothing we can do about it
    sigstack.ss_sp = malloc_valgrind_ignore(SIGSTKSZ);
    sigstack.ss_size = SIGSTKSZ;
    sigstack.ss_flags = 0;
    sigaltstack(&sigstack, nullptr);
}
#else
UnixSignalHandler::UnixSignalHandler() : QObject()
{ }
#endif

UnixSignalHandler *UnixSignalHandler::instance()
{
    if (Q_UNLIKELY(!s_instance))
        s_instance = new UnixSignalHandler();
    return s_instance;
}

const char *UnixSignalHandler::signalName(int sig)
{
#if defined(Q_OS_UNIX)
    return sys_siglist[sig];
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

        for (const auto &h : that->m_handlers) {
            if (h.m_signal == sig) {
                if (!h.m_qt) {
                    h.m_handler(sig);
                } else {
#if defined(Q_OS_UNIX)
                    (void) write(that->m_pipe[1], &sig, sizeof(int));
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
            // someone called resetToDefault - now's a good time to handle it
            that->m_handlers.remove_if([that](const SigHandler &h) {
                return that->m_resetSignalMask & am_sigmask(h.m_signal);
            });
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
            (void) pipe(m_pipe);

            auto sn = new QSocketNotifier(m_pipe[0], QSocketNotifier::Read, this);
            connect(sn, &QSocketNotifier::activated, qApp, [this]() {
                // this lambda is the "signal handler" multiplexer within the Qt event loop
                int sig = 0;

                if (read(m_pipe[0], &sig, sizeof(int)) != sizeof(int)) {
                    qCWarning(LogSystem) << "Error reading from signal handler:" << strerror(errno);;
                    return;
                }

                for (const auto &h : UnixSignalHandler::instance()->m_handlers) {
                    if (h.m_qt && h.m_signal == sig)
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
                for (const int &sig : qAsConst(m_signalsForEventLoop)) {
                    for (const auto &h : UnixSignalHandler::instance()->m_handlers) {
                        if (h.m_qt && h.m_signal == sig)
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
        //TODO: use the private QWindowsPipe{Reader,Writer} classes in QtBase on Windows
        return false;
#endif
    }

    // STL'ish for append with construct in place
    for (int sig : sigs)
        m_handlers.emplace_back(sig, handlerType == ForwardedToEventLoopHandler, handler);

#if defined(Q_OS_UNIX)
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
