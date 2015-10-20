/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QCoreApplication>
#include <QtQml>
#include <QQuickView>
#include <QUrl>
#include <QQmlEngine>
#include <QLocale>
#include <QVariant>
#include <QQuickItem>

#include "global.h"

#include "application.h"
#include "applicationmanager.h"
#include "abstractruntime.h"
#include "window.h"
#include "windowmanager.h"
#include "waylandwindow.h"
#include "inprocesswindow.h"

/*!
    \class WindowManager
    \internal
*/

/*!
    \qmltype WindowManager
    \inqmlmodule com.pelagicore.ApplicationManager 0.1
    \brief The WindowManager singleton

    This singleton class is the window managing part of the application manager. It provides a QML
    API only.

    To make QML programmers lifes easier, the class is derived from \c QAbstractListModel,
    so you can directly use this singleton as a model in your window views.

    Each item in this model corresponds to an actual window surface. Please not that a single
    application can have multiple surfaces: the \c applicationId role is not unique within this model!

    \keyword WindowManager Roles

    The following roles are available in this model:

    \table
    \header
        \li Role name
        \li Type
        \li Description
    \row
        \li \c applicationId
        \li \c string
        \li The unique id of an application represented as a string in reverse-dns form (e.g.
            \c com.pelagicore.foo). This can be used to look up information about the application
            in the ApplicationManager model.
    \row
        \li \c surfaceItem
        \li \c Item
        \li The window surface of the application - used to composite the windows on the screen.
    \row
        \li \c isFullscreen
        \li \c bool
        \li A boolean value telling if the app's surface is being displayed in fullscreen mode.
    \endtable

    The QML import for this singleton is

    \c{import com.pelagicore.ApplicationManager 0.1}

    After importing, you can just use the WindowManager singleton as shown in the example below.
    It shows how to implement a basic fullscreen window compositor that already supports window
    show and hide animations:

    \qml
    import QtQuick 2.0
    import com.pelagicore.ApplicationManager 0.1

    // simple solution for a full-screen setup
    Item {
        id: fullscreenView

        MouseArea {
            // without this area, clicks would go "through" the surfaces
            id: filterMouseEventsForWindowContainer
            anchors.fill: parent
            enabled: false
        }

        Item {
            id: windowContainer
            anchors.fill: parent
            state: "closed"

            function surfaceItemReadyHandler(index, item) {
                filterMouseEventsForWindowContainer.enabled = true
                windowContainer.state = ""
                windowContainer.windowItem = item
                windowContainer.windowItemIndex = index
            }
            function surfaceItemClosingHandler(index, item) {
                if (item === windowContainer.windowItem) {
                    // start close animation
                    windowContainer.state = "closed"
                } else {
                    // immediately close anything which is not handled by this container
                    WindowManager.releaseSurfaceItem(index, item)
                }
            }
            function surfaceItemLostHandler(index, item) {
                if (windowContainer.windowItem === item) {
                    windowContainer.windowItemIndex = -1
                    windowContainer.windowItem = placeHolder
                }
            }
            Component.onCompleted: {
                WindowManager.surfaceItemReady.connect(surfaceItemReadyHandler)
                WindowManager.surfaceItemClosing.connect(surfaceItemClosingHandler)
                WindowManager.surfaceItemLost.connect(surfaceItemLostHandler)
            }

            property int windowItemIndex: -1
            property Item windowItem: placeHolder
            onWindowItemChanged: {
                windowItem.parent = windowContainer  // reset parent in any case
            }

            Item {
                id: placeHolder;
            }

            // a different syntax for 'anchors.fill: parent' due to the volatile nature of windowItem
            Binding { target: windowContainer.windowItem; property: "x"; value: windowContainer.x }
            Binding { target: windowContainer.windowItem; property: "y"; value: windowContainer.y }
            Binding { target: windowContainer.windowItem; property: "width"; value: windowContainer.width }
            Binding { target: windowContainer.windowItem; property: "height"; value: windowContainer.height }

            transitions: [
                Transition {
                    to: "closed"
                    SequentialAnimation {
                        alwaysRunToEnd: true

                        // your closing animation goes here
                        // ...

                        ScriptAction {
                            script: {
                                windowContainer.windowItem.visible = false;
                                WindowManager.releaseSurfaceItem(windowContainer.windowItemIndex, windowContainer.windowItem);
                                filterMouseEventsForWindowContainer.enabled = false;
                            }
                        }
                    }
                },
                Transition {
                    from: "closed"
                    SequentialAnimation {
                        alwaysRunToEnd: true

                        // your opening animation goes here
                        // ...
                    }
                }
            ]
        }
    }
    \endqml

*/

