// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef WATCHDOG_P_H
#define WATCHDOG_P_H

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtQuick/QQuickWindow>
#include <QtCore/QEventLoop>
#include <QtCore/QThreadStorage>

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


    void setQuickWindowTimeouts(std::chrono::milliseconds check, std::chrono::milliseconds warn,
                                std::chrono::milliseconds kill);
    bool isQuickWindowWatchingEnabled() const;
    void watchQuickWindow(QQuickWindow *quickWindow);
    void quickWindowCheck();

    enum RenderState : unsigned int {
        Idle = 0,
        Sync,
        Render,
        Swap,
    };

    struct QuickWindowData {
        // watchdog thread use only
        QPointer<QQuickWindow> m_window;
        quint64 m_uniqueCounter = 0; // to avoid an ABA problem on m_window
        bool m_threadedRenderLoop = false;
        uint m_stuckCounterSync = 0;
        uint m_stuckCounterRender = 0;
        uint m_stuckCounterSwap = 0;
        uint m_lastCounter = 0;
        std::chrono::milliseconds m_longestStuckDuration = { };
        RenderState m_longestStuckType = Idle;

        // written from the render thread, read from the watchdog thread
        QAtomicInteger<qint64> m_timer = { 0 };
        QAtomicInteger<char> m_renderState = { char(Idle) };
        QPointer<QThread> m_renderThread;
        quintptr m_renderThreadHandle = 0;
        QAtomicInteger<int> m_renderThreadSet = 0; // QPointer is not thread-safe
    };
    QList<QuickWindowData *> m_quickWindows;

    struct EventLoopData {
        // watchdog thread use only
        QPointer<QThread> m_thread;
        quint64 m_uniqueCounter = 0; // to avoid an ABA problem on m_thread
        bool m_isMainThread = false;
        uint m_stuckCounter = 0;
        uint m_lastCounter = 0;
        std::chrono::milliseconds m_longestStuckDuration = { };

        // written from the watched thread, read from the watchdog thread
        QAtomicInteger<qint64> m_timer = { 0 };
        QAtomicInteger<quintptr> m_threadHandle = 0; // QPointer is not thread-safe
    };
    void eventNotify(EventLoopData *eld, bool begin);
    // This needs to be static to outlive qApp, as we cannot remove local data once set,
    // which is a bit of design flaw in QThreadStorage
    static QThreadStorage<EventLoopData *> s_eventLoopData;

    QList<EventLoopData *> m_eventLoops;

    QAtomicInteger<int> m_threadIsBeingKilled = 0;
    bool m_watchingMainEventLoop = false;
    QThread *m_wdThread;

    quint64 now() const;
    qint64 m_referenceTime;

    QTimer *m_quickWindowCheck;
    std::chrono::milliseconds m_quickWindowCheckInterval { };
    std::chrono::milliseconds m_warnQuickWindowTime { };
    std::chrono::milliseconds m_killQuickWindowTime { };

    QTimer *m_eventLoopCheck;
    std::chrono::milliseconds m_eventLoopCheckInterval { };
    std::chrono::milliseconds m_warnEventLoopTime { };
    std::chrono::milliseconds m_killEventLoopTime { };
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // WATCHDOG_P_H
