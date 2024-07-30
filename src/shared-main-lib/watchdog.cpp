// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QThread>
#include <private/qquickwindow_p.h>
#include <private/qsgrenderloop_p.h>

#include "logging.h"
#include "utilities.h"
#include "watchdog.h"
#include "watchdog_p.h"
#include "qtappman_common-config_p.h"

#if defined(Q_OS_UNIX)
#  include <pthread.h>
#  include <csignal>
#  if QT_CONFIG(am_systemd_watchdog)
#    include <systemd/sd-daemon.h>
#  endif
#elif defined(Q_OS_WINDOWS)
#  include <windows.h>
#endif

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;


QT_BEGIN_NAMESPACE_AM

static const char *renderStateName(WatchdogPrivate::RenderState rs)
{
    switch (rs) {
    case WatchdogPrivate::Idle:   return "Idle";
    case WatchdogPrivate::Sync:   return "Syncing";
    case WatchdogPrivate::Render: return "Rendering";
    case WatchdogPrivate::Swap:   return "Swapping";
    }
    return "";
}

QDebug &operator<<(QDebug &dbg, WatchdogPrivate::RenderState rs)
{
    dbg << renderStateName(rs);
    return dbg;
}

static quintptr currentThreadHandle()
{
#if defined(Q_OS_DARWIN)
    return reinterpret_cast<quintptr>(::pthread_self());  // pointer
#elif defined(Q_OS_UNIX)
    return static_cast<quintptr>(::pthread_self());  // ulong
#elif defined(Q_OS_WINDOWS)
    return static_cast<quintptr>(::GetCurrentThreadId());  // DWORD
#else
    return 0;
#endif
}

static void killThread(quintptr threadHandle)
{
#if defined(Q_OS_DARWIN)
    ::pthread_kill(reinterpret_cast<pthread_t>(threadHandle), SIGABRT);
#elif defined(Q_OS_UNIX)
    ::pthread_kill(static_cast<pthread_t>(threadHandle), SIGABRT);
#elif defined(Q_OS_WINDOWS)
    auto winId = static_cast<DWORD>(threadHandle);

    if (::GetCurrentThreadId() == winId) {
        ::abort();
    } else {
        bool ok = false;
        auto winHandle = ::OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                      false, winId);
        // There's no built-in way on Windows to abort a specific thread
        // We try to set the single-step trap on the thread and expect it to abort
        if (winHandle) {
            CONTEXT context = { };
            context.ContextFlags = CONTEXT_CONTROL;
            if (::SuspendThread(winHandle) == 0) { // 0: not suspended before
                if (::GetThreadContext(winHandle, &context) > 0) {
                    context.ContextFlags = CONTEXT_CONTROL;
#  if defined(Q_PROCESSOR_ARM_64)
                    context.Cpsr |= 0x200000;  // single-step trap
#  elif defined(Q_PROCESSOR_X86_64)
                    context.EFlags |= 0x100; // single-step trap
#  else
                    static_assert(false, "This architecture is not supported.");
#  endif
                    if (::SetThreadContext(winHandle, &context) > 0) {
                        if (::ResumeThread(winHandle) == 1) // 1: suspended once
                            ok = true;
                    }
                }
            }
            ::CloseHandle(winHandle);
        }
        if (!ok) {
            qCCritical(LogWatchdogStat).nospace()
                << "Failed to kill thread " << winId << ". Aborting process instead";
            ::abort();
        }
    }
#else
    Q_UNUSED(threadHandle)
#endif
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// WatchdogPrivate
///////////////////////////////////////////////////////////////////////////////////////////////////

Watchdog *WatchdogPrivate::s_instance = nullptr;
QAtomicPointer<QTimer> WatchdogPrivate::s_mainThreadTimer;

