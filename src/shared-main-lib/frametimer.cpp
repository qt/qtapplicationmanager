// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "frametimer.h"

#include <QQuickWindow>
#include <qqmlinfo.h>

/*!
    \qmltype FrameTimer
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides frame-rate information about a given window.

    FrameTimer is used to get frame-rate information for a given window. The window can be either
    a toplevel Window (from the QtQuick.Window module) or a WindowObject
    (from the QtApplicationManager.SystemUI module).

    The following snippet shows how to use FrameTimer to display the frame-rate of a Window:

    \qml
    import QtQuick 2.11
    import QtApplicationManager 1.0

    Window {
        id: toplevelWindow
        ...
        FrameTimer {
            id: frameTimer
            running: topLevelWindow.visible
            window: toplevelWindow
        }
        Text {
            text: "FPS: " + Number(frameTimer.averageFps).toLocaleString(Qt.locale("en_US"), 'f', 1)
        }
    }
    \endqml

    You can also use this component as a MonitorModel data source if you want to plot its
    previous values over time:

    \qml
    import QtQuick 2.11
    import QtApplicationManager 1.0

    Window {
        id: toplevelWindow
        ...
        MonitorModel {
            running: true
            FrameTimer {
                window: toplevelWindow
            }
        }
    }
    \endqml

    Please note that when using FrameTimer as a MonitorModel data source there's no need to set it
    to \l{FrameTimer::running}{running} as MonitorModel will already call update() as needed.
*/

QT_BEGIN_NAMESPACE_AM

const qreal FrameTimer::MicrosInSec = qreal(1000 * 1000);

FrameTimer::FrameTimer(QObject *parent)
    : QObject(parent)
{
    m_updateTimer.setInterval(1000);
    connect(&m_updateTimer, &QTimer::timeout, this, &FrameTimer::update);
}

void FrameTimer::newFrame()
{
    int frameTime = m_timer.isValid() ? qMax(1, int(m_timer.nsecsElapsed() / 1000)) : IdealFrameTime;
    m_timer.restart();

    m_count++;
    m_sum += frameTime;
    m_min = qMin(m_min, frameTime);
    m_max = qMax(m_max, frameTime);
    m_jitter += qAbs(MicrosInSec / IdealFrameTime - MicrosInSec / frameTime);
}

void FrameTimer::reset()
{
    m_count = m_sum = m_max = 0;
    m_jitter = 0;
    m_min = std::numeric_limits<int>::max();
}

/*!
    \qmlproperty real FrameTimer::averageFps
    \readonly

    The average frame rate of the given \l window, in frames per second, since update()
    was last called (either manually or automatically in case \l{FrameTimer::running}{running} is set to true).

    \sa window running update()
*/
qreal FrameTimer::averageFps() const
{
    return m_averageFps;
}

/*!
    \qmlproperty real FrameTimer::minimumFps
    \readonly

    The minimum frame rate of the given \l window, in frames per second, since update()
    was last called (either manually or automatically in case \l{FrameTimer::running}{running} is set to true).

    \sa window running update()
*/
qreal FrameTimer::minimumFps() const
{
    return m_minimumFps;
}

/*!
    \qmlproperty real FrameTimer::maximumFps
    \readonly

    The maximum frame rate of the given \l window, in frames per second, since update()
    was last called (either manually or automatically in case \l{FrameTimer::running}{running} is set to true).

    \sa window running update()
*/
qreal FrameTimer::maximumFps() const
{
    return m_maximumFps;
}

/*!
    \qmlproperty real FrameTimer::jitterFps
    \readonly

    The frame rate jitter of the given \l window, in frames per second, since update()
    was last called (either manually or automatically in case \l{FrameTimer::running}{running} is set to true).

    \sa window running update()
*/
qreal FrameTimer::jitterFps() const
{
    return m_jitterFps;
}

/*!
    \qmlproperty Object FrameTimer::window

    The window to be monitored, from which frame-rate information will be gathered.
    It can be either a toplevel Window (from the QtQuick.Window module) or a WindowObject
    (from the QtApplicationManager.SystemUI module).

    \sa WindowObject
*/
QObject *FrameTimer::window() const
{
    return m_window;
}

void FrameTimer::setWindow(QObject *value)
{
    if (m_window == value)
        return;

    disconnectFromAppManWindow();
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);

    m_window = value;

    if (m_window) {
        if (!connectToQuickWindow() && !connectToAppManWindow())
            qmlWarning(this) << "The given window is neither a QQuickWindow nor a WindowObject.";
    }

    emit windowChanged();
}

bool FrameTimer::connectToQuickWindow()
{
    QQuickWindow *quickWindow = qobject_cast<QQuickWindow*>(m_window);
    if (!quickWindow)
        return false;

    connect(quickWindow, &QQuickWindow::frameSwapped, this, &FrameTimer::newFrame, Qt::UniqueConnection);
    return true;
}

bool FrameTimer::connectToAppManWindow()
{
    return false;
}


void FrameTimer::disconnectFromAppManWindow()
{ }

/*!
    \qmlproperty list<string> FrameTimer::roleNames
    \readonly

    Names of the roles provided by FrameTimer when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList FrameTimer::roleNames() const
{
    return { qSL("averageFps"), qSL("minimumFps"), qSL("maximumFps"), qSL("jitterFps") };
}

/*!
    \qmlmethod FrameTimer::update

    Updates the properties averageFps, minimumFps, maximumFps and jitterFps. Then resets internal
    counters so that new numbers can be taken for the new time period starting from the moment
    this method is called.

    Note that you normally don't have to call this method directly, as FrameTimer does it automatically
    every \l interval milliseconds while \l{FrameTimer::running}{running} is set to true.

    \sa running
*/
void FrameTimer::update()
{
    m_averageFps = m_sum ? MicrosInSec * m_count / m_sum : qreal(0);
    m_minimumFps = m_max ? MicrosInSec / m_max : qreal(0);
    m_maximumFps = m_min ? MicrosInSec / m_min : qreal(0);
    m_jitterFps = m_count ? m_jitter / m_count :  qreal(0);

    // Start counting again for the next sampling period but keep m_timer running because
    // we still need the diff between the last rendered frame and the upcoming one.
    reset();

    emit updated();
}

/*!
    \qmlproperty bool FrameTimer::running

    If \c true, update() will get called automatically every \l interval milliseconds.

    When using FrameTimer as a MonitorModel data source, this property should be kept as \c false.

    \sa update() interval
*/
bool FrameTimer::running() const
{
    return m_updateTimer.isActive();
}

void FrameTimer::setRunning(bool value)
{
    if (value && !m_updateTimer.isActive()) {
        m_updateTimer.start();
        emit runningChanged();
    } else if (!value && m_updateTimer.isActive()) {
        m_updateTimer.stop();
        emit runningChanged();
    }
}

/*!
    \qmlproperty int FrameTimer::interval

    The interval, in milliseconds, between update() calls while \l{FrameTimer::running}{running} is \c true.

    \sa update() running
*/
int FrameTimer::interval() const
{
    return m_updateTimer.interval();
}

void FrameTimer::setInterval(int value)
{
    if (value != m_updateTimer.interval()) {
        m_updateTimer.setInterval(value);
        emit intervalChanged();
    }
}

QT_END_NAMESPACE_AM

#include "moc_frametimer.cpp"
