/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

#include "window.h"

/*!
    \qmltype WindowObject
    \inqmlmodule QtApplicationManager
    \ingroup system-ui
    \brief A client window surface

    A WindowObject represents a window from a client application. Each visible
    ApplicationManagerWindow on the application side will be reflected by a
    corresponding WindowObject on the server, System-UI, side.

    To render a WindowObject inside your System-UI you have to assign it to a WindowItem.

    \sa WindowItem
*/

/*!
    \qmlmethod bool WindowObject::setWindowProperty(string name, var value)

    Sets the application window's shared property identified by \a name to the given \a value.

    These properties are shared between the System-UI and the client application: in single-process
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

    Resizes the WindowObject to the given size, in pixels. Usually you don't have to call
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

    \sa QWindow::close(), QCloseEvent
*/

QT_BEGIN_NAMESPACE_AM

Window::Window(AbstractApplication *app)
    : QObject()
    , m_application(app)
{
}

Window::~Window()
{
}

AbstractApplication *Window::application() const
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
