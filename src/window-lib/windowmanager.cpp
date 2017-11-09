/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

#include <QGuiApplication>
#include <QRegularExpression>
#include <QQuickView>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQmlEngine>
#include <QVariant>
#include <QTimer>
#include <QThread>
#include <private/qabstractanimation_p.h>

#if defined(AM_MULTI_PROCESS)
#  include "waylandcompositor.h"
#endif

#include "global.h"
#include "logging.h"
#include "application.h"
#include "applicationmanager.h"
#include "abstractruntime.h"
#include "runtimefactory.h"
#include "window.h"
#include "windowmanager.h"
#include "windowmanager_p.h"
#include "waylandwindow.h"
#include "inprocesswindow.h"
#include "qml-utilities.h"


/*!
    \qmltype WindowManager
    \inqmlmodule QtApplicationManager
    \brief The WindowManager singleton.

    The WindowManager singleton type is the window managing part of the application manager.
    It provides a QML API only.

    The type is derived from QAbstractListModel, and can be directly used as a model in window views.

    Each item in this model corresponds to an actual window surface. Note that a single
    application can have multiple surfaces; therefore, the \c applicationId role is not unique
    within this model.

    \target WindowManager Roles

    The following roles are available in this model:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c applicationId
        \li string
        \li The unique id of an application represented as a string. This can be used to look up
            information about the application in the ApplicationManager model.
    \row
        \li \c windowItem
        \li Item
        \li The QtQuick Item representing the window surface of the application - used to actually
            composite the window on the screen.
    \row
        \li \c isFullscreen
        \li bool
        \li A boolean value indicating whether the surface is being displayed in fullscreen mode.
    \row
        \li \c isMapped
        \li bool
        \li A boolean value indicating whether the surface is mapped (visible).
    \row
        \li \c isClosing
        \li bool
        \li A boolean value indicating whether the surface is currently closing.
            A surface is closing when the wayland surface is already destroyed, but the window is still
            available for showing an animation until releaseWindow() is called.
    \endtable

    \target Multi-process Wayland caveats

    \note Please be aware that Wayland is essentially an asynchronous IPC protocol, resulting in
    different local states in the client and server processes during state changes. A prime example
    for this is window property changes on the client side: in addition to being changed
    asynchronously on the server side, the windowPropertyChanged signal will not be emitted while
    the window object is not yet made available on the server side via the windowReady signal. All
    those changes are not lost however, but the last change before emitting the windowReady signal
    will be the initial state of the window object on the System-UI side.

    \target Minimal compositor

    After importing, the WindowManager singleton can be used as in the example below.
    It demonstrates how to implement a basic, fullscreen window compositor with support
    for window show and hide animations:

    \qml
    import QtQuick 2.0
    import QtApplicationManager 1.0

    // Simple solution for a full-screen setup
    Item {
        id: fullscreenView

        MouseArea {
            // Without this area, mouse events would propagate "through" the surfaces
            id: filterMouseEventsForWindowContainer
            anchors.fill: parent
            enabled: false
        }

        Item {
            id: windowContainer
            anchors.fill: parent
            state: "closed"

            function windowReadyHandler(index, window) {
                filterMouseEventsForWindowContainer.enabled = true
                windowContainer.state = ""
                windowContainer.windowItem = window
            }
            function windowClosingHandler(index, window) {
                if (window === windowContainer.windowItem) {
                    // Start close animation
                    windowContainer.state = "closed"
                } else {
                    // Immediately close anything not handled by this container
                    WindowManager.releasewindow(window)
                }
            }
            function windowLostHandler(index, window) {
                if (windowContainer.windowItem === window) {
                    windowContainer.windowItem = placeHolder
                }
            }
            Component.onCompleted: {
                WindowManager.windowReady.connect(windowReadyHandler)
                WindowManager.windowClosing.connect(windowClosingHandler)
                WindowManager.windowLost.connect(windowLostHandler)
            }

            property Item windowItem: placeHolder
            onWindowItemChanged: {
                windowItem.parent = windowContainer  // Always reset parent
            }

            Item {
                id: placeHolder;
            }

            // Use a different syntax for 'anchors.fill: parent' due to the volatile nature of windowItem
            Binding { target: windowContainer.windowItem; property: "x"; value: windowContainer.x }
            Binding { target: windowContainer.windowItem; property: "y"; value: windowContainer.y }
            Binding { target: windowContainer.windowItem; property: "width"; value: windowContainer.width }
            Binding { target: windowContainer.windowItem; property: "height"; value: windowContainer.height }

            transitions: [
                Transition {
                    to: "closed"
                    SequentialAnimation {
                        alwaysRunToEnd: true

                        // Closing animation declared here
                        // ...

                        ScriptAction {
                            script: {
                                windowContainer.windowItem.visible = false;
                                WindowManager.releaseWindow(windowContainer.windowItem);
                                filterMouseEventsForWindowContainer.enabled = false;
                            }
                        }
                    }
                },
                Transition {
                    from: "closed"
                    SequentialAnimation {
                        alwaysRunToEnd: true

                        // Opening animation declared here
                        // ...
                    }
                }
            ]
        }
    }
    \endqml

*/

