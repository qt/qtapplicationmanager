// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QtCore/QObject>
#include <QtCore/QAtomicInteger>

#include <initializer_list>
#include <list>
#include <vector>
#include <functional>
#include <signal.h>

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#elif defined(Q_OS_WIN)
#  include <QtCore/QMutex>
#  include <QtCore/QWinEventNotifier>
#endif

QT_BEGIN_NAMESPACE_AM

class UnixSignalHandler : public QObject
{
    Q_OBJECT

public:
    static UnixSignalHandler *instance();

    static const char *signalName(int sig);

    void resetToDefault(int sig);
    void resetToDefault(const std::initializer_list<int> &sigs);

    enum Type {
        RawSignalHandler,
        ForwardedToEventLoopHandler
    };

    bool install(Type handlerType, int sig, const std::function<void(int)> &handler);
    bool install(Type handlerType, const std::initializer_list<int> &sigs,
                 const std::function<void(int)> &handler);

private:
    UnixSignalHandler();
    static UnixSignalHandler *s_instance;

    struct SigHandler
    {
        SigHandler(int signal, bool qt, const std::function<void(int)> &handler);

        int m_signal;
        bool m_qt;
        std::function<void(int)> m_handler;
        mutable QAtomicInteger<bool> m_disabled;
    };

    std::list<SigHandler> m_handlers; // we're using STL to avoid (accidental) implicit copies
    int m_currentSignal = 0;

    // 64 bits are currently enough to map all Linux signals
    using am_sigmask_t = quint64;

    static am_sigmask_t am_sigmask(int sig);
    QAtomicInteger<am_sigmask_t> m_resetSignalMask;
#if defined(Q_OS_UNIX)
    int m_pipe[2] = { -1, -1 };
#elif defined(Q_OS_WIN)
    QMutex m_winLock;
    QWinEventNotifier *m_winEvent = nullptr;
    QList<int> m_signalsForEventLoop;
#endif
};

QT_END_NAMESPACE_AM
