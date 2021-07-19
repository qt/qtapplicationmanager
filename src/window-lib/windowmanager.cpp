/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QGuiApplication>
#include <QRegularExpression>
#include <QQuickView>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQmlEngine>
#include <QVariant>
#include <QMetaObject>
#include <QThread>
#include <QQmlComponent>
#include <private/qabstractanimation_p.h>
#include <QLocalServer>
#include "global.h"

#if defined(AM_MULTI_PROCESS)
#  include "waylandcompositor.h"
#  include <private/qwaylandcompositor_p.h>
#endif

#include "logging.h"
#include "application.h"
#include "applicationmanager.h"
#include "abstractruntime.h"
#include "runtimefactory.h"
#include "window.h"
#include "windowitem.h"
#include "windowmanager.h"
#include "windowmanager_p.h"
#include "waylandwindow.h"
#include "inprocesswindow.h"
#include "qml-utilities.h"


/*!
    \qmltype WindowManager
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-singletons
    \brief The window model and controller.

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
        \li \c window
        \target windowmanager-window-role
        \li WindowObject
        \li The WindowObject containing the client surface. To display it you have to put it in a
            WindowItem
    \row
        \li \c contentState
        \target windowmanager-contentState-role
        \li WindowObject::ContentState
        \li The content state of the WindowObject. See WindowObject::contentState
    \endtable

    \target Multi-process Wayland caveats

    \note Please be aware that Wayland is essentially an asynchronous IPC protocol, resulting in
    different local states in the client and server processes during state changes. A prime example
    for this is window property changes on the client side: in addition to being changed
    asynchronously on the server side, the windowPropertyChanged signal will not be emitted while
    the window object is not yet made available on the server side via the windowAdded signal. All
    those changes are not lost however, but the last change before emitting the windowAdded signal
    will be the initial state of the window object on the System UI side.

    \target Minimal compositor

    After importing, the WindowManager singleton can be used as in the example below.
    It demonstrates how to implement a basic, fullscreen window compositor with support
    for window show and hide animations:

    \qml
    import QtQuick 2.10
    import QtApplicationManager.SystemUI 2.0

    // Simple solution for a full-screen setup
    Item {
        width: 1024
        height: 640

        Connections {
            target: WindowManager
            // Send windows to a separate model so that we have control
            // over removals and ordering
            function onWindowAdded(window) {
                windowsModel.append({"window":window});
            }
        }

        Repeater {
            model: ListModel { id: windowsModel }
            delegate: WindowItem {
                id: windowItem
                anchors.fill: parent
                z: model.index

                window: model.window

                states: [
                    State {
                        name: "open"
                        when: model.window.contentState === WindowObject.SurfaceWithContent
                        PropertyChanges { target: windowItem; scale: 1; visible: true }
                    }
                ]

                scale: 0.50
                visible: false

                transitions: [
                    Transition {
                        to: "open"
                        NumberAnimation {
                            target: windowItem; property: "scale"
                            duration: 500; easing.type: Easing.OutQuad
                        }
                    },
                    Transition {
                        from: "open"
                        SequentialAnimation {
                            // we wanna see the window during the closing animation
                            PropertyAction { target: windowItem; property: "visible"; value: true }
                            NumberAnimation {
                                target: windowItem; property: "scale"
                                duration: 500; easing.type: Easing.InQuad
                            }
                            ScriptAction { script: {
                                // It's important to destroy our WindowItem once it's no longer needed in
                                // order to free up resources
                                if (model.window.contentState === WindowObject.NoSurface)
                                    windowsModel.remove(model.index, 1);
                            } }
                        }
                    }
                ]
            }
        }
    }
    \endqml

*/

/*!
    \qmlsignal WindowManager::windowAdded(WindowObject window)

    This signal is emitted when a new WindowObject is added to the model. This happens in response
    to an application creating a new window surface, which usually occurs during that application's
    startup.

    To display that \a window on your QML scene you need to assign it to a WindowItem.

    \note Please be aware that the windowAdded signal is not emitted immediately when the client
          sets a window to visible. This is due to the \l
          {Multi-process Wayland caveats}{asynchronous nature} of the underlying Wayland protocol.
*/

/*!
    \qmlsignal WindowManager::windowAboutToBeRemoved(WindowObject window)

    This signal is emitted before the \a window is removed from the model. This happens for
    instance, when the window \c visible property is set to \c false on the application side.
*/