/*!
    \qmlsignal WindowManager::windowReady(int index, Item window)

    This signal is emitted after a new \a window surface has been created. Most likely due
    to an application launch.

    For the convenience of the System-UI, this signal will provide you with both the QML \a window
    Item as well as the \a index of that window within the WindowManager model.

    More information about this window can be retrieved via the model \a index. Most often you
    need the owning application of the window, which can be achieved by:

    \badcode
    var appId = WindowManager.get(index).applicationId
    var app = ApplicationManager.application(appId)
    \endcode

    \note Please be aware that the windowReady signal is not emitted immediately when the client
          sets a window to visible. This is due to the \l
          {Multi-process Wayland caveats}{asynchronous nature} of the underlying Wayland protocol.
*/

/*!
    \qmlsignal WindowManager::windowClosing(int index, Item window)

    This signal is emitted when a \a window surface is unmapped.

    For the convenience of the System-UI, this signal will provide you with both the QML \a window
    Item as well as the \a index of that window within the WindowManager model.

    Either the client application closed the window, or it exited and all its Wayland surfaces got
    implicitly unmapped.
    When using the \c qml-inprocess runtime, this signal is also emitted when the \c close
    signal of the fake surface is triggered.

    The actual surface can still be used for animations as it is not deleted immediately
    after this signal is emitted.

    More information about this window can be retrieved via the model \a index.

    \sa windowLost()
*/

/*!
    \qmlsignal WindowManager::windowLost(int index, Item window)

    This signal is emitted when the \a window surface has been destroyed on the client side.

    For the convenience of the System-UI, this signal will provide you with both the QML \a window
    Item as well as the \a index of that window within the WindowManager model.

    If the surface was mapped, you will receive an implicit windowClosing signal before windowLost.

    \note It is mandatory to call releaseWindow() after the windowLost() signal has been received: all
          resources associated with the window surface will not be released automatically. The
          timing is up to the System-UI; calling releaseWindow() can be delayed in order to play a
          shutdown animation, but failing to call it will result in resource leaks.

    \note If the System-UI fails to call releaseWindow() within 2 seconds afer receiving the
          windowLost() signal, the application-manager will automatically call it to prevent
          resource leaks and deadlocks on shutdown.

    More information about this window can be retrieved via the model \a index.
*/

/*!
    \qmlsignal WindowManager::raiseApplicationWindow(string applicationId)

    This signal is emitted when an application start is triggered for the already running application
    identified by \a applicationId via the ApplicationManager.
*/

QT_BEGIN_NAMESPACE_AM

namespace {
enum Roles
{
    Id = Qt::UserRole + 1000,
    WindowItem,
    IsFullscreen,
    IsMapped,
    IsClosing
};
}

WindowManager *WindowManager::s_instance = nullptr;

WindowManager *WindowManager::createInstance(QQmlEngine *qmlEngine, const QString &waylandSocketName)
{
    if (s_instance)
        qFatal("WindowManager::createInstance() was called a second time.");

    qmlRegisterSingletonType<WindowManager>("QtApplicationManager", 1, 0, "WindowManager",
                                            &WindowManager::instanceForQml);

    return s_instance = new WindowManager(qmlEngine, waylandSocketName);
}

