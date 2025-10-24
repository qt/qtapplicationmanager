// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "framecontenttracker.h"
#include "applicationmanagerwindow.h"

#include <QQuickWindow>
#include <private/qquickwindow_p.h>
#include <private/qsgrhisupport_p.h>
#include <qqmlinfo.h>

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype FrameContentTracker
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides duplicate frame detection for a given window.
    \since 6.9

    FrameContentTracker is used to detect duplicate frames being rendered for a given window.

    The window must be a real toplevel Window (from the \c QtQuick.Window module) or
    an ApplicationManagerWindow from a multi-process app. These however do \b not work:
    WindowObject (from the \c QtApplicationManager.SystemUI module) as well as
    ApplicationManagerWindow in single-process mode.

    This is useful for detecting unnecessary redraws, which can become a performance issue:
    the most likely causes are (a) animations that are not stopped, even though they are not visible
    anymore and (b) Wayland client windows being updated, although they are hidden or the
    compositor is not showing the updated area(s).

    The implementation first copies each rendered frame from the GPU into a QImage on the CPU side.
    Then this frame is compared pixel-by-pixel to the previous frame. If they are the same, the
    duplicateFrames counter is incremented.

    \note Do not use this component in a production environment or even when profiling the
          application, as it will slow down the rendering significantly.

    Please note that when using FrameContentTracker as a MonitorModel data source there's no need to
    set it to \l running as MonitorModel will already call update() as needed.
*/

FrameContentTracker::FrameContentTracker(QObject *parent)
    : QObject(parent)
{
    m_updateTimer.setInterval(1000);
    connect(&m_updateTimer, &QTimer::timeout, this, &FrameContentTracker::update);
}

FrameContentTracker::~FrameContentTracker()
{ }

/*!
    \qmlproperty list<string> FrameContentTracker::roleNames
    \readonly

    Names of the roles provided by FrameContentTracker when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList FrameContentTracker::roleNames() const
{
    return { u"duplicateFrames"_s };
}

/*!
    \qmlmethod FrameContentTracker::update

    Updates the property duplicateFrames. Then resets the internal counters for the new time period
    starting from the moment this method is called.

    Note that you normally don't have to call this method directly, as FrameContentTracker does it
    automatically every \l interval milliseconds while \l running is set to \c true.

    \sa running
*/
void FrameContentTracker::update()
{
    m_duplicateFrames = m_sameFrameCounter.fetchAndStoreAcquire(0);
    emit updated();

}

/*!
    \qmlproperty bool FrameContentTracker::running

    If \c true, update() will get called automatically every \l interval milliseconds.

    When using FrameContentTracker as a MonitorModel data source, this property should be kept as
    \c false.

    \sa update() interval
*/
bool FrameContentTracker::running() const
{
    return m_updateTimer.isActive();
}

void FrameContentTracker::setRunning(bool running)
{
    if (running && !m_updateTimer.isActive()) {
        m_updateTimer.start();
        emit runningChanged();
    } else if (!running && m_updateTimer.isActive()) {
        m_updateTimer.stop();
        emit runningChanged();
    }
}

/*!
    \qmlproperty int FrameContentTracker::interval

    The interval, in milliseconds, between update() calls while \l running is \c true.

    \sa update() running
*/
int FrameContentTracker::interval() const
{
    return m_updateTimer.interval();
}

void FrameContentTracker::setInterval(int interval)
{
    if (interval != m_updateTimer.interval()) {
        m_updateTimer.setInterval(interval);
        emit intervalChanged();
    }
}

/*!
    \qmlproperty Object FrameContentTracker::window

    The window to be monitored, from which duplicate frames information will be gathered.
    It must be a real toplevel Window (from the \c QtQuick.Window module) or
    an ApplicationManagerWindow from a multi-process app. These however do \b not work:
    WindowObject (from the \c QtApplicationManager.SystemUI module) as well as
    ApplicationManagerWindow in single-process mode.

    \note Frames are being tracked as soon as this property is set to a valid window instance, even
          if \l running is set to \c false. Setting this property to \c null will stop the tracking.
*/
QObject *FrameContentTracker::window() const
{
    return m_window;
}

void FrameContentTracker::setWindow(QObject *window)
{
    if (m_window == window)
        return;

    if (m_frameAfterRenderingConnection) {
        disconnect(m_frameAfterRenderingConnection);
        m_frameAfterRenderingConnection = { };
    }

    m_window = window;
    m_lastFrame = { };
    m_sameFrameCounter = 0;

    if (m_window) {
        QQuickWindow *quickWindow = qobject_cast<QQuickWindow*>(m_window);
        if (auto *amWindow = qobject_cast<ApplicationManagerWindow *>(m_window)) {
            if (amWindow->isSingleProcess()) {
                qmlWarning(this) << "The content of an application's window in single-process mode"
                                    "cannot be tracked";
                return;
            }
            quickWindow = qobject_cast<QQuickWindow *>(amWindow->backingObject());
        }

        if (quickWindow) {
            m_frameAfterRenderingConnection = connect(quickWindow, &QQuickWindow::afterRendering,
                                                      this, [this, quickWindow]() {
                // we are on the render thread
                auto *wp = QQuickWindowPrivate::get(quickWindow);
                QImage img = QSGRhiSupport::instance()->grabAndBlockInCurrentFrame(
                    wp->rhi, wp->swapchain->currentFrameCommandBuffer());
                if (img == m_lastFrame)
                    ++m_sameFrameCounter;
                else
                    m_lastFrame = std::move(img);
            }, Qt::DirectConnection);
        }

        if (!m_frameAfterRenderingConnection)
            qmlWarning(this) << "The given window is not a QQuickWindow.";
    }

    emit windowChanged();
}

/*!
    \qmlproperty real FrameContentTracker::duplicateFrames
    \readonly

    The number of duplicate frames rendered for the given \l window, since update() was last called
    (either manually or automatically in case \l running is set to \c true).

    \sa window running update()
*/
int FrameContentTracker::duplicateFrames() const
{
    return m_duplicateFrames;
}

QT_END_NAMESPACE_AM

#include "moc_framecontenttracker.cpp"
