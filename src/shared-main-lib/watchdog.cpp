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

/*

The watchdog is a class that monitors the event loop and the rendering of a QQuickWindow.
Since the code is running on a lot of different threads, we need to be careful with the
synchronization and atomicity of our data structures.

The "exactly how long" part of the watchdog is run directly on the monitored threads, using
synchronous callbacks. In order to minimize the runtime impact, these callbacks do very little
work: they just record the current time and the state in atomic variables. Any logging due to
exceeding timeouts is delegated to the watchdog thread via QMetaObject::invokeMethod.

The periodic checks and all logging is done from the dedicated watchdog thread.

Shutdown of the watchdog is tricky. Ideally we get the QCoreApplication::aboutToQuit signal: In this
case we can just stop the watchdog thread, wait for it to finish and then set the 'clean shutdown'
flag.
If the application's event loop was never started, there will be no aboutToQuit signal: In this case
we have to rely on the Watchdog destructor (being triggered by the QCoreApplication destructor) to
stop the watchdog thread, if the 'clean shutdown' flag is not set. This is not ideal, because the
watchdog thread is still active while the QCoreApplication instance is being destroyed and this will
result in TSAN warnings.

*/

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
            qCCritical(LogWatchdog).nospace()
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

QThreadStorage<WatchdogPrivate::EventLoopData *> WatchdogPrivate::s_eventLoopData = { };


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
            qCInfo(LogWatchdog) << "Event loop of thread" << static_cast<void *>(eld->m_thread)
                                << "has finished and is not being watched anymore";
            m_watchingMainEventLoop = false;
        }
        // eld is owned by QThreadStorage
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

        qCInfo(LogWatchdog).nospace() << "Systemd watchdog enabled (timeout is "
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
        qCWarning(LogWatchdog).nospace()
            << "Event loop warning timeout (" << m_warnEventLoopTime
            << ") is greater than kill timeout (" << m_killEventLoopTime << ")";
    }

    Watchdog::s_instance->m_active = isEventLoopWatchingEnabled() || isQuickWindowWatchingEnabled();

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

    if (!thread->eventDispatcher()) {
        qCWarning(LogWatchdog).nospace().noquote()
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
    eld->m_isMainThread = isMainThread;
    m_eventLoops << eld;

    if (isMainThread)
        m_watchingMainEventLoop = true;

    if (!m_eventLoopCheck->isActive())
        m_eventLoopCheck->start();

    qCInfo(LogWatchdog).nospace().noquote()
        << "Event loop of thread " << static_cast<void *>(thread) << info << " is being watched now "
        << "(check every " << m_eventLoopCheckInterval << ", warn/kill after "
        << m_warnEventLoopTime << "/" << m_killEventLoopTime << ")";

    // no finished signal for the main thread - only the destructor
    if (!eld->m_isMainThread) {
        connect(thread, &QThread::finished, this, [this, eld]() {
                // we're on the wd thread
                Q_ASSERT(QThread::currentThread() == m_wdThread);

                qCInfo(LogWatchdog) << "Event loop of thread" << static_cast<void *>(eld->m_thread)
                                    << "has finished and is not being watched anymore";

                m_eventLoops.removeOne(eld);
                // eld is owned by QThreadStorage and deleted automatically

                if (m_eventLoops.isEmpty() && m_eventLoopCheck->isActive())
                    m_eventLoopCheck->stop();
            }, Qt::QueuedConnection);
    }

    // Our notifyEvent callback does not know which EventLoopData is assigned to the current
    // thread. Looking this information up on each event in a thread-safe map/hash would be too
    // expensive. Instead we save the EventLoopData in a QThreadStorage, which is thread-local and
    // can be accessed quickly.
    // In order to set this up, we need to execute setLocalData() on the watched thread though:

    QObject *dummy = new QObject();
    dummy->moveToThread(thread);
    QMetaObject::invokeMethod(dummy, [=]() {
            WatchdogPrivate::s_eventLoopData.setLocalData(eld);
            delete dummy;
        }, Qt::QueuedConnection);

    // eld is now owned by QThreadStorage and deleted automatically
}