/*!
    \qmlsignal WindowManager::windowContentStateChanged(WindowObject window)

    This signal is emitted when the WindowObject::contentState of the given \a window changes.

    \sa WindowObject::contentState
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
    WindowRole,
    ContentState
};
}

WindowManager *WindowManager::s_instance = nullptr;

WindowManager *WindowManager::createInstance(QQmlEngine *qmlEngine, const QString &waylandSocketName)
{
    if (s_instance)
        qFatal("WindowManager::createInstance() was called a second time.");

    qmlRegisterSingletonType<WindowManager>("QtApplicationManager.SystemUI", 2, 0, "WindowManager",
                                            &WindowManager::instanceForQml);

    qmlRegisterUncreatableType<Window>("QtApplicationManager.SystemUI", 2, 0, "WindowObject", qSL("Cannot create objects of type WindowObject"));
    qRegisterMetaType<Window*>("Window*");

    qmlRegisterType<WindowItem>("QtApplicationManager.SystemUI", 2, 0, "WindowItem");

    qRegisterMetaType<InProcessSurfaceItem*>("InProcessSurfaceItem*");
    qRegisterMetaType<QSharedPointer<InProcessSurfaceItem>>("QSharedPointer<InProcessSurfaceItem>");

    return s_instance = new WindowManager(qmlEngine, waylandSocketName);
}

WindowManager *WindowManager::instance()
{
    if (!s_instance)
        qFatal("WindowManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *WindowManager::instanceForQml(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

void WindowManager::shutDown()
{
    d->shuttingDown = true;

    if (d->allWindows.isEmpty())
        QMetaObject::invokeMethod(this, &WindowManager::shutDownFinished, Qt::QueuedConnection);
}

/*!
    \qmlproperty bool WindowManager::runningOnDesktop
    \readonly

    Holds \c true if running on a classic desktop window manager (Windows, X11, or macOS),
    \c false otherwise.
*/
bool WindowManager::isRunningOnDesktop() const
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
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
        for (Application *application : ApplicationManager::instance()->applications()) {
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
            qApp->disconnect(con); // MSVC cannot distinguish between static and non-static overloads in lambdas
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
    d->roleNames.insert(WindowRole, "window");
    d->roleNames.insert(ContentState, "contentState");

    d->qmlEngine = qmlEngine;

    qApp->installEventFilter(this);
}

WindowManager::~WindowManager()
{
    qApp->removeEventFilter(this);

#if defined(AM_MULTI_PROCESS)
    delete d->waylandCompositor;
#endif
    delete d;
    s_instance = nullptr;
}

void WindowManager::enableWatchdog(bool enable)
{
#if defined(AM_MULTI_PROCESS)
    WaylandWindow::m_watchdogEnabled = enable;
#else
    Q_UNUSED(enable);
#endif
}

bool WindowManager::addWaylandSocket(QLocalServer *waylandSocket)
{
#if defined(AM_MULTI_PROCESS)
    if (d->waylandCompositor) {
        qCWarning(LogGraphics) << "Cannot add extra Wayland sockets after the compositor has been created"
                                  " (tried to add:" << waylandSocket->fullServerName() << ").";
        delete waylandSocket;
        return false;
    }

    if (!waylandSocket || (waylandSocket->socketDescriptor() < 0)) {
        delete waylandSocket;
        return false;
    }

    waylandSocket->setParent(this);
    d->extraWaylandSockets << waylandSocket->socketDescriptor();
    return true;
#else
    Q_UNUSED(waylandSocket)
    return true;
#endif
}

int WindowManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->windowsInModel.count();
}