WindowManager *WindowManager::instance()
{
    if (!s_instance)
        qFatal("WindowManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *WindowManager::instanceForQml(QQmlEngine *qmlEngine, QJSEngine *)
{
    if (qmlEngine)
        retakeSingletonOwnershipFromQmlEngine(qmlEngine, instance());
    return instance();
}

void WindowManager::shutDown()
{
    d->shuttingDown = true;

    if (d->windows.isEmpty())
        QTimer::singleShot(0, this, &WindowManager::shutDownFinished);
}

/*!
    \qmlproperty bool WindowManager::runningOnDesktop
    \readonly

    Holds \c true if running on a classic desktop window manager (Windows, X11, or macOS),
    \c false otherwise.
*/
bool WindowManager::isRunningOnDesktop() const
{
#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    return true;
#else
    return qApp->platformName() == qSL("xcb");
#endif
}

/*!
    \qmlproperty bool WindowManager::slowAnimations

    Whether animations are in slow mode.

    It's false by default and might be initialized to true using the command line option
    \c --slow-animations.

    Also useful to check this value if you need to adjust timings for the slow animation
    mode.
*/
bool WindowManager::slowAnimations() const
{
    return d->slowAnimations;
}

void WindowManager::setSlowAnimations(bool slowAnimations)
{
    if (slowAnimations != d->slowAnimations) {
        d->slowAnimations = slowAnimations;

        for (auto view : d->views)
            updateViewSlowMode(view);

        // Update timer of the main, GUI, thread
        QUnifiedTimer::instance()->setSlowModeEnabled(d->slowAnimations);

        // For new applications to start with the correct value
        RuntimeFactory::instance()->setSlowAnimations(d->slowAnimations);

        // Update already running applications
        for (const Application *application : ApplicationManager::instance()->applications()) {
            auto runtime = application->currentRuntime();
            if (runtime)
                runtime->setSlowAnimations(d->slowAnimations);
        }

        qCDebug(LogSystem) << "WindowManager::slowAnimations =" << d->slowAnimations;

        emit slowAnimationsChanged(d->slowAnimations);
    }
}

void WindowManager::updateViewSlowMode(QQuickWindow *view)
{
    // QUnifiedTimer are thread-local. To also slow down animations running in the SG thread
    // we need to enable the slow mode in this timer as well.
    static QHash<QQuickWindow *, QMetaObject::Connection> conns;
    bool isSlow = d->slowAnimations;
    conns.insert(view, connect(view, &QQuickWindow::beforeRendering, this, [view, isSlow] {
        QMetaObject::Connection con = conns[view];
        if (con) {
#if defined(Q_CC_MSVC)
            qApp->disconnect(con); // MSVC2013 cannot call static member functions without capturing this
#else
            QObject::disconnect(con);
#endif
            QUnifiedTimer::instance()->setSlowModeEnabled(isSlow);
        }
    }, Qt::DirectConnection));
}

WindowManager::WindowManager(QQmlEngine *qmlEngine, const QString &waylandSocketName)
    : QAbstractListModel()
    , d(new WindowManagerPrivate())
{
#if defined(AM_MULTI_PROCESS)
    d->waylandSocketName = waylandSocketName;
#else
    Q_UNUSED(waylandSocketName)
#endif

    d->roleNames.insert(Id, "applicationId");
    d->roleNames.insert(WindowItem, "windowItem");
    d->roleNames.insert(IsFullscreen, "isFullscreen");
    d->roleNames.insert(IsMapped, "isMapped");
    d->roleNames.insert(IsClosing, "isClosing");

    d->watchdogEnabled = true;
    d->qmlEngine = qmlEngine;
}

WindowManager::~WindowManager()
{
#if defined(AM_MULTI_PROCESS)
    delete d->waylandCompositor;
#endif
    delete d;
}

void WindowManager::enableWatchdog(bool enable)
{
    d->watchdogEnabled = enable;
}

bool WindowManager::isWatchdogEnabled() const
{
    return d->watchdogEnabled;
}

QVector<Window *> WindowManager::windows() const
{
    return d->windows;
}

QVector<Window *> WindowManager::applicationWindows(const QString &appId) const
{
    QVector<Window *> appWindows;

    for (int i = 0; i < d->windows.size(); i++) {
        if (d->windows.at(i)->application() &&
                d->windows.at(i)->application()->id() == appId)
            appWindows.append(d->windows.value(i));
    }

    return appWindows;
}

int WindowManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->windows.count();
}

QVariant WindowManager::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return QVariant();

    const Window *win = d->windows.at(index.row());

    switch (role) {
    case Id:
        if (win->application()) {
            if (win->application()->nonAliased())
                return win->application()->nonAliased()->id();
            else
                return win->application()->id();
        } else {
            return QString();
        }
    case WindowItem:
        return QVariant::fromValue<QQuickItem*>(win->windowItem());
    case IsFullscreen: {
        //TODO: find a way to handle fullscreen transparently for wayland and in-process
        return false;
    }
    case IsMapped: {
        if (win->isInProcess()) {
            return !win->isClosing();
        } else {
#if defined(AM_MULTI_PROCESS)
            auto ww = qobject_cast<const WaylandWindow*>(win);
            if (ww && ww->surface() && ww->surface()->surface())
#  if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
                return ww->surface()->surface()->isMapped();
#  else
                return ww->surface()->surface()->hasContent();
#  endif
#endif
            return false;
        }
    }
    case IsClosing: return win->isClosing();
    }
    return QVariant();
}