/*!
    \qmlproperty int WindowManager::count

    This property holds the number of applications available.
*/

/*!
    \qmlsignal WindowManager::surfaceItemReady(int index, Item item)

    This signal is emitted after a new surface \a item (a window) has been created. Most likely due
    to an application launch.

    The corresponding application can be retrieved via the model \a index.
*/

/*!
    \qmlsignal WindowManager::surfaceItemClosing(int index, Item item)

    This signal is emitted when the application is about to close. A process exited - either by
    exiting normally or by crashing - and the corresponding Wayland surface \a item got unmapped.
    When using the \c qml-inprocess runtime this signal will also be emitted when the \c close
    signal of the fake surface is triggered.

    The actual surface can still be used for animations, since it will not be deleted right
    after this signal is emitted.

    The QML code is required to answer this signal by calling releaseSurfaceItem as soon as the QML
    animations on this surface are finished. Neglecting to do so will result in resource leaks!

    The corresponding application can be retrieved via the model \a index.

    \sa surfaceItemLost
*/

/*!
    \qmlsignal WindowManager::surfaceItemLost(int index, Item item)

    After this signal has been fired, the surface \a item is not usable anymore. This should only
    happen after releaseSurfaceItem has been called.

    The corresponding application can be retrieved via the model \a index.
*/


namespace {
enum Roles
{
    Id = Qt::UserRole + 1000,
    SurfaceItem,
    IsFullscreen
};
}

class WindowManagerPrivate
{
public:
    int findWindowByApplication(const Application *app) const;
    int findWindowBySurfaceItem(QQuickItem *quickItem) const;
#ifndef AM_SINGLEPROCESS_MODE
    int findWindowByWaylandSurface(QWaylandSurface *waylandSurface) const;

    QWaylandSurface *waylandSurfaceFromItem(QQuickItem *surfaceItem) const;
#endif
    QHash<int, QByteArray> roleNames;
    QList<Window *> windows;
    QMap<const Window *, bool> isClosing;

    bool watchdogEnabled;
};

WindowManager *WindowManager::s_instance = 0;


WindowManager *WindowManager::createInstance(QQuickView *view)
{
    if (s_instance)
        qFatal("WindowManager::createInstance() was called a second time.");
    return s_instance = new WindowManager(view);
}

WindowManager *WindowManager::instance()
{
    if (!s_instance)
        qFatal("WindowManager::instance() was called before createInstance().");
    return s_instance;
}

WindowManager::WindowManager(QQuickView *parent)
    : QAbstractListModel(parent)
#ifndef AM_SINGLEPROCESS_MODE
#  if QT_VERSION < QT_VERSION_CHECK(5,3,0)
    , QWaylandCompositor(parent)
#  elif QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    , QWaylandQuickCompositor(0, DefaultExtensions | SubSurfaceExtension)
#  else
      , QWaylandQuickCompositor(parent, 0, DefaultExtensions | SubSurfaceExtension)
#  endif
#endif
    , d(new WindowManagerPrivate())
{
#ifndef AM_SINGLEPROCESS_MODE
#  if QT_VERSION < QT_VERSION_CHECK(5,3,0)
    qputenv("QT_COMPOSITOR_NEGATE_INVERTED_Y", "1");
#  else
    #  if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
        createOutput(parent, QString(), QString());
    #  endif
    parent->winId();
    addDefaultShell();
#endif

    connect(parent, &QQuickWindow::beforeSynchronizing, this, [this](){this->frameStarted();}, Qt::DirectConnection);
    connect(parent, &QQuickWindow::afterRendering, this, &WindowManager::sendCallbacks);

    connect(parent, &QWindow::heightChanged, this, &WindowManager::resize);
    connect(parent, &QWindow::widthChanged, this, &WindowManager::resize);
    setOutputGeometry(parent->geometry());
#endif


    d->roleNames.insert(Id, "applicationId");
    d->roleNames.insert(SurfaceItem, "surfaceItem");
    d->roleNames.insert(IsFullscreen, "isFullscreen");

    d->watchdogEnabled = true;
}

