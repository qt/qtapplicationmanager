// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WATCHDOG_P_H
#define WATCHDOG_P_H

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtQuick/QQuickWindow>
#include <QtCore//QEventLoop>

#include "watchdog.h"

QT_FORWARD_DECLARE_CLASS(QThread)
QT_FORWARD_DECLARE_CLASS(QQuickWindow)

QT_BEGIN_NAMESPACE_AM

class WatchdogPrivate : public QObject
{
    Q_OBJECT

public:
    WatchdogPrivate(Watchdog *q);
    ~WatchdogPrivate() override;

    void setupSystemdWatchdog();

    void setEventLoopTimeouts(std::chrono::milliseconds check, std::chrono::milliseconds warn,
                           std::chrono::milliseconds kill);
    bool isEventLoopWatchingEnabled() const;
    void watchEventLoop(QThread *thread);
    void eventLoopCheck();


    void setQuickWindowTimeouts(std::chrono::milliseconds check,
                                std::chrono::milliseconds warnSync, std::chrono::milliseconds killSync,
                                std::chrono::milliseconds warnRender, std::chrono::milliseconds killRender,
                                std::chrono::milliseconds warnSwap, std::chrono::milliseconds killSwap);
    bool isQuickWindowWatchingEnabled() const;
    void watchQuickWindow(QQuickWindow *quickWindow);
    void quickWindowCheck();

    enum RenderState : unsigned int {
        Idle = 0,
        Sync,
        Render,
        Swap,
    };

    union AtomicState {
        quint64 pod = 0;
        struct {
            quint64 state                : 2;  // enum RenderState
            quint64 startedAt            : 30; // ~ max 12days (msecs since reference)
            quint64 stuckCounterSync     : 4;
            quint64 stuckCounterRender   : 4;
            quint64 stuckCounterSwap     : 4;
            quint64 longestStuckType     : 2;  // enum RenderState
            quint64 longestStuckDuration : 18; // ~ max 4min (msecs)
        } quickWindowBits;
        struct {
            quint64 reserved1            : 2;
            quint64 expectedAt           : 30; // ~ max 12days (msecs since reference)
            quint64 stuckCounter         : 4;
            quint64 reserved2            : 8;
            quint64 longestStuckDuration : 20; // ~ max 16min (msecs)
        } eventLoopBits;
    };
    Q_STATIC_ASSERT(sizeof(AtomicState) == 8);

    struct QuickWindowData {
        // watchdog thread use only
        QPointer<QQuickWindow> m_window;
        quint64 m_uniqueCounter = 0; // to avoid an ABA problem on m_window
        AtomicState m_lastState = { 0 };

        // written from the render thread, read from the watchdog thread
        QAtomicInteger<quint64> m_state = { 0 };
        QPointer<QThread> m_renderThread;
        quintptr m_renderThreadHandle = 0;
        QAtomicInteger<int> m_renderThreadSet = 0; // QPointer is not thread-safe
    };
    QList<QuickWindowData *> m_quickWindows;

    struct EventLoopData {
        // watchdog thread use only
        QPointer<QThread> m_thread;
        quint64 m_uniqueCounter = 0; // to avoid an ABA problem on m_window
        AtomicState m_lastState = { 0 };
        bool m_isMainThread = false;

        // written from the watched thread, read from the watchdog thread
        QAtomicInteger<quint64> m_state = { 0 };
        QTimer *m_timer = nullptr;
        QAtomicInteger<quintptr> m_threadHandle = 0; // QPointer is not thread-safe
    };
    QList<EventLoopData *> m_eventLoops;

    QAtomicInteger<int> m_threadIsBeingKilled = 0;
    QAtomicInteger<qint64> m_referenceTime;
    bool m_watchingMainEventLoop = false;
    QThread *m_wdThread;

    static Watchdog *s_instance;
    static QAtomicPointer<QTimer> s_mainThreadTimer; // we can't delete the timer in the destructor

    QTimer *m_quickWindowCheck;
    std::chrono::milliseconds m_quickWindowCheckInterval;
    std::chrono::milliseconds m_warnRenderStateTime[4];
    std::chrono::milliseconds m_killRenderStateTime[4];

    QTimer *m_eventLoopCheck;
    std::chrono::milliseconds m_eventLoopCheckInterval;
    std::chrono::milliseconds m_warnEventLoopTime;
    std::chrono::milliseconds m_killEventLoopTime;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // WATCHDOG_P_H