QHash<int, QByteArray> WindowManager::roleNames() const
{
    return d->roleNames;
}

/*!
    \qmlproperty int WindowManager::count
    \readonly

    This property holds the number of applications available.
*/
int WindowManager::count() const
{
    return rowCount();
}

/*!
    \qmlmethod object WindowManager::get(int index) const

    Retrieves the model data at \a index as a JavaScript object. See the \l {WindowManager
    Roles}{role names} for the expected object fields.

    Returns an empty object if the specified \a index is invalid.
*/
QVariantMap WindowManager::get(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogGraphics) << "invalid index:" << index;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(qL1S(it.value()), data(QAbstractListModel::index(index), it.key()));
    return map;
}

/*!
    \qmlmethod int WindowManager::indexOfWindow(Item window)

    Returns the index of the \a window within the WindowManager model, or \c -1 if the window item is
    not a managed window.
 */
int WindowManager::indexOfWindow(QQuickItem *window)
{
    return d->findWindowBySurfaceItem(window);
}

void WindowManager::setupInProcessRuntime(AbstractRuntime *runtime)
{
    // special hacks to get in-process mode working transparently
    if (runtime->manager()->inProcess()) {
        runtime->setInProcessQmlEngine(d->qmlEngine);

        connect(runtime, &AbstractRuntime::inProcessSurfaceItemFullscreenChanging,
                this, &WindowManager::surfaceFullscreenChanged, Qt::QueuedConnection);
        connect(runtime, &AbstractRuntime::inProcessSurfaceItemReady,
                this, static_cast<void (WindowManager::*)(QQuickItem *)>(&WindowManager::inProcessSurfaceItemCreated), Qt::QueuedConnection);
        connect(runtime, &AbstractRuntime::inProcessSurfaceItemClosing,
                this, static_cast<void (WindowManager::*)(QQuickItem *)>(&WindowManager::inProcessSurfaceItemClosing), Qt::QueuedConnection);
    }
}

/*!
    \qmlmethod WindowManager::releaseWindow(Item window)

    Releases all resources of the \a window surface and removes the window from the model.

    \note It is mandatory to call this function after the windowLost() signal has been received: all
          resources associated with the window surface will not be released automatically. The
          timing is up to the System-UI; calling releaseWindow() can be delayed in order to play a
          shutdown animation, but failing to call it will result in resource leaks.

    \note If the System-UI fails to call releaseWindow() within 2 seconds afer receiving the
          windowLost() signal, the application-manager will automatically call it to prevent
          resource leaks and deadlocks on shutdown.
    \sa windowLost
*/

void WindowManager::releaseWindow(QQuickItem *window)
{
    int index = d->findWindowBySurfaceItem(window);
    if (index == -1) {
        qCWarning(LogGraphics) << "releaseWindow was called with an invalid window pointer" << window;
        return;
    }
    Window *win = d->windows.at(index);
    if (!win)
        return;

    beginRemoveRows(QModelIndex(), index, index);
    d->windows.removeAt(index);
    endRemoveRows();

    win->deleteLater();

    if (d->shuttingDown && (count() == 0))
        QTimer::singleShot(0, this, &WindowManager::shutDownFinished);
}