WindowManager::~WindowManager()
{
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
        return win->application() ? win->application()->id() : QString();
    case SurfaceItem:
        return QVariant::fromValue<QQuickItem*>(win->surfaceItem());
    case IsFullscreen: {
        //TODO: find a way to handle fullscreen transparently for wayland and in-process
        return false;
    }
    }
    return QVariant();
}

QHash<int, QByteArray> WindowManager::roleNames() const
{
    return d->roleNames;
}

/*!
    \qmlmethod object WindowManager::get(int row) const

    Retrieves the model data at \a row as a JavaScript object. Please see the \l {WindowManager
    Roles}{role names} for the expected object fields.

    Will return an empty object, if the specified \a row is invalid.
*/
QVariantMap WindowManager::get(int row) const
{
    if (row < 0 || row >= count()) {
        qCWarning(LogWayland) << "invalid row:" << row;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(it.value(), data(index(row), it.key()));
    return map;
}

void WindowManager::setupInProcessRuntime(AbstractRuntime *runtime)
{
    // special hacks to get in-process mode working transparently
    if (runtime->inProcess()) {
        QQuickView *qv = qobject_cast<QQuickView*>(QObject::parent());

        runtime->setInProcessQmlEngine(qv->engine());

        connect(runtime, &AbstractRuntime::inProcessSurfaceItemFullscreenChanging,
                this, &WindowManager::surfaceFullscreenChanged, Qt::QueuedConnection);
        connect(runtime, &AbstractRuntime::inProcessSurfaceItemReady,
                this, static_cast<void (WindowManager::*)(QQuickItem *)>(&WindowManager::inProcessSurfaceItemCreated), Qt::QueuedConnection);
        connect(runtime, &AbstractRuntime::inProcessSurfaceItemClosing,
                this, &WindowManager::surfaceItemAboutToClose, Qt::QueuedConnection);
    }
}

/*!
    \qmlmethod WindowManager::releaseSurfaceItem(int index, Item item)

    Releases the surface \a item for the application at \a index. After cleanup is complete, the
    signal surfaceItemLost will be fired.
*/

void WindowManager::releaseSurfaceItem(int index, QQuickItem* item)
{
    Q_UNUSED(index);
    item->deleteLater();
    surfaceItemDestroyed(item);
}

void WindowManager::surfaceFullscreenChanged(QQuickItem *surfaceItem, bool isFullscreen)
{
    Q_ASSERT(surfaceItem);

    int index = d->findWindowBySurfaceItem(surfaceItem);

    if (index == -1) {
        qCWarning(LogWayland) << "could not find an application window for surfaceItem";
        return;
    }

    QModelIndex modelIndex = QAbstractListModel::index(index);
    qCDebug(LogWayland) << "emitting dataChanged, index: " << modelIndex.row() << ", isFullScreen: " << isFullscreen;
    emit dataChanged(modelIndex, modelIndex, QVector<int>() << IsFullscreen);
}

/*! /internal
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
    if (!app) {
        qCCritical(LogSystem) << "This function must be called by a signal of Runtime which actually has an application attached!";
        return;
    }

    setupWindow(new InProcessWindow(app, surfaceItem));
}


/*! /internal
    Used to create the Window objects for a surface
    This is called for both wayland and in-process surfaces
 */
void WindowManager::setupWindow(Window *window)
{
    if (!window)
        return;

    connect(window, &Window::windowPropertyChanged,
            this, &WindowManager::windowPropertyChanged);

    beginInsertRows(QModelIndex(), d->windows.count(), d->windows.count());
    d->windows << window;
    endInsertRows();

    emit surfaceItemReady(d->windows.count() - 1, window->surfaceItem());
}

void WindowManager::surfaceItemDestroyed(QQuickItem* item)
{
    // item may already be deleted at this point!

    int index = d->findWindowBySurfaceItem(item);

    if (index > -1) {
        emit surfaceItemLost(index, item);

        beginRemoveRows(QModelIndex(), index, index);
        d->windows.removeAt(index);
        endRemoveRows();
    } else {
        qCCritical(LogSystem) << "ERROR: check why this code path has been entered! If that's ok, remove/change this debug output";
    }
}

void WindowManager::surfaceItemAboutToClose(QQuickItem *item)
{
    int index = d->findWindowBySurfaceItem(item);

    if (index == -1)
        qCWarning(LogSystem) << "could not find application for surfaceItem";
    emit surfaceItemClosing(index, item);  // is it really better to emit the signal with index==-1 then doing nothing...?
}


#ifndef AM_SINGLEPROCESS_MODE

void WindowManager::sendCallbacks()
{
    if (!d->windows.isEmpty()) {
        QList<QWaylandSurface *> listToSend;

        // TODO: optimize! no need to send this to hidden/minimized/offscreen/etc. surfaces
        foreach (const Window *win, d->windows) {
            if (!d->isClosing.value(win) && !win->isInProcess()) {
                if (QWaylandSurface *surface = d->waylandSurfaceFromItem(win->surfaceItem())) {
                    listToSend << surface;
                }
            }
        }

        if (!listToSend.isEmpty()) {
            //qCDebug(LogWayland) << "sending callbacks to clients: " << listToSend; // note: debug-level 6 needs to be enabled manually...
            sendFrameCallbacks(listToSend);
        }
    }
}

void WindowManager::resize()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
    setOutputGeometry(window()->geometry());
#endif
}