WatchdogPrivate::WatchdogPrivate(Watchdog *q)
    : QObject(nullptr)
    , m_wdThread(new QThread(q))
    , m_quickWindowCheck(new QTimer(this))
    , m_eventLoopCheck(new QTimer(this))
{
    // we're on the ui thread
    Q_ASSERT(QThread::currentThread() == qApp->thread());

    m_wdThread->setObjectName("QtAM-Watchdog");
    moveToThread(m_wdThread);
    connect(m_wdThread, &QThread::finished, this, [this]() { delete this; }, Qt::DirectConnection);

    // "this" is now on the wd thread
    Q_ASSERT(thread() == m_wdThread);
    Q_ASSERT(m_quickWindowCheck->thread() == m_wdThread);

    QElapsedTimer et;
    et.start();
    Q_ASSERT(et.isMonotonic());
    m_referenceTime = et.msecsSinceReference();

    //TODO: detect overflow on the 30 bit elapsed timer and reset the reference time
    //      needs to < ~12days, maybe default to 1day

    connect(m_quickWindowCheck, &QTimer::timeout,
            this, &WatchdogPrivate::quickWindowCheck);

    connect(m_eventLoopCheck, &QTimer::timeout,
            this, &WatchdogPrivate::eventLoopCheck);

    QMetaObject::invokeMethod(this, &WatchdogPrivate::setupSystemdWatchdog, Qt::QueuedConnection);
}

WatchdogPrivate::~WatchdogPrivate()
{
    Q_ASSERT(QThread::currentThread() == m_wdThread);
    for (const auto *eld : std::as_const(m_eventLoops)) {
        if (eld->m_isMainThread) {
            qCInfo(LogWatchdogStat) << "Event loop of thread" << static_cast<void *>(eld->m_thread)
                                    << "/ the main GUI thread has finished and is not being watched anymore";
            s_mainThreadTimer = eld->m_timer; // cannot delete from this thread
            m_watchingMainEventLoop = false;
        }
        delete eld;
    }
    for (const auto *qwd : std::as_const(m_quickWindows))
        delete qwd;
}