/*!
    \qmlmethod WindowManager::registerCompositorView(QQuickWindow *view)

    Registers the given \a view as a possible destination for Wayland window composition.

    Wayland window items can only be rendered within top-level windows that have been registered
    via this function.

    \note The \a view parameter is an actual top-level window on the server side - not a client
          application window item.
*/
void WindowManager::registerCompositorView(QQuickWindow *view)
{
    static bool once = false;

    d->views << view;

    updateViewSlowMode(view);

#if defined(AM_MULTI_PROCESS)
    if (!ApplicationManager::instance()->isSingleProcess()) {
        if (!d->waylandCompositor) {
            d->waylandCompositor = new WaylandCompositor(view, d->waylandSocketName, this);
            // export the actual socket name for our child processes.
            qputenv("WAYLAND_DISPLAY", d->waylandCompositor->socketName());
            qCDebug(LogGraphics).nospace() << "WindowManager: running in Wayland mode [socket: "
                                          << d->waylandCompositor->socketName() << "]";
        } else {
            d->waylandCompositor->registerOutputWindow(view);
        }
    } else {
        if (!once)
            qCDebug(LogGraphics) << "WindowManager: running in single-process mode [forced at run-time]";
    }
#else
    if (!once)
        qCDebug(LogGraphics) << "WindowManager: running in single-process mode [forced at compile-time]";
#endif
    emit compositorViewRegistered(view);

    if (!once)
        once = true;
}

void WindowManager::surfaceFullscreenChanged(QQuickItem *surfaceItem, bool isFullscreen)
{
    Q_ASSERT(surfaceItem);

    int index = d->findWindowBySurfaceItem(surfaceItem);

    if (index == -1) {
        qCWarning(LogGraphics) << "could not find an application window for surfaceItem";
        return;
    }

    QModelIndex modelIndex = QAbstractListModel::index(index);
    qCDebug(LogGraphics) << "emitting dataChanged, index: " << modelIndex.row() << ", isFullScreen: " << isFullscreen;
    emit dataChanged(modelIndex, modelIndex, QVector<int>() << IsFullscreen);
}

/*! \internal
    Only used for in-process surfaces
 */
void WindowManager::inProcessSurfaceItemCreated(QQuickItem *surfaceItem)
{
    AbstractRuntime *rt = qobject_cast<AbstractRuntime *>(sender());
    if (!rt) {
        qCCritical(LogSystem) << "This function must be called by a signal of Runtime!";
        return;
    }
    const Application *app = rt->application();
    if (app->nonAliased())
        app = app->nonAliased();
    if (!app) {
        qCCritical(LogSystem) << "This function must be called by a signal of Runtime which actually has an application attached!";
        return;
    }

    //Only create a new Window if we don't have it already in the window list, as the user controls whether windows are removed or not
    int index = d->findWindowBySurfaceItem(surfaceItem);
    if (index == -1) {
        setupWindow(new InProcessWindow(app, surfaceItem));
    } else {
        QModelIndex modelIndex = QAbstractListModel::index(index);
        qCDebug(LogGraphics) << "emitting dataChanged, index: " << modelIndex.row() << ", isMapped: true";
        emit dataChanged(modelIndex, modelIndex, QVector<int>() << IsMapped);
        emit windowReady(index, d->windows.at(index)->windowItem());
    }
}

void WindowManager::inProcessSurfaceItemClosing(QQuickItem *surfaceItem)
{
    qCDebug(LogGraphics) << "inProcessSurfaceItemClosing" << surfaceItem;

    int index = d->findWindowBySurfaceItem(surfaceItem);
    if (index == -1) {
        qCWarning(LogGraphics) << "inProcessSurfaceItemClosing: could not find an application window for item" << surfaceItem;
        return;
    }
    InProcessWindow *win = qobject_cast<InProcessWindow *>(d->windows.at(index));
    if (!win) {
        qCCritical(LogGraphics) << "inProcessSurfaceItemClosing: expected surfaceItem to be a InProcessWindow, got" << d->windows.at(index);
        return;
    }

    win->setClosing();

    QModelIndex modelIndex = QAbstractListModel::index(index);
    qCDebug(LogGraphics) << "emitting dataChanged, index: " << modelIndex.row() << ", isMapped: false";
    emit dataChanged(modelIndex, modelIndex, QVector<int>() << IsMapped);

    emit windowClosing(index, win->windowItem()); //TODO: rename to windowUnmapped

    //emit destroyed as well, so the compositor knows that the closing transition can be played now and the window be freed
    emit windowLost(index, win->windowItem());
}

/*! \internal
    Used to create the Window objects for a surface
    This is called for both wayland and in-process surfaces
*/
void WindowManager::setupWindow(Window *window)
{
    if (!window)
        return;

    connect(window, &Window::windowPropertyChanged,
            this, [this](const QString &name, const QVariant &value) {
        if (Window *win = qobject_cast<Window *>(sender()))
            emit windowPropertyChanged(win->windowItem(), name, value);
    });

    beginInsertRows(QModelIndex(), d->windows.count(), d->windows.count());
    d->windows << window;
    endInsertRows();

    emit windowReady(d->windows.count() - 1, window->windowItem());
}


