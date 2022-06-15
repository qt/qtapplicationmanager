// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "window.h"

/*!
    \qmltype WindowObject
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \brief A client window surface.

    A WindowObject represents a window from a client application. Each visible
    ApplicationManagerWindow on the application side will be reflected by a
    corresponding WindowObject on the server, System UI, side.

    To render a WindowObject inside your System UI you have to assign it to a WindowItem.

    \sa WindowItem
*/

/*!
    \qmlmethod bool WindowObject::setWindowProperty(string name, var value)

    Sets the application window's shared property identified by \a name to the given \a value.
    Returns \c true when successful.

    These properties are shared between the System UI and the client application: in single-process
    mode simply via a QVariantMap; in multi-process mode the sharing is done via Qt's extended
    surface Wayland extension. Changes from the client side are notified by the
    windowPropertyChanged() signal.

    See ApplicationManagerWindow for the client side API.

    \sa windowProperty(), windowProperties(), windowPropertyChanged()
*/

/*!
    \qmlmethod var WindowObject::windowProperty(string name)

    Returns the value of the application window's shared property identified by \a name.

    \sa setWindowProperty()
*/

/*!
    \qmlmethod var WindowObject::windowProperties()

    Returns an object containing all shared properties of the application window.

    \sa setWindowProperty()
*/

/*!
    \qmlmethod var WindowObject::resize(Size size)

    Resizes the WindowObject to the given \a size, in pixels. Usually you don't have to call
    this yourself as WindowItem takes care of it by default.

    \sa WindowObject::size, WindowItem::objectFollowsItemSize
*/

/*!
    \qmlproperty enumeration WindowObject::contentState
    \readonly

    This property holds the state of the content of this window.
    A window may be backed up by a surface and that surface may have content or not.

    \list
    \li WindowObject.SurfaceWithContent - The window is backed by a surface and that surface has content.
                                         This is the initial state.
    \li WindowObject.SurfaceNoContent - The window is backed by a surface but that surface presently has no content.
                                        It may get content again, in which case the state goes back to
                                        WindowObject::SurfaceWithContent, or it may be followed by a state change to
                                        WindowObject::NoSurface.
    \target windowobject-nosurface
    \li WindowObject.NoSurface - The window doesn't have a surface anymore. This is an end state. After displaying
                                 the appropriate animations (if any), System UI should ensure that there's no
                                 WindowItem instance left pointing to this Window (by either destroying them or
                                 resetting their WindowItem::window properties) in order to free up resources
                                 associated with it.
    \endlist
*/

/*!
    \qmlproperty QSize WindowObject::size
    \readonly

    The size of the window surface, in pixels

    \sa WindowObject::resize
*/

/*!
    \qmlproperty ApplicationObject WindowObject::application
    \readonly

    This property holds the \l{ApplicationObject}{application} that created this window.
*/

/*!
    \qmlmethod WindowObject::close()

    Sends a close event to the WindowObject.

    \sa Window::close()
*/

/*!
    \qmlproperty WaylandSurface WindowObject::waylandSurface
    \readonly

    This property exists only when running in multi-process mode. Enables
    you to access the underlying WaylandSurface of this window, if any.
    It will be null in case WindowObject::contentState is WindowObject.NoSurface.

    Naturally you should avoid using this property in code that should work in
    both single and multi-process modes.
*/

/*!
    \qmlsignal WindowObject::windowPropertyChanged(string name, var value)

    Notifies that the window property with \a name has a new \a value.
    Window property changes can be caused either by the System UI (via WindowObject::setWindowProperty)
    or by the application that created that window (via ApplicationManagerWindow::setWindowProperty).

    \sa setWindowProperty
*/
/*!
    \qmlproperty bool WindowObject::popup

    Returns \c true if this window is a native popup to the windowing system or \c false otherwise.
    Currently, only popups created through Wayland's xdg-shell extension are recognized as native.
*/
/*!
    \qmlproperty point WindowObject::requestedPopupPosition

    The position of the native popup as requested by the client; corresponds to
    QWaylandXdgPopup::configuredGeometry.

    Currently, only popups created through Wayland's xdg-shell extension are recognized as native.
    In all other cases this property returns a null point.

    \sa popup
*/
QT_BEGIN_NAMESPACE_AM

Window::Window(Application *app)
    : QObject()
    , m_application(app)
{
}

Window::~Window()
{
}

Application *Window::application() const
{
    return m_application;
}

void Window::registerItem(WindowItem *item)
{
    m_items.insert(item);
    if (m_items.count() == 1)
        Q_EMIT isBeingDisplayedChanged();
}

void Window::unregisterItem(WindowItem *item)
{
    m_items.remove(item);
    if (m_items.count() == 0)
        Q_EMIT isBeingDisplayedChanged();
}

void Window::setPrimaryItem(WindowItem *item)
{
    m_primaryItem = item;
}

bool Window::isBeingDisplayed() const
{
    return m_items.count() > 0;
}

QT_END_NAMESPACE_AM

#include "moc_window.cpp"