void WatchdogPrivate::setupSystemdWatchdog()
{
#if QT_CONFIG(am_systemd_watchdog)
    // we're on the wd thread
    Q_ASSERT(QThread::currentThread() == m_wdThread);

    uint64_t wdTimeoutUsec = 0;
    if (::sd_watchdog_enabled(1, &wdTimeoutUsec) <= 0)
        wdTimeoutUsec = 0;

    if (wdTimeoutUsec > 0) {
        static auto sdTrigger = []() { ::sd_notify(0, "WATCHDOG=1"); };
        auto sdTimer = new QTimer(this);
        sdTimer->setInterval(wdTimeoutUsec / 1000 / 2);
        // the timer is triggered on the wd thread
        connect(sdTimer, &QTimer::timeout, this, sdTrigger);
        sdTrigger();
        sdTimer->start();

        qCInfo(LogWatchdogStat).nospace() << "Systemd watchdog enabled (timeout is "
                                          << (wdTimeoutUsec / 1000) << "ms)";
    }
#endif
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// WatchdogPrivate   /   EventLoop
///////////////////////////////////////////////////////////////////////////////////////////////////


void WatchdogPrivate::setEventLoopTimeouts(std::chrono::milliseconds check, std::chrono::milliseconds warn,
                                           std::chrono::milliseconds kill)
{
    m_eventLoopCheckInterval = std::max(0ms, check);
    m_eventLoopCheck->setInterval(m_eventLoopCheckInterval);
    if (m_eventLoopCheckInterval > 0ms)
        m_eventLoopCheck->start();
    else
        m_eventLoopCheck->stop();

    // these will be picked up automatically
    m_warnEventLoopTime = std::max(0ms, warn);
    m_killEventLoopTime = std::max(0ms, kill);

    if (m_warnEventLoopTime == m_killEventLoopTime)
        m_warnEventLoopTime = 0ms;

    if (m_warnEventLoopTime > m_killEventLoopTime) {
        qCWarning(LogWatchdogStat).nospace()
            << "Event loop warning timeout (" << m_warnEventLoopTime
            << ") is greater than kill timeout (" << m_killEventLoopTime << ")";
    }

    // watch the main event loop
    if (!m_watchingMainEventLoop)
        watchEventLoop(qApp->thread());
}

bool WatchdogPrivate::isEventLoopWatchingEnabled() const
{
    if (m_eventLoopCheckInterval <= 0ms)
        return false;
    else
        return (m_warnEventLoopTime > 0ms) || (m_killEventLoopTime > 0ms);
}

void WatchdogPrivate::watchEventLoop(QThread *thread)
{
    // we're on the wd thread
    Q_ASSERT(QThread::currentThread() == m_wdThread);

    if (!thread)
        return;

    if (!isEventLoopWatchingEnabled())
        return;

    QString info;
    const auto className = thread->metaObject()->className();
    const auto objectName = thread->objectName();
    bool isMainThread = qApp && (thread == qApp->thread());
    if (className && qstrcmp(className, "QThread"))
        info = info + u" / class: " + QString::fromLatin1(className);
    if (!objectName.isEmpty())
        info = info + u" / name: " + objectName;
    if (isMainThread)
        info = info + u" / the main GUI thread";

    if (!thread->eventDispatcher()) {
        qCWarning(LogWatchdogStat).nospace().noquote()
            << "Event loop of thread " << static_cast<void *>(thread) << info
            << " cannot be watched, because the thread has no event dispatcher installed";
        return;
    }

    for (const auto *eld : std::as_const(m_eventLoops)) {
        if (eld->m_thread == thread)
            return;
    }

    auto *eld = new EventLoopData;
    static quint64 uniqueCounter = 0;
    eld->m_thread = thread;
    eld->m_uniqueCounter = ++uniqueCounter;
    eld->m_timer = new QTimer;
    eld->m_isMainThread = isMainThread;
    m_eventLoops << eld;

    if (isMainThread)
        m_watchingMainEventLoop = true;

    if (!m_eventLoopCheck->isActive())
        m_eventLoopCheck->start();

    qCInfo(LogWatchdogStat).nospace().noquote()
        << "Event loop of thread " << static_cast<void *>(thread) << info << " is being watched now "
        << "(check every " << m_eventLoopCheckInterval << ", warn/kill after "
        << m_warnEventLoopTime << "/" << m_killEventLoopTime << ")";

    eld->m_timer->setInterval((m_warnEventLoopTime > 0ms ? m_warnEventLoopTime : m_killEventLoopTime) / 2);
    eld->m_timer->start();
    eld->m_timer->moveToThread(thread);
    connect(eld->m_timer, &QTimer::timeout, eld->m_timer, [this, eld]() {
        // we're on the watched thread
        Q_ASSERT(QThread::currentThread() == eld->m_thread);

        if (!eld->m_threadHandle)
            eld->m_threadHandle = currentThreadHandle();

        AtomicState as;
        as.pod = eld->m_state;

        QElapsedTimer et;
        et.start();
        quint64 now = quint64(et.msecsSinceReference() - m_referenceTime);
        quint64 expectedAt = as.eventLoopBits.expectedAt;
        quint64 elapsed = (!expectedAt || (now < expectedAt)) ? 0 : (now - expectedAt);

        bool wasStuck = false;
        quint64 maxTime = quint64(m_warnEventLoopTime.count());

        if (maxTime && (elapsed > maxTime)) {
            wasStuck = true;
            as.eventLoopBits.stuckCounter = std::clamp(as.eventLoopBits.stuckCounter + 1ULL, 0ULL, (1ULL << 4) - 1);
        }
        if (wasStuck && (elapsed > as.eventLoopBits.longestStuckDuration))
            as.eventLoopBits.longestStuckDuration = std::clamp(elapsed, 0ULL, (1ULL << 20) - 1);

        if (wasStuck) {
            qCWarning(LogWatchdogLive).nospace()
                << "Event loop of thread " << static_cast<void *>(eld->m_thread.get())
                << " was stuck for " << elapsed << "ms, but then continued (the warn threshold is "
                << maxTime << "ms)";
        }

        as.eventLoopBits.expectedAt = now + maxTime / 2;
        eld->m_state = as.pod;

        auto nextDuration = (m_warnEventLoopTime > 0ms ? m_warnEventLoopTime : m_killEventLoopTime) / 2;
        if (eld->m_timer->intervalAsDuration() != nextDuration)
            eld->m_timer->setInterval(nextDuration);
    });

    // no finished signal for the main thread - only the destructor
    if (!eld->m_isMainThread) {
        connect(thread, &QThread::finished, this, [eld]() {
                // we're on the watched thread
                Q_ASSERT(QThread::currentThread() == eld->m_thread);

                delete eld->m_timer;
                eld->m_timer = nullptr;
            }, Qt::DirectConnection);

        connect(thread, &QThread::finished, this, [this, eld]() {
                // we're on the wd thread
                Q_ASSERT(QThread::currentThread() == m_wdThread);

                qCInfo(LogWatchdogStat) << "Event loop of thread" << static_cast<void *>(eld->m_thread)
                                        << "has finished and is not being watched anymore";

                m_eventLoops.removeOne(eld);
                delete eld;

                if (m_eventLoops.isEmpty() && m_eventLoopCheck->isActive())
                    m_eventLoopCheck->stop();
            }, Qt::QueuedConnection);
    }
}

void WatchdogPrivate::eventLoopCheck()
{
    // we're on the wd thread
    Q_ASSERT(QThread::currentThread() == m_wdThread);

    // this is the "print statistics" and "kill thread" timer

    // no point in spamming the log, while libbacktrace takes its time
    if (m_threadIsBeingKilled)
        return;

    for (auto *eld : std::as_const(m_eventLoops)) {
        if (!eld->m_thread || m_threadIsBeingKilled)
            continue;

        AtomicState as;
        as.pod = eld->m_state;

        if (as.eventLoopBits.stuckCounter > eld->m_lastState.eventLoopBits.stuckCounter) {
            static constexpr quint64 maxCounter = (1ULL << 4) - 2;
            QByteArray stuckCounterString;
            quint64 counter = as.eventLoopBits.stuckCounter;

            if (counter) {
                if (counter > maxCounter)
                    stuckCounterString += "more than ";
                stuckCounterString = stuckCounterString + QByteArray::number(counter)
                                     + ((counter == 1) ? " time" : " times");
            }

            qCWarning(LogWatchdogStat).nospace()
                << "Event loop of thread " << static_cast<void *>(eld->m_thread.get())
                << " was stuck " << stuckCounterString.constData() << ". "
                << "The longest period was " << as.eventLoopBits.longestStuckDuration << "ms";
        }
        eld->m_lastState.pod = as.pod;

        QElapsedTimer et;
        et.start();
        quint64 now = quint64(et.msecsSinceReference() - m_referenceTime);
        quint64 expectedAt = as.eventLoopBits.expectedAt;
        quint64 elapsed = (!expectedAt || (now < expectedAt)) ? 0 : (now - expectedAt);
        quint64 warnTime = quint64(m_warnEventLoopTime.count());
        quint64 killTime = quint64(m_killEventLoopTime.count());

        if (killTime && (elapsed > killTime) && eld->m_thread) {
            qCCritical(LogWatchdogStat).nospace()
                << "Event loop of thread " << static_cast<void *>(eld->m_thread.get())
                << " is getting killed, because it is now stuck for over " << elapsed
                << "ms (the kill threshold is " << killTime << "ms)";

            // avoid multiple messages, until the thread is actually killed
            m_threadIsBeingKilled = 1;
            killThread(eld->m_threadHandle);
        } else if (warnTime && (elapsed > warnTime)
                   && (elapsed > (quint64(m_eventLoopCheckInterval.count()) / 2))) {
            qCWarning(LogWatchdogStat).nospace()
                << "Event loop of thread " << static_cast<void *>(eld->m_thread.get())
                << " is currently stuck for over " << elapsed
                << "ms (the warn threshold is " << warnTime << "ms)";
        }
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// WatchdogPrivate   /   QuickWindow
///////////////////////////////////////////////////////////////////////////////////////////////////


void WatchdogPrivate::setQuickWindowTimeouts(std::chrono::milliseconds check,
                                             std::chrono::milliseconds warn,
                                             std::chrono::milliseconds kill)
{
    m_quickWindowCheckInterval = std::max(0ms, check);
    m_quickWindowCheck->setInterval(m_quickWindowCheckInterval);
    if (m_quickWindowCheckInterval > 0ms)
        m_quickWindowCheck->start();
    else
        m_quickWindowCheck->stop();

    // these will be picked up automatically
    m_warnQuickWindowTime = std::max(0ms, warn);
    m_killQuickWindowTime = std::max(0ms, kill);

    if (m_warnQuickWindowTime == m_killQuickWindowTime)
        m_warnQuickWindowTime = 0ms;

    if (m_warnQuickWindowTime > m_killQuickWindowTime) {
        qCWarning(LogWatchdogStat).nospace()
            << "Quick window warning timeout (" << m_warnQuickWindowTime
            << ") is greater than kill timeout (" << m_killQuickWindowTime << ")";
    }
}

bool WatchdogPrivate::isQuickWindowWatchingEnabled() const
{
    if (m_quickWindowCheckInterval <= 0ms)
        return false;
    else
        return (m_warnQuickWindowTime > 0ms) || (m_killQuickWindowTime > 0ms);
}

void WatchdogPrivate::watchQuickWindow(QQuickWindow *quickWindow)
{
    // This function is called from the main thread with Qt::BlockingQueuedConnection.
    // We need to be as quick as possible here to not block the UI.

    // we're on the wd thread
    Q_ASSERT(QThread::currentThread() == m_wdThread);

    if (!quickWindow)
        return;

    if (!isQuickWindowWatchingEnabled())
        return;

    for (const auto *qwd : std::as_const(m_quickWindows)) {
        if (qwd->m_window == quickWindow)
            return;
    }

    auto renderLoop = QQuickWindowPrivate::get(quickWindow)->windowManager;
    if (!renderLoop) {
        // this is not a visible window, but a render target
        return;
    }

    auto *qwd = new QuickWindowData;
    static quint64 uniqueCounter = 0;
    qwd->m_window = quickWindow;
    qwd->m_uniqueCounter = ++uniqueCounter;
    qwd->m_threadedRenderLoop = (qstrcmp(renderLoop->metaObject()->className(),
                                         "QSGGuiThreadRenderLoop") != 0);
    m_quickWindows << qwd;

    if (!m_quickWindowCheck->isActive())
        m_quickWindowCheck->start();

    QString info;
    const auto className = quickWindow->metaObject()->className();
    const auto objectName = quickWindow->objectName();
    const auto title = quickWindow->title();
    if (className && qstrcmp(className, "QQuickWindowQmlImpl"))
        info = info + u" / class: " + QString::fromLatin1(className);
    if (qwd->m_threadedRenderLoop)
        info = info + u" / threaded rendering";
    if (!objectName.isEmpty())
        info = info + u" / name: \"" + objectName + u'"';
    if (!title.isEmpty())
        info = info + u" / title: \"" + title + u'"';

    // We're in a BlockingQueued slot call from the UI thread, we cannot log here
    QMetaObject::invokeMethod(this, [win = static_cast<void *>(quickWindow), info,
               check = m_quickWindowCheckInterval, warn = m_warnQuickWindowTime,
               kill = m_killQuickWindowTime]() {
            qCInfo(LogWatchdogStat).nospace().noquote()
                << "Window " << win << info << " is being watched now "
                << "(check every " << check << ", warn/kill after "
                << warn << "/" << kill << ")";
        }, Qt::QueuedConnection);

    connect(quickWindow, &QObject::destroyed, this, [this, qwd](QObject *o) {
        // we're on wd thread
        Q_ASSERT(QThread::currentThread() == m_wdThread);

        qCInfo(LogWatchdogStat) << "Window" << static_cast<void *>(o)
                                << "has been destroyed and is not being watched anymore";
        m_quickWindows.removeOne(qwd);
        delete qwd;

        if (m_quickWindows.isEmpty() && m_quickWindowCheck->isActive())
            m_quickWindowCheck->stop();
    });

    auto changeState = [this](QuickWindowData *qwd, RenderState fromState, RenderState toState)
    {
        // we're on the render thread
        Q_ASSERT(QThread::currentThread() != m_wdThread);
        if (qwd->m_threadedRenderLoop)
            Q_ASSERT(QThread::currentThread() != qApp->thread());
        else
            Q_ASSERT(QThread::currentThread() == qApp->thread());

        // this function is never called on the same wd concurrently!

        if (!qwd->m_renderThreadSet) {
            qwd->m_renderThread = QThread::currentThread();
            qwd->m_renderThreadHandle = currentThreadHandle();
            qwd->m_renderThreadSet = 1;
        }

        AtomicState as;
        as.pod = qwd->m_state;

        as.quickWindowBits.state = quint64(toState);
        QElapsedTimer et;
        et.start();
        quint64 now = quint64(et.msecsSinceReference() - m_referenceTime);
        quint64 startedAt = as.quickWindowBits.startedAt;
        quint64 elapsed = (!startedAt || (now < startedAt)) ? 0 : (now - startedAt);
        as.quickWindowBits.startedAt =  now;

        bool wasStuck = false;
        quint64 maxTime = quint64(m_warnQuickWindowTime.count());

        if (maxTime && (elapsed > maxTime)) {
            wasStuck = true;

            if ((fromState == Sync) && (toState == Render))
                as.quickWindowBits.stuckCounterSync = std::clamp(as.quickWindowBits.stuckCounterSync + 1ULL, 0ULL, (1ULL << 4) - 1);
            else if ((fromState == Render) && (toState == Swap))
                as.quickWindowBits.stuckCounterRender = std::clamp(as.quickWindowBits.stuckCounterRender + 1ULL, 0ULL, (1ULL << 4) - 1);
            else if ((fromState == Swap) && (toState == Idle))
                as.quickWindowBits.stuckCounterSwap = std::clamp(as.quickWindowBits.stuckCounterSwap + 1ULL, 0ULL, (1ULL << 4) - 1);
        }
        if (wasStuck && (elapsed > as.quickWindowBits.longestStuckDuration)) {
            as.quickWindowBits.longestStuckType = quint64(fromState);
            as.quickWindowBits.longestStuckDuration = std::clamp(elapsed, 0ULL, (1ULL << 18) - 1);
        }

        qwd->m_state = as.pod;

        if (wasStuck) {
            qCWarning(LogWatchdogLive).nospace()
                << "Window " << static_cast<void *>(qwd->m_window.get())
                << " was stuck in state " << fromState << " for " << elapsed
                << "ms, but then continued (the warn threshold is " << maxTime << "ms)";
        }
    };

    connect(quickWindow, &QQuickWindow::beforeSynchronizing, this, [=]() {
            changeState(qwd, Idle, Sync);
        }, Qt::DirectConnection);
    connect(quickWindow, &QQuickWindow::beforeRendering, this, [=]() {
            changeState(qwd, Sync, Render);
        }, Qt::DirectConnection);
    connect(quickWindow, &QQuickWindow::afterRendering, this, [=]() {
            changeState(qwd, Render, Swap);
        }, Qt::DirectConnection);
    connect(quickWindow, &QQuickWindow::frameSwapped, this, [=]() {
            changeState(qwd, Swap, Idle);
        }, Qt::DirectConnection);
    connect(quickWindow, &QQuickWindow::sceneGraphAboutToStop, this, [=]() {
            AtomicState as = { 0 };
            as.quickWindowBits.state = quint64(Idle);
            qwd->m_state = as.pod;
        }, Qt::DirectConnection);
}

void WatchdogPrivate::quickWindowCheck()
{
    // we're on the wd thread
    Q_ASSERT(QThread::currentThread() == m_wdThread);

    // this is the "print statistics" and "kill thread" timer

    // no point in spamming the log, while libbacktrace takes its time
    if (m_threadIsBeingKilled)
        return;

    for (auto *qwd : std::as_const(m_quickWindows)) {
        if (!qwd->m_window)
            continue;

        AtomicState as;
        as.pod = qwd->m_state;

        // only print the stats, if any "stuck" counter changed since last time
        if ((as.quickWindowBits.stuckCounterSync > qwd->m_lastState.quickWindowBits.stuckCounterSync)
            || (as.quickWindowBits.stuckCounterRender > qwd->m_lastState.quickWindowBits.stuckCounterRender)
            || (as.quickWindowBits.stuckCounterSwap > qwd->m_lastState.quickWindowBits.stuckCounterSwap)) {
            static constexpr quint64 maxCounter = (1ULL << 4) - 2;
            QByteArray stuckCounterString;

            auto appendStuckCounter = [&](quint64 counter, RenderState rs) {
                if (counter) {
                    if (!stuckCounterString.isEmpty())
                        stuckCounterString += ", ";
                    if (counter > maxCounter)
                        stuckCounterString += "more than ";
                    stuckCounterString = stuckCounterString + QByteArray::number(counter)
                                         + ((counter == 1) ? " time" : " times")
                                         + " in state " + renderStateName(rs);
                }
            };

            appendStuckCounter(as.quickWindowBits.stuckCounterSync, RenderState::Sync);
            appendStuckCounter(as.quickWindowBits.stuckCounterRender, RenderState::Render);
            appendStuckCounter(as.quickWindowBits.stuckCounterSwap, RenderState::Swap);

            qCWarning(LogWatchdogStat).nospace()
                << "Window " << static_cast<void *>(qwd->m_window.get()) << " was stuck "
                << stuckCounterString.constData() << ". The longest period was "
                << as.quickWindowBits.longestStuckDuration << "ms in state "
                << RenderState(as.quickWindowBits.longestStuckType);
        }
        qwd->m_lastState.pod = as.pod;

        if (as.quickWindowBits.state == quint64(Idle))
            continue;

        QElapsedTimer et;
        et.start();
        quint64 now = quint64(et.msecsSinceReference() - m_referenceTime);
        quint64 startedAt = as.quickWindowBits.startedAt;
        quint64 elapsed = (!startedAt || (now < startedAt)) ? 0 : (now - startedAt);
        quint64 warnTime = quint64(m_warnQuickWindowTime.count());
        quint64 killTime = quint64(m_killQuickWindowTime.count());

        if (killTime && (elapsed > killTime) && qwd->m_renderThread) {
            qCCritical(LogWatchdogStat).nospace()
                << "Window " << static_cast<void *>(qwd->m_window.get())
                << " is getting its render thread (" << static_cast<void *>(qwd->m_renderThread.get())
                << ") killed, because it is now stuck in state "
                << RenderState(as.quickWindowBits.state) << " for over " << elapsed
                << "ms (the kill threshold is " << killTime << "ms)";

            // avoid multiple messages, until the thread is actually killed
            m_threadIsBeingKilled = 1;
            killThread(qwd->m_renderThreadHandle);
        } else if (warnTime && (elapsed > warnTime)
                   && (elapsed > (quint64(m_quickWindowCheckInterval.count()) / 2))) {
            qCWarning(LogWatchdogStat).nospace()
                << "Window " << static_cast<void *>(qwd->m_window.get())
                << " is currently stuck in state " << RenderState(as.quickWindowBits.state)
                << " for over " << elapsed << "ms (the warn threshold is " << warnTime << "ms)";
        }
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Watchdog
///////////////////////////////////////////////////////////////////////////////////////////////////


Watchdog::Watchdog()
    : QObject(qApp)
    , d(new WatchdogPrivate(this))
{
    // automatically watch any QQuickWindows that are created
    qApp->installEventFilter(this);

    // we need to stop event handling in the WD thread *before* ~QCoreApplication,
    // otherwise we run into a data-race on the qApp vptr:
    // https://github.com/google/sanitizers/wiki/ThreadSanitizerPopularDataRaces#data-race-on-vptr
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        shutdown();
        m_cleanShutdown = true;
    });
    d->m_wdThread->start();
}

Watchdog *Watchdog::create()
{
    Q_ASSERT(qApp);
    Q_ASSERT(QThread::currentThread() == qApp->thread());

    if (!WatchdogPrivate::s_instance)
        WatchdogPrivate::s_instance = new Watchdog;
    return WatchdogPrivate::s_instance;
}

Watchdog::~Watchdog()
{
    if (!m_cleanShutdown) {
        qCCritical(LogWatchdogStat) << "The watchdog could not properly shutdown, as no "
                                       "QCoreApplication::aboutToQuit signal was received "
                                       "(this will result in TSAN warnings).";
        shutdown();
    }
    WatchdogPrivate::s_instance = nullptr;

    // the finished() signal of the thread will auto-delete d from within that thread
}

void Watchdog::shutdown()
{
    auto wdThread = d->m_wdThread; // 'd' is dead after quit()
    wdThread->quit();
    wdThread->wait();

    if (qApp)
        qApp->removeEventFilter(this);

    // the main thread timer must be deleted from the main thread
    if (auto *timer = WatchdogPrivate::s_mainThreadTimer.fetchAndStoreOrdered(nullptr))
        delete timer;
}

void Watchdog::setEventLoopTimeouts(std::chrono::milliseconds check,
                                    std::chrono::milliseconds warn, std::chrono::milliseconds kill)
{
    QMetaObject::invokeMethod(d, [this, check, warn, kill]() {
            d->setEventLoopTimeouts(check, warn, kill);
        }, Qt::QueuedConnection);
}

void Watchdog::setQuickWindowTimeouts(std::chrono::milliseconds check,
                                      std::chrono::milliseconds warn, std::chrono::milliseconds kill)
{
    QMetaObject::invokeMethod(d, [this, check, warn, kill]() {
            d->setQuickWindowTimeouts(check, warn, kill);
        }, Qt::QueuedConnection);
}

bool Watchdog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::PlatformSurface) {
        auto surfaceEventType = static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType();
        if (surfaceEventType == QPlatformSurfaceEvent::SurfaceCreated) {
            if (auto *quickWindow = qobject_cast<QQuickWindow *>(watched)) {
                QPointer p(quickWindow);
                // We need a blocking invoke here to ensure that quickWindow is still valid
                // on the watchdog thread when calling watchQuickWindow().
                // Otherwise, the window could be destroyed prematurely and - even worse - we could
                // run into an ABA problem on quickWindow.
                QMetaObject::invokeMethod(d, [this, p]() {
                        d->watchQuickWindow(p.get());
                    }, Qt::BlockingQueuedConnection);
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

QT_END_NAMESPACE_AM