QVariant WindowManager::data(const QModelIndex &index, int role) const
{
    if (index.parent().isValid() || !index.isValid())
        return QVariant();

    if (index.row() < 0 || index.row() >= d->windowsInModel.count())
        return QVariant();

    Window *win = d->windowsInModel.at(index.row());

    switch (role) {
    case Id:
        if (win->application()) {
            return win->application()->nonAliased()->id();
        } else {
            return QString();
        }
    case WindowRole:
        return QVariant::fromValue(win);
    case ContentState:
        return win->contentState();
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
        qCWarning(LogSystem) << "WindowManager::get(index): invalid index:" << index;
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map.insert(qL1S(it.value()), data(QAbstractListModel::index(index), it.key()));
    return map;
}

/*!
    \qmlmethod WindowObject WindowManager::window(int index)

    Returns the \l{WindowObject}{window} corresponding to the given \a index in the
    model, or \c null if the index is invalid.

    \note The object ownership of the returned Window object stays with the application manager.
          If you want to store this pointer, you can use the WindowManager's QAbstractListModel
          signals or the windowAboutToBeRemoved signal to get notified if the object is about
          to be deleted on the C++ side.
*/
Window *WindowManager::window(int index) const
{
    if (index < 0 || index >= count()) {
        qCWarning(LogSystem) << "WindowManager::window(index): invalid index:" << index;
        return nullptr;
    }
    return d->windowsInModel.at(index);
}

/*!
    \qmlmethod list<WindowObject> WindowManager::windowsOfApplication(string applicationId)

    Returns a list of \l{WindowObject}{windows} belonging to the given \a applicationId in the
    model, or an empty list if the applicationId is invalid.

    \note The object ownership of the returned Window objects stays with the application manager.
          If you want to store these pointers, you can use the WindowManager's QAbstractListModel
          signals or the windowAboutToBeRemoved signal to get notified if the objects are about
          to be deleted on the C++ side.
*/
QList<QObject *> WindowManager::windowsOfApplication(const QString &id) const
{
    QList<QObject *> result;
    for (Window *window : d->windowsInModel) {
        if (window->application() && window->application()->id() == id)
            result << window;
    }
    return result;
}

/*!
    \qmlmethod int WindowManager::indexOfWindow(WindowObject window)

    Returns the index of the \a window within the WindowManager model, or \c -1 if the window item is
    not a managed window.
 */
int WindowManager::indexOfWindow(Window *window) const
{
    return d->windowsInModel.indexOf(window);
}

/*!
    \qmlmethod object WindowManager::addExtension(Component component)

    Creates a Wayland compositor extension from \a component and adds it to the System UI's
    underlying WaylandCompositor. The \a component must hold a valid Wayland compositor extension.
    On success, in multi-process mode, the function returns the created extension. Extensions can
    only be added, once ApplicationManager::windowManagerCompositorReady is true, for example:

    \code
    import QtWayland.Compositor.TextureSharingExtension 1.0

    Component {
        id: texshare
        TextureSharingExtension {}
    }

    Connections {
        target: ApplicationManager
        function onWindowManagerCompositorReadyChanged() {
            WindowManager.addExtension(texshare);
        }
    }
    \endcode

    \sa ApplicationManager::windowManagerCompositorReady

 */
QObject *WindowManager::addExtension(QQmlComponent *component) const
{
#if defined(AM_MULTI_PROCESS)
    if (!component || ApplicationManager::instance()->isSingleProcess())
        return nullptr;

    if (!d->waylandCompositor) {
        qCWarning(LogSystem) << "Extensions can only be added after ApplicationManager::windowManagerCompositorReady";
        return nullptr;
    }

    QObject *obj = component->beginCreate(qmlContext(this));
    auto extension = qobject_cast<QWaylandCompositorExtension*>(obj);

    if (extension)
        extension->setParent(d->waylandCompositor);

    component->completeCreate();

    if (!extension)
        delete obj;

    return extension;
#else
    Q_UNUSED(component)
    return nullptr;
#endif
}

void WindowManager::setupInProcessRuntime(AbstractRuntime *runtime)
{
    // special hacks to get in-process mode working transparently
    if (runtime->manager()->inProcess()) {
        runtime->setInProcessQmlEngine(d->qmlEngine);

        connect(runtime, &AbstractRuntime::inProcessSurfaceItemReady,
                this, &WindowManager::inProcessSurfaceItemCreated);
    }
}

/*
    Releases all resources of the window and destroys it.
*/
void WindowManager::releaseWindow(Window *window)
{
    int index = d->allWindows.indexOf(window);
    if (index == -1)
        return;

    d->allWindows.removeAt(index);

    disconnect(window, nullptr, this, nullptr);

    window->deleteLater();

    if (d->shuttingDown && (count() == 0))
        QMetaObject::invokeMethod(this, &WindowManager::shutDownFinished, Qt::QueuedConnection);
}

/*
    Adds the given window to the model.
 */
void WindowManager::addWindow(Window *window)
{
    beginInsertRows(QModelIndex(), d->windowsInModel.count(), d->windowsInModel.count());
    d->windowsInModel << window;
    endInsertRows();
    emit countChanged();
    emit windowAdded(window);
}

/*
    Removes the given window from the model.
 */
void WindowManager::removeWindow(Window *window)
{
    int index = d->windowsInModel.indexOf(window);
    if (index == -1)
        return;

    emit windowAboutToBeRemoved(window);

    beginRemoveRows(QModelIndex(), index, index);
    d->windowsInModel.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

/*
    Registers the given \a view as a possible destination for Wayland window composition.

    Wayland window items can only be rendered within top-level windows that have been registered
    via this function.
*/
void WindowManager::registerCompositorView(QQuickWindow *view)
{
    static bool once = false;

    if (d->views.contains(view))
        return;
    d->views << view;

    updateViewSlowMode(view);

#if defined(AM_MULTI_PROCESS)
    if (!ApplicationManager::instance()->isSingleProcess()) {
        if (!d->waylandCompositor) {
            d->waylandCompositor = new WaylandCompositor(view, d->waylandSocketName);
            for (const auto &extraSocket : d->extraWaylandSockets)
                d->waylandCompositor->addSocketDescriptor(extraSocket);

            connect(d->waylandCompositor, &QWaylandCompositor::surfaceCreated,
                    this, &WindowManager::waylandSurfaceCreated);
            connect(d->waylandCompositor, &WaylandCompositor::surfaceMapped,
                    this, &WindowManager::waylandSurfaceMapped);

            // export the actual socket name for our child processes.
            qputenv("WAYLAND_DISPLAY", d->waylandCompositor->socketName());
            qCInfo(LogGraphics).nospace() << "WindowManager: running in Wayland mode [socket: "
                                           << d->waylandCompositor->socketName() << "]";
            // SHM is always loaded
            if (QWaylandCompositorPrivate::get(d->waylandCompositor)->clientBufferIntegrations().size() <= 1) {
                qCCritical(LogGraphics) << "No OpenGL capable Wayland client buffer integration available: "
                                           "Wayland client applications can only use software rendering";
            }
            ApplicationManager::instance()->setWindowManagerCompositorReady(true);
        } else {
            d->waylandCompositor->registerOutputWindow(view);
        }
    } else {
        if (!once)
            qCInfo(LogGraphics) << "WindowManager: running in single-process mode [forced at run-time]";
    }
#else
    if (!once)
        qCInfo(LogGraphics) << "WindowManager: running in single-process mode [forced at compile-time]";
#endif
    emit compositorViewRegistered(view);

    if (!once)
        once = true;
}

/*! \internal
    Only used for in-process surfaces
 */
void WindowManager::inProcessSurfaceItemCreated(QSharedPointer<InProcessSurfaceItem> surfaceItem)
{
    AbstractRuntime *rt = qobject_cast<AbstractRuntime *>(sender());
    if (!rt) {
        qCCritical(LogSystem) << "This function must be called by a signal of Runtime!";
        return;
    }
    Application *app = rt->application() ? rt->application()->nonAliased() : nullptr;
    if (!app) {
        qCCritical(LogSystem) << "This function must be called by a signal of Runtime which actually has an application attached!";
        return;
    }

    //Only create a new Window if we don't have it already in the window list, as the user controls whether windows are removed or not
    int index = d->findWindowBySurfaceItem(surfaceItem.data());
    if (index == -1) {
        setupWindow(new InProcessWindow(app, surfaceItem));
    } else {
        auto window = qobject_cast<InProcessWindow*>(d->allWindows.at(index));
        window->setContentState(Window::SurfaceWithContent);
    }
}

/*! \internal
    Used to create the Window objects for a surface
    This is called for both wayland and in-process surfaces
*/
void WindowManager::setupWindow(Window *window)
{
    if (!window)
        return;

    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);

    connect(window, &Window::windowPropertyChanged,
            this, [this](const QString &name, const QVariant &value) {
        if (Window *win = qobject_cast<Window *>(sender()))
            emit windowPropertyChanged(win, name, value);
    });

    // In very rare cases window->deleteLater() is processed before the isBeingDisplayedChanged
    // handler below, i.e. the window destructor runs before the handler.
    QPointer<Window> guardedWindow = window;
    connect(window, &Window::isBeingDisplayedChanged, this, [this, guardedWindow]() {
        if (!guardedWindow.isNull() && guardedWindow->contentState() == Window::NoSurface
            && !guardedWindow->isBeingDisplayed()) {
            removeWindow(guardedWindow);
            releaseWindow(guardedWindow);
        }
    }, Qt::QueuedConnection);

    connect(window, &Window::contentStateChanged, this, [this, window]() {
        auto contentState = window->contentState();
        auto index = indexOfWindow(window);
        if (index != -1) {
            emit windowContentStateChanged(window);

            QModelIndex modelIndex = QAbstractListModel::index(index);
            qCDebug(LogGraphics).nospace() << "emitting dataChanged, index: " << modelIndex.row()
                    << ", contentState: " << window->contentState();
            emit dataChanged(modelIndex, modelIndex, QVector<int>() << ContentState);
        }

        if (contentState == Window::NoSurface) {
            removeWindow(window);
            if (!window->isBeingDisplayed())
                releaseWindow(window);
        }
    }, Qt::QueuedConnection);

    d->allWindows << window;
    addWindow(window);
}


#if defined(AM_MULTI_PROCESS)

void WindowManager::waylandSurfaceCreated(QWaylandSurface *surface)
{
    Q_UNUSED(surface)
    // this function is still useful for Wayland debugging
    //qCDebug(LogGraphics) << "New Wayland surface:" << surface->surface() << "pid:" << surface->processId();
}

void WindowManager::waylandSurfaceMapped(WindowSurface *surface)
{
    qint64 processId = surface->processId();
    const auto apps = ApplicationManager::instance()->fromProcessId(processId);
    Application *app = nullptr;

    if (apps.size() == 1) {
        app = apps.constFirst();
    } else if (apps.size() > 1) {
        // if there is more than one app within the same process, check the XDG surface appId
        const QString xdgAppId = surface->applicationId();
        if (!xdgAppId.isEmpty()) {
            for (const auto &checkApp : apps) {
                if (checkApp->id() == xdgAppId) {
                    app = checkApp;
                    break;
                }
            }
        }
    }

    if (!app && ApplicationManager::instance()->securityChecksEnabled()) {
        qCCritical(LogGraphics) << "SECURITY ALERT: an unknown application with pid" << processId
                                << "tried to map a Wayland surface!";
        return;
    }

    Q_ASSERT(surface);

    qCDebug(LogGraphics) << "Mapping Wayland surface" << surface << "of" << d->applicationId(app, surface);

    // Only create a new Window if we don't have it already in the window list, as the user controls
    // whether windows are removed or not
    int index = d->findWindowByWaylandSurface(surface->surface());
    if (index == -1) {
        WaylandWindow *w = new WaylandWindow(app, surface);
        setupWindow(w);
    }
}

#endif // defined(AM_MULTI_PROCESS)

/*!
    \qmlsignal WindowManager::windowPropertyChanged(WindowObject window, string name, var value)

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
        // app without System UI

        // filter out alias and apps not matching appId (if set)
        QVector<Application *> apps = ApplicationManager::instance()->applications();
        auto it = std::remove_if(apps.begin(), apps.end(), [appId](Application *app) {
            return app->isAlias() || (!appId.isEmpty() && (appId != app->id()));
        });
        apps.erase(it, apps.end());

        auto grabbers = new QList<QSharedPointer<const QQuickItemGrabResult>>;

        for (const Window *w : qAsConst(d->windowsInModel)) {
            if (apps.contains(w->application())) {
                if (attributeName.isEmpty()
                        || (w->windowProperty(attributeName).toString() == attributeValue)) {
                    for (int i = 0; i < d->views.count(); ++i) {
                        if (screenId.isEmpty() || screenId.toInt() == i) {
                            QQuickWindow *view = d->views.at(i);

                            bool onScreen = false;

                            auto itemList = w->items().values();
                            if (itemList.count() == 0)
                                continue;

                            // TODO: Care about multiple views?
                            WindowItem *windowItem = itemList.first();

                            onScreen = windowItem->QQuickItem::window() == view;

                            if (onScreen) {
                                foundAtLeastOne = true;
                                QSharedPointer<const QQuickItemGrabResult> grabber = windowItem->grabToImage();

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

int WindowManagerPrivate::findWindowBySurfaceItem(QQuickItem *quickItem) const
{
    for (int i = 0; i < allWindows.count(); ++i) {
        auto *window = allWindows.at(i);
        if (window->isInProcess() && static_cast<InProcessWindow*>(window)->rootItem() == quickItem)
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
    for (int i = 0; i < allWindows.count(); ++i) {
        auto *window = allWindows.at(i);

        if (window->isInProcess())
            continue;

        auto windowSurface = static_cast<WaylandWindow*>(window)->surface();
        if (windowSurface && windowSurface->surface() == waylandSurface)
            return i;
    }
    return -1;
}

QString WindowManagerPrivate::applicationId(Application *app, WindowSurface *windowSurface)
{
    if (app)
        return app->id();
    else if (windowSurface && windowSurface->surface() && windowSurface->surface()->client())
        return qSL("pid: ") + QString::number(windowSurface->surface()->client()->processId());
    else
        return qSL("<unknown client>");
}

#endif // defined(AM_MULTI_PROCESS)

bool WindowManager::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::PlatformSurface) {
        auto *window = qobject_cast<QQuickWindow *>(watched);
        if (!window)
            return false;

        auto surfaceEventType = static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType();
        if (surfaceEventType == QPlatformSurfaceEvent::SurfaceCreated)
            registerCompositorView(window);
    }
    return false;
}

QT_END_NAMESPACE_AM

#include "moc_windowmanager.cpp"