#if defined(AM_MULTI_PROCESS)

void WindowManager::waylandSurfaceCreated(WindowSurface *surface)
{
    Q_UNUSED(surface)
    // this function is still useful for Wayland debugging
    //qCDebug(LogGraphics) << "New Wayland surface:" << surface->surface() << "pid:" << surface->processId();
}

void WindowManager::waylandSurfaceMapped(WindowSurface *surface)
{
    qint64 processId = surface->processId();
    const Application *app = ApplicationManager::instance()->fromProcessId(processId);

    if (!app && ApplicationManager::instance()->securityChecksEnabled()) {
        qCCritical(LogGraphics) << "SECURITY ALERT: an unknown application with pid" << processId
                               << "tried to map a Wayland surface!";
        return;
    }

    Q_ASSERT(surface);
    Q_ASSERT(surface->item());

    qCDebug(LogGraphics) << "Mapping Wayland surface" << surface->item() << "of" << d->applicationId(app, surface);

    // Only create a new Window if we don't have it already in the window list, as the user controls
    // whether windows are removed or not
    int index = d->findWindowByWaylandSurface(surface->surface());
    if (index == -1) {
        WaylandWindow *w = new WaylandWindow(app, surface);
        setupWindow(w);
    } else {
        QModelIndex modelIndex = QAbstractListModel::index(index);
        emit dataChanged(modelIndex, modelIndex, QVector<int>() << IsMapped);
        emit windowReady(index, d->windows.at(index)->windowItem());
    }

    // switch on Wayland ping/pong -- currently disabled, since it is a bit unstable
    //if (d->watchdogEnabled)
    //    w->enablePing(true);

    if (app) {
        //We only take focus for applications.
        surface->takeFocus(); // otherwise we will never get keyboard focus in the client
    }
}

void WindowManager::waylandSurfaceUnmapped(WindowSurface *surface)
{
    int index = d->findWindowByWaylandSurface(surface->surface());

    if (index == -1) {
        qCWarning(LogGraphics) << "Unmapping a surface failed, because no application window is "
                                 "registered for Wayland surface" << surface->item();
        return;
    }
    WaylandWindow *win = qobject_cast<WaylandWindow *>(d->windows.at(index));
    if (!win)
        return;

    qCDebug(LogGraphics) << "Unmapping Wayland surface" << surface->item() << "of"
                        << d->applicationId(win->application(), surface);

    // switch off Wayland ping/pong
    if (d->watchdogEnabled)
        win->enablePing(false);

    QModelIndex modelIndex = QAbstractListModel::index(index);
    emit dataChanged(modelIndex, modelIndex, QVector<int>() << IsMapped);

    emit windowClosing(index, win->windowItem()); //TODO: rename to windowUnmapped
}

void WindowManager::waylandSurfaceDestroyed(WindowSurface *surface)
{
    int index = d->findWindowByWaylandSurface(surface->surface());
    if (index == -1) {
        // this is a surface that was only created, but never mapped - just ignore it
        return;
    }
    WaylandWindow *win = qobject_cast<WaylandWindow *>(d->windows.at(index));
    if (!win)
        return;

    qCDebug(LogGraphics) << "Destroying Wayland surface" << (surface ? surface->item() : nullptr)
                        << "of" << d->applicationId(win->application(), surface);

    win->setClosing();

    emit windowLost(index, win->windowItem()); //TODO: rename to windowDestroyed

    // Just to safe-guard against the system-ui not releasing windows. This could lead to severe
    // leaks in the graphics stack and it will also prevent clean shutdowns of the appman process.
    int timeout = 2000;
    if (slowAnimations())
        timeout *= 5; // QUnifiedTimer has no getter for this...
    QTimer::singleShot(timeout, win, [this, win]() { releaseWindow(win->windowItem()); });
}

#endif // defined(AM_MULTI_PROCESS)


/*!
    \qmlmethod bool WindowManager::setWindowProperty(Item window, string name, var value)

    Sets an application \a window's shared property identified by \a name to the given \a value.

    These properties are shared between the System-UI and the client application: in single-process
    mode simply via a QVariantMap; in multi-process mode the sharing is done via Qt's extended
    surface Wayland extension. Changes from the client side are notified by the
    windowPropertyChanged() signal.

    See ApplicationManagerWindow for the client side API.

    \sa windowProperty(), windowProperties(), windowPropertyChanged()
*/
bool WindowManager::setWindowProperty(QQuickItem *window, const QString &name, const QVariant &value)
{
    int index = d->findWindowBySurfaceItem(window);

    if (index < 0)
        return false;

    Window *win = d->windows.at(index);
    return win->setWindowProperty(name, value);
}