void WatchdogPrivate::eventNotify(EventLoopData *eld, bool begin)
{
    // we're on the watched thread

    Q_ASSERT(QThread::currentThread() == eld->m_thread);

    if (begin) {
        eld->m_timer = now();
    } else if (eld->m_timer) {
        // !begin && !m_timer would be the end of a nested event loop - we need to ignore that,
        // because we cannot record nested event start times
        auto elapsed = std::chrono::milliseconds(now() - eld->m_timer.fetchAndStoreAcquire(0));

        if (m_warnEventLoopTime.count() && (elapsed > m_warnEventLoopTime)) {
            QMetaObject::invokeMethod(m_eventLoopCheck,
                //[this, eld](std::chrono::milliseconds elapsed) { // TODO: modernize in 6.9
                [this, eld, elapsed]() {
                    // we're on the wd thread
                    Q_ASSERT(QThread::currentThread() == m_wdThread);

                    ++eld->m_stuckCounter;
                    if (elapsed > eld->m_longestStuckDuration)
                        eld->m_longestStuckDuration = elapsed;

                    qCWarning(LogWatchdog).nospace()
                        << "Event loop of thread " << static_cast<void *>(eld->m_thread.get())
                        << " was stuck for " << elapsed
                        << ", but then continued (the warn threshold is "
                        << m_warnEventLoopTime << ")";

                }, Qt::QueuedConnection /*, elapsed*/); // TODO: modernize in 6.9
        }
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

    const auto timeNow = now();

    for (auto *eld : std::as_const(m_eventLoops)) {
        if (!eld->m_thread || m_threadIsBeingKilled)
            continue;

        const quint64 counter = eld->m_stuckCounter;
        if (counter > eld->m_lastCounter) {
            eld->m_lastCounter = counter;

            qCWarning(LogWatchdog).nospace()
                << "Event loop of thread " << static_cast<void *>(eld->m_thread.get())
                << " was stuck " << counter << ((counter == 1) ? " time" : " times") << ". "
                << "The longest period was " << eld->m_longestStuckDuration << ".";
        }

        const quint64 lastEvent = eld->m_timer;
        if (!lastEvent)
            continue;

        const auto elapsed = std::chrono::milliseconds(timeNow - lastEvent);

        if (m_killEventLoopTime.count() && (elapsed > m_killEventLoopTime) && eld->m_thread) {
            qCCritical(LogWatchdog).nospace()
                << "Event loop of thread " << static_cast<void *>(eld->m_thread.get())
                << " is getting killed, because it is now stuck for over " << elapsed
                << " (the kill threshold is " << m_killEventLoopTime << ")";

            // avoid multiple messages, until the thread is actually killed
            m_threadIsBeingKilled = 1;
            killThread(eld->m_threadHandle);
        } else if (m_warnEventLoopTime.count() && (elapsed > m_warnEventLoopTime)
                   && (elapsed > (m_eventLoopCheckInterval / 2))) {
            qCWarning(LogWatchdog).nospace()
                << "Event loop of thread " << static_cast<void *>(eld->m_thread.get())
                << " is currently stuck for over " << elapsed
                << " (the warn threshold is " << m_warnEventLoopTime << ")";
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
        qCWarning(LogWatchdog).nospace()
            << "Quick window warning timeout (" << m_warnQuickWindowTime
            << ") is greater than kill timeout (" << m_killQuickWindowTime << ")";
    }

    Watchdog::s_instance->m_active = isEventLoopWatchingEnabled() || isQuickWindowWatchingEnabled();
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
            qCInfo(LogWatchdog).nospace().noquote()
                << "Window " << win << info << " is being watched now "
                << "(check every " << check << ", warn/kill after "
                << warn << "/" << kill << ")";
        }, Qt::QueuedConnection);

    connect(quickWindow, &QObject::destroyed, this, [this, qwd](QObject *o) {
        // we're on wd thread
        Q_ASSERT(QThread::currentThread() == m_wdThread);

        qCInfo(LogWatchdog) << "Window" << static_cast<void *>(o)
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

        const auto timeNow = now();
        const auto elapsed = std::chrono::milliseconds(timeNow - qwd->m_timer.fetchAndStoreOrdered(timeNow));

        if (fromState != Idle) { // being stuck in the 'Idle' state is not an issue
            if (m_warnQuickWindowTime.count() && (elapsed > m_warnQuickWindowTime)) {
                QMetaObject::invokeMethod(m_quickWindowCheck,
                    //[this, qwd](std::chrono::milliseconds elapsed, RenderState fromState) { // TODO: modernize in 6.9
                    [this, qwd, elapsed, fromState] {
                        // we're on the wd thread
                        Q_ASSERT(QThread::currentThread() == m_wdThread);
                        if (fromState == Sync)
                            ++qwd->m_stuckCounterSync;
                        else if (fromState == Render)
                            ++qwd->m_stuckCounterRender;
                        else if (fromState == Swap)
                            ++qwd->m_stuckCounterSwap;

                        if (elapsed > qwd->m_longestStuckDuration) {
                            qwd->m_longestStuckType = fromState;
                            qwd->m_longestStuckDuration = elapsed;
                        }

                        qCWarning(LogWatchdog).nospace()
                            << "Window " << static_cast<void *>(qwd->m_window.get())
                            << " was stuck in state " << fromState << " for " << elapsed
                            << ", but then continued (the warn threshold is "
                            << m_warnQuickWindowTime << ")";

                    }, Qt::QueuedConnection /*, elapsed, fromState*/); // TODO: modernize in 6.9
            }
        }
        qwd->m_renderState = char(toState);
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
            qwd->m_renderState = Idle;
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

    const auto timeNow = now();

    for (auto *qwd : std::as_const(m_quickWindows)) {
        if (!qwd->m_window)
            continue;

        const uint allCounters = qwd->m_stuckCounterSync + qwd->m_stuckCounterRender
                                 + qwd->m_stuckCounterSwap;

        if (allCounters > qwd->m_lastCounter) {
            qwd->m_lastCounter = allCounters;

            QByteArray stuckCounterString;

            auto appendStuckCounter = [&](quint64 counter, RenderState rs) {
                if (counter) {
                    if (!stuckCounterString.isEmpty())
                        stuckCounterString += ", ";
                    stuckCounterString = stuckCounterString + QByteArray::number(counter)
                                         + ((counter == 1) ? " time" : " times")
                                         + " in state " + renderStateName(rs);
                }
            };

            appendStuckCounter(qwd->m_stuckCounterSync, RenderState::Sync);
            appendStuckCounter(qwd->m_stuckCounterRender, RenderState::Render);
            appendStuckCounter(qwd->m_stuckCounterSwap, RenderState::Swap);

            qCWarning(LogWatchdog).nospace()
                << "Window " << static_cast<void *>(qwd->m_window.get()) << " was stuck "
                << stuckCounterString.constData() << ". The longest period was "
                << qwd->m_longestStuckDuration << " in state " << qwd->m_longestStuckType;
        }

        const auto renderState = static_cast<RenderState>(qwd->m_renderState.loadAcquire());
        if (renderState == Idle)
            continue;

        const auto elapsed = std::chrono::milliseconds(timeNow - qwd->m_timer);

        if (m_killQuickWindowTime.count() && (elapsed > m_killQuickWindowTime) && qwd->m_renderThread) {
            qCCritical(LogWatchdog).nospace()
                << "Window " << static_cast<void *>(qwd->m_window.get())
                << " is getting its render thread (" << static_cast<void *>(qwd->m_renderThread.get())
                << ") killed, because it is now stuck in state "
                << renderState << " for over " << elapsed
                << " (the kill threshold is " << m_killQuickWindowTime << ")";

            // avoid multiple messages, until the thread is actually killed
            m_threadIsBeingKilled = 1;
            killThread(qwd->m_renderThreadHandle);
        } else if (m_warnQuickWindowTime.count() && (elapsed > m_warnQuickWindowTime)
                   && (elapsed > (m_quickWindowCheckInterval / 2))) {
            qCWarning(LogWatchdog).nospace()
                << "Window " << static_cast<void *>(qwd->m_window.get())
                << " is currently stuck in state " << renderState
                << " for over " << elapsed << " (the warn threshold is " << m_warnQuickWindowTime << ")";
        }
    }
}

quint64 WatchdogPrivate::now() const
{
    QElapsedTimer et;
    et.start();
    qint64 msr = et.msecsSinceReference();
    return (msr < m_referenceTime) ? 0ULL : quint64(msr - m_referenceTime);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Watchdog
///////////////////////////////////////////////////////////////////////////////////////////////////

Watchdog *Watchdog::s_instance = nullptr;

Watchdog::Watchdog()
    : QObject(qApp)
    , d(new WatchdogPrivate(this))
{
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

    if (!s_instance)
        s_instance = new Watchdog;
    return s_instance;
}

Watchdog::~Watchdog()
{
    if (!m_cleanShutdown) {
        qCCritical(LogWatchdog) << "The watchdog could not properly shutdown, as no "
                                   "QCoreApplication::aboutToQuit signal was received "
                                   "(this will result in TSAN warnings).";
        shutdown();
    }
    s_instance = nullptr;

    // the finished() signal of the thread will auto-delete d from within that thread
}

void Watchdog::shutdown()
{
    m_active = false;

    auto wdThread = d->m_wdThread; // 'd' is dead after quit()
    wdThread->quit();
    wdThread->wait();
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

void Watchdog::eventCallback(const QThread *thread, bool begin, QObject *receiver, QEvent *event)
{
    // This function is called twice for every event: it has to be as efficient as possible.

    Q_ASSERT(this);
    Q_ASSERT(thread);
    Q_ASSERT(begin == bool(receiver));
    Q_ASSERT(begin == bool(event));
    Q_ASSERT(m_active);
    Q_ASSERT(d);

    if (auto *eld = std::as_const(WatchdogPrivate::s_eventLoopData).localData()) {
        Q_ASSERT(eld->m_thread == thread);
        d->eventNotify(eld, begin);
    }

    if (begin && (event->type() == QEvent::PlatformSurface)) {
        auto surfaceEventType = static_cast<const QPlatformSurfaceEvent *>(event)->surfaceEventType();
        if (surfaceEventType == QPlatformSurfaceEvent::SurfaceCreated) {
            if (auto *quickWindow = qobject_cast<QQuickWindow *>(receiver)) {
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
}

QT_END_NAMESPACE_AM