void WindowManager::surfaceCreated(QWaylandSurface *surface)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    qCDebug(LogWayland) << "waylandSurfaceCreate" << surface << "(PID:" << surface->client()->processId() << ")" << surface->windowProperties();
#else
    qCDebug(LogWayland) << "waylandSurfaceCreate" << surface << "(PID:" << surface->processId() << ")" << surface->windowProperties();
#endif

    connect(surface, &QWaylandSurface::mapped, this, &WindowManager::waylandSurfaceMapped);
    connect(surface, &QWaylandSurface::unmapped, this, &WindowManager::waylandSurfaceUnmapped);
    connect(surface, &QWaylandSurface::surfaceDestroyed, this, &WindowManager::waylandSurfaceDestroyed);
}

void WindowManager::waylandSurfaceMapped()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
#else
    QWaylandQuickSurface *surface = qobject_cast<QWaylandQuickSurface *>(sender());
#endif
    qCDebug(LogWayland) << "waylandSurfaceMapped" << surface;

    Q_ASSERT(surface != 0);

#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    if (surface->client()->processId() == 0) {
#else
    if (surface->processId() == 0) {
#endif
        // TODO find out what those surfaces are and what I should do with them ;)
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    const Application* app = ApplicationManager::instance()->fromProcessId(surface->client()->processId());
#else
    const Application* app = ApplicationManager::instance()->fromProcessId(surface->processId());
#endif

    if (!app && ApplicationManager::instance()->securityChecksEnabled()) {
        qCWarning(LogWayland) << "SECURITY ALERT: an unknown application tried to create a wayland-surface!";
        return;
    }

#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
    if (!surface->hasShellSurface()) {
        // TODO find out what shellSurface is and why we get this for all lauched applications....
        //        qCDebug(LogWayland, 1, "shell-surface not supported yet ... please find out when/how/where this is happening and add corresponding code..."); <= this is not working ;) ... we get mouse-cursors or something ...
        return;
    }

    QWaylandSurfaceItem *item = surface->surfaceItem();
    if (!item) {
        // Create a WaylandSurfaceItem if we have not yet done so
        QQuickItem* rootObject = qobject_cast<QQuickView*>(window())->rootObject(); // the qobject_cast is validated/check upton by ctor
        Q_ASSERT(rootObject != 0); // a proper QQuickView should always have a rootObject, otherwise something really weird is going on!

        item = new QWaylandSurfaceItem(surface, 0);  /* NOTE #1: QWaylandSurfaceItem ctor will do surface->setSurfaceItem(this)
                                                        NOTE #2: no parent is set. That's the duty of qml!*/

        // TODO check if this is what we want (and whether it's working properly again ...)
        item->setResizeSurfaceToItem(true);
        // TODO find out what this does, and if we need it or not .. in either case, leave a comment explaining the decision.
        item->setTouchEventsEnabled(true);

        WaylandWindow *w = new WaylandWindow(app, item);
        setupWindow(w);

        // switch on Wayland ping/pong
        if (d->watchdogEnabled) {
            w->enablePing(true);
        }
    } else {
        emit surfaceItemReady(d->findWindowByWaylandSurface(surface), item);
    }
#else
    QWaylandSurfaceItem *item = static_cast<QWaylandSurfaceItem *>(surface->views().first());
    Q_ASSERT(item);

    item->setResizeSurfaceToItem(true);
    item->setTouchEventsEnabled(true);

    WaylandWindow *w = new WaylandWindow(app, item);
    setupWindow(w);

    // switch on Wayland ping/pong -- currently disabled, since it is a bit unstable
    //if (d->watchdogEnabled)
    //    w->enablePing(true);

#endif

    if (app) {
        //We only take focus for applications.
        item->takeFocus(); // otherwise we will never get keyboard focus in the client
    }
}