/*!
    \qmlmethod var WindowManager::windowProperty(Item window, string name)

    Returns the value of an application \a window's shared property identified by \a name.

    \sa setWindowProperty()
*/
QVariant WindowManager::windowProperty(QQuickItem *window, const QString &name) const
{
    int index = d->findWindowBySurfaceItem(window);

    if (index < 0)
        return QVariant();

    const Window *win = d->windows.at(index);
    return win->windowProperty(name);
}

/*!
    \qmlmethod var WindowManager::windowProperties(Item window)

    Returns an object containing all shared properties of an application \a window.

    \sa setWindowProperty()
*/
QVariantMap WindowManager::windowProperties(QQuickItem *window) const
{
    int index = d->findWindowBySurfaceItem(window);

    if (index < 0)
        return QVariantMap();

    Window *win = d->windows.at(index);
    return win->windowProperties();
}

/*!
    \qmlsignal WindowManager::windowPropertyChanged(Item window, string name, var value)

    Reports a change of an application \a window's property identified by \a name to the given
    \a value.

    \note When listening to property changes of Wayland clients, be aware of the \l
          {Multi-process Wayland caveats}{asynchronous nature} of the underlying Wayland protocol.

    \sa ApplicationManagerWindow::setWindowProperty()
*/

/*!
    \qmlmethod bool WindowManager::makeScreenshot(string filename, string selector)

    Creates one or several screenshots depending on the \a selector, saving them to the files
    specified by \a filename.

    The \a filename argument can either be a plain file name for single screenshots, or it can
    contain format sequences that will be replaced accordingly, if multiple screenshots are
    requested:

    \table
    \header
        \li Format
        \li Description
    \row
        \li \c{%s}
        \li Will be replaced with the screen-id of this particular screenshot.
    \row
        \li \c{%i}
        \li Will be replaced with the application id, when making screenshots of application
            windows.
    \row
        \li \c{%%}
        \li Will be replaced by a single \c{%} character.
    \endtable

    The \a selector argument is a string which is parsed according to this pattern:
    \badcode
    <application-id>[window-property=value]:<screen-id>
    \endcode

    All parts are optional, so if you specify an empty string, the call will create a screenshot
    of every screen.
    If you specify an \c application-id (which can also contain wildcards for matching multiple
    applications), a screenshot will be made for each window of this (these) application(s).
    If only specific windows of one or more applications should be used to create screenshots, you
    can specify a \c window-property selector, which will only select windows that have a matching
    WindowManager::windowProperty.
    Adding a \c screen-id will restrict the creation of screenshots to the the specified screen.

    Here is an example, creating screenshots of all windows on the second screen, that have the
    window-property \c type set to \c cluster and written by Pelagicore:
    \badcode
    com.pelagicore.*[type=cluster]:1
    \endcode

    Returns \c true on success and \c false otherwise.

    \note This call will be handled asynchronously, so even a positive return value does not mean
          that all screenshot images have been created already.
*/
//TODO: either change return value to list of to-be-created filenames or add a 'finished' signal
bool WindowManager::makeScreenshot(const QString &filename, const QString &selector)
{
    // filename:
    // %s -> screenId
    // %i -> appId
    // %% -> %

    // selector:
    // <appid>[attribute=value]:<screenid>
    // e.g. com.pelagicore.music[windowType=widget]:1
    //      com.pelagicore.*[windowType=]

    auto substituteFilename = [filename](const QString &screenId, const QString &appId) -> QString {
        QString result;
        bool percent = false;
        for (int i = 0; i < filename.size(); ++i) {
            QChar c = filename.at(i);
            if (!percent) {
                if (c != QLatin1Char('%'))
                    result.append(c);
                else
                    percent = true;
            } else {
                switch (c.unicode()) {
                case '%':
                    result.append(c);
                    break;
                case 's':
                    result.append(screenId);
                    break;
                case 'i':
                    result.append(appId);
                    break;
                }
                percent = false;
            }
        }
        return result;
    };

    QRegularExpression re(QStringLiteral("^([a-z.-]+)?(\\[([a-zA-Z0-9_.]+)=([^\\]]*)\\])?(:([0-9]+))?"));
    auto match = re.match(selector);
    QString screenId = match.captured(6);
    QString appId = match.captured(1);
    QString attributeName = match.captured(3);
    QString attributeValue = match.captured(4);

    // qWarning() << "Matching result:" << match.isValid();
    // qWarning() << "  screen ...... :" << screenId;
    // qWarning() << "  app ......... :" << appId;
    // qWarning() << "  attributeName :" << attributeName;
    // qWarning() << "  attributeValue:" << attributeValue;

    bool result = true;
    bool foundAtLeastOne = false;

    if (appId.isEmpty() && attributeName.isEmpty()) {
        // fullscreen screenshot

        for (int i = 0; i < d->views.count(); ++i) {
            if (screenId.isEmpty() || screenId.toInt() == i) {
                QImage img = d->views.at(i)->grabWindow();

                foundAtLeastOne = true;
                result &= img.save(substituteFilename(QString::number(i), QString()));
            }
        }
    } else {
        // app without System-UI

        // filter out alias and apps not matching appId (if set)
        QVector<const Application *> apps = ApplicationManager::instance()->applications();
        auto it = std::remove_if(apps.begin(), apps.end(), [appId](const Application *app) {
            return app->isAlias() || (!appId.isEmpty() && (appId != app->id()));
        });
        apps.erase(it, apps.end());

        auto grabbers = new QList<QSharedPointer<QQuickItemGrabResult>>;

        for (const Window *w : qAsConst(d->windows)) {
            if (apps.contains(w->application())) {
                if (attributeName.isEmpty()
                        || (w->windowProperty(attributeName).toString() == attributeValue)) {
                    for (int i = 0; i < d->views.count(); ++i) {
                        if (screenId.isEmpty() || screenId.toInt() == i) {
                            QQuickWindow *view = d->views.at(i);

                            bool onScreen = false;

                            if (w->isInProcess()) {
                                onScreen = (w->windowItem()->window() == view);
                            }
#if defined(AM_MULTI_PROCESS)
                            else if (const WaylandWindow *wlw = qobject_cast<const WaylandWindow *>(w)) {
                                onScreen = wlw->surface() && (wlw->surface()->outputWindow() == view);
                            }
#endif
                            if (onScreen) {
                                foundAtLeastOne = true;
                                QSharedPointer<QQuickItemGrabResult> grabber = w->windowItem()->grabToImage();

                                if (!grabber) {
                                    result = false;
                                    continue;
                                }

                                QString saveTo = substituteFilename(QString::number(i), w->application()->id());
                                grabbers->append(grabber);
                                connect(grabber.data(), &QQuickItemGrabResult::ready, this, [grabbers, grabber, saveTo]() {
                                    grabber->saveToFile(saveTo);
                                    grabbers->removeOne(grabber);
                                    if (grabbers->isEmpty())
                                        delete grabbers;
                                });

                            }
                        }
                    }
                }
            }
        }
    }
    return foundAtLeastOne && result;
}


