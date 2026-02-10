// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef UNIXSIGNALHANDLER_H
#define UNIXSIGNALHANDLER_H

#include <QtAppManCommon/qtappmancommonglobal.h>
#include <QtCore/QObject>
#include <QtCore/QAtomicInteger>

#include <initializer_list>
#include <vector>
#include <functional>
#include <csignal>

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#elif defined(Q_OS_WIN)
#  include <QtCore/QMutex>
#  include <QtCore/QWinEventNotifier>
#endif

QT_BEGIN_NAMESPACE_AM

class Q_APPMANCOMMON_EXPORT UnixSignalHandler : public QObject
{
    Q_OBJECT

public:
    static UnixSignalHandler *instance();

    static const char *signalName(int sig);

    static int watchdogSignal();

    bool resetToDefault(int sig);
    bool resetToDefault(std::initializer_list<int> sigs);

    std::vector<int> reinstallIfNeeded(std::initializer_list<int> sigs);

    enum Type {
        RawSignalHandler,
        ForwardedToEventLoopHandler
    };

    bool install(Type handlerType, int sig, const std::function<void(int, int)> &handler);
    bool install(Type handlerType, std::initializer_list<int> sigs,
                 const std::function<void(int, int)> &handler);

    static constexpr bool isValidSignal(int sig) { return ((sig > 0) && (sig < NSIG)); }

private:
    UnixSignalHandler();
    static UnixSignalHandler *s_instance;

#if defined(Q_OS_UNIX) && !defined(Q_OS_QNX)
    static void signalHandler(int sig, siginfo_t *info, void *ucontext);
#else
    static void signalHandler(int sig);
#endif

    struct SigHandler
    {
        explicit SigHandler(bool eventLoop, const std::function<void(int, int)> &handler)
            : m_eventLoop(eventLoop), m_handler(handler) { }

        bool m_eventLoop;
        std::function<void(int, int)> m_handler;
    };

    void deleteAfterGracePeriod(SigHandler *sh);

    QAtomicInteger<int> m_currentSignal = 0;
    // NSIG = number of signals defined + 1 (also: signals are 1-based, so access is [sig-1])
    std::array<QAtomicPointer<SigHandler>, NSIG - 1> m_handlers;

#if defined(Q_OS_UNIX)
    std::array<int, 2> m_pipe = { -1, -1 };
#elif defined(Q_OS_WIN)
    QMutex m_winLock;
    QWinEventNotifier *m_winEvent = nullptr;
    QList<int> m_signalsForEventLoop;
#endif
};

QT_END_NAMESPACE_AM

#endif // UNIXSIGNALHANDLER_H