void WindowManager::waylandSurfaceUnmapped()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());

    qCDebug(LogWayland) << "waylandSurfaceUnmapped" << surface;
    handleWaylandSurfaceDestroyedOrUnmapped(surface);
}

void WindowManager::waylandSurfaceDestroyed()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());

    qCDebug(LogWayland) << "waylandSurfaceDestroyed" << surface;
    int index = d->findWindowByWaylandSurface(surface);
    if (index < 0)
        return;
    WaylandWindow *win = qobject_cast<WaylandWindow *>(d->windows[index]);
    if (!win)
        return;

    // switch off Wayland ping/pong
    if (d->watchdogEnabled)
        win->enablePing(false);

    handleWaylandSurfaceDestroyedOrUnmapped(surface);

    d->isClosing.remove(win);
    d->windows.removeAt(index);
    win->deleteLater();
}

void WindowManager::handleWaylandSurfaceDestroyedOrUnmapped(QWaylandSurface *surface)
{
    int index = d->findWindowByWaylandSurface(surface);
    if (index < 0)
        return;

    Window *win = d->windows[index];

    if (win->surfaceItem() && !d->isClosing[win]) {
        d->isClosing[win] = true;
        surfaceItemAboutToClose(win->surfaceItem());
    }
}

#endif

bool WindowManager::setSurfaceWindowProperty(QQuickItem *item, const QString &name, const QVariant &value)
{
    int index = d->findWindowBySurfaceItem(item);

    if (index < 0)
        return false;

    Window *win = d->windows[index];
    return win->setWindowProperty(name, value);
}

QVariant WindowManager::surfaceWindowProperty(QQuickItem *item, const QString &name) const
{
    int index = d->findWindowBySurfaceItem(item);

    if (index < 0)
        return QVariant();

    Window *win = d->windows[index];
    return win->windowProperty(name);
}

QVariantMap WindowManager::surfaceWindowProperties(QQuickItem *item) const
{
    int index = d->findWindowBySurfaceItem(item);

    if (index < 0)
        return QVariantMap();

    Window *win = d->windows[index];
    return win->windowProperties();
}


void WindowManager::windowPropertyChanged(const QString &name, const QVariant &value)
{
    Window *win = qobject_cast<Window *>(sender());
    if (!win)
        return;
    emit surfaceWindowPropertyChanged(win->surfaceItem(), name, value);
}


int WindowManagerPrivate::findWindowByApplication(const Application *app) const
{
    for (int i = 0; i < windows.count(); ++i) {
        if (app == windows.at(i)->application())
            return i;
    }
    return -1;
}

int WindowManagerPrivate::findWindowBySurfaceItem(QQuickItem *quickItem) const
{
    for (int i = 0; i < windows.count(); ++i) {
        if (quickItem == windows.at(i)->surfaceItem())
            return i;
    }
    return -1;
}

#ifndef AM_SINGLEPROCESS_MODE
int WindowManagerPrivate::findWindowByWaylandSurface(QWaylandSurface *waylandSurface) const
{
    for (int i = 0; i < windows.count(); ++i) {
        if (!windows.at(i)->isInProcess() && (waylandSurface == waylandSurfaceFromItem(windows.at(i)->surfaceItem())))
            return i;
    }
    return -1;
}

QWaylandSurface *WindowManagerPrivate::waylandSurfaceFromItem(QQuickItem *surfaceItem) const
{
    if (QWaylandSurfaceItem *item = qobject_cast<QWaylandSurfaceItem *>(surfaceItem))
        return item->surface();
    return 0;
}

#endif