int WindowManagerPrivate::findWindowByApplication(const Application *app) const
{
    for (int i = 0; i < windows.count(); ++i) {
        if (app == windows.at(i)->application())
            return i;
        if (app->nonAliased() && app->nonAliased() == windows.at(i)->application())
            return i;
    }
    return -1;
}

int WindowManagerPrivate::findWindowBySurfaceItem(QQuickItem *quickItem) const
{
    for (int i = 0; i < windows.count(); ++i) {
        if (quickItem == windows.at(i)->windowItem())
            return i;
    }
    return -1;
}

QList<QQuickWindow *> WindowManager::compositorViews() const
{
    return d->views;
}

#if defined(AM_MULTI_PROCESS)

int WindowManagerPrivate::findWindowByWaylandSurface(QWaylandSurface *waylandSurface) const
{
    for (int i = 0; i < windows.count(); ++i) {
        if (!windows.at(i)->isInProcess() && (waylandSurface == waylandCompositor->waylandSurfaceFromItem(windows.at(i)->windowItem())))
            return i;
    }
    return -1;
}

QString WindowManagerPrivate::applicationId(const Application *app, WindowSurface *windowSurface)
{
    if (app)
        return app->id();
    else if (windowSurface && windowSurface->surface() && windowSurface->surface()->client())
        return qSL("pid: ") + QString::number(windowSurface->surface()->client()->processId());
    else
        return qSL("<unknown client>");
}

#endif // defined(AM_MULTI_PROCESS)

QT_END_NAMESPACE_AM
