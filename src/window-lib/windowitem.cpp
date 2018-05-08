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

#include "windowitem.h"
#include "window.h"

#if defined(AM_MULTI_PROCESS)
#include "waylandcompositor.h"
#include "waylandwindow.h"
#include <QWaylandWlShellSurface>
#endif // AM_MULTI_PROCESS

#include "applicationmanager.h"
#include "inprocesswindow.h"

#include <QtAppManCommon/logging.h>

/*!
    \qmltype WindowItem
    \inqmlmodule QtApplicationManager
    \ingroup system-ui
    \brief An Item that renders a given WindowObject

    In order to render a WindowObject in System UI's scene, you must specify where and how it should
    be done. This is achieved by placing a WindowItem in the scene, which is a regular QML Item, and
    assigning the desired WindowObject to it.

    The WindowObject will then be rendered in the System-UI scene according to the WindowItem's geometry,
    opacity, visibility, transformations, etc.

    The relationship between WindowObjects and WindowItems is similar to the one between image files
    and Image items. The former is the content and the latter defines how it's rendered in a QML scene.

    \sa WindowObject
*/

/*!
    \qmlproperty WindowObject WindowItem::window

    The window surface to be displayed.
*/

QT_BEGIN_NAMESPACE_AM

///////////////////////////////////////////////////////////////////////////////////////////////////
// WindowItem
///////////////////////////////////////////////////////////////////////////////////////////////////

WindowItem::~WindowItem()
{
    setWindow(nullptr);
    delete m_impl;
}

void WindowItem::setWindow(Window *window)
{
    Window *currWindow = this->window();
    if (window == currWindow)
        return;

    if (currWindow) {
        m_impl->tearDown();
        disconnect(currWindow, nullptr, this, nullptr);
        currWindow->unregisterItem(this);
    }

    if (window) {
        window->registerItem(this);
        connect(window, &QObject::destroyed, this, [this]() { this->setWindow(nullptr); });

        createImpl(window->isInProcess());
        m_impl->setup(window);
    } else {
        delete m_impl;
        m_impl = nullptr;
    }

    emit windowChanged();
}

Window *WindowItem::window() const
{
    return m_impl ? m_impl->window() : nullptr;
}

void WindowItem::createImpl(bool inProcess)
{
#if defined(AM_MULTI_PROCESS)
    if (inProcess) {
        if (m_impl && !m_impl->isInProcess()) {
            delete m_impl;
            m_impl = nullptr;
        }
        if (!m_impl)
            m_impl = new InProcessImpl(this);
    } else {
        if (m_impl && m_impl->isInProcess()) {
            delete m_impl;
            m_impl = nullptr;
        }
        if (!m_impl)
            m_impl = new WaylandImpl(this);
    }
#else // AM_MULTI_PROCESS
    Q_ASSERT(inProcess);
    if (!m_impl)
        m_impl = new InProcessImpl(this);
#endif // AM_MULTI_PROCESS
}

void WindowItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (m_impl && newGeometry.isValid())
        m_impl->updateSize(newGeometry.size());

    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// WindowItem::InProcessImpl
///////////////////////////////////////////////////////////////////////////////////////////////////

void WindowItem::InProcessImpl::setup(Window *window)
{
    Q_ASSERT(!m_inProcessWindow);
    Q_ASSERT(window && window->isInProcess());

    m_inProcessWindow = static_cast<InProcessWindow*>(window);

    auto rootItem = m_inProcessWindow->rootItem();
    rootItem->setParentItem(q);
    rootItem->setX(0);
    rootItem->setY(0);
    rootItem->setSize(q->size());
}

void WindowItem::InProcessImpl::tearDown()
{
    m_inProcessWindow->rootItem()->setParentItem(nullptr);
    m_inProcessWindow = nullptr;
}

void WindowItem::InProcessImpl::updateSize(const QSizeF &newSize)
{
    m_inProcessWindow->rootItem()->setSize(newSize);
}

Window *WindowItem::InProcessImpl::window() const
{
    return static_cast<Window*>(m_inProcessWindow);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// WindowItem::WaylandImpl
///////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(AM_MULTI_PROCESS)

WindowItem::WaylandImpl::~WaylandImpl()
{
    delete m_waylandItem;
}

void WindowItem::WaylandImpl::setup(Window *window)
{
    Q_ASSERT(!m_waylandWindow);
    Q_ASSERT(window && !window->isInProcess());

    m_waylandWindow = static_cast<WaylandWindow*>(window);

    if (!m_waylandItem)
        createWaylandItem();

    m_waylandItem->setBufferLocked(false);
    m_waylandItem->setSurface(m_waylandWindow->surface());

    updateSize(q->size());
}

void WindowItem::WaylandImpl::createWaylandItem()
{
    m_waylandItem = new QWaylandQuickItem(q);
    m_waylandItem->setTouchEventsEnabled(true);
    m_waylandItem->setSizeFollowsSurface(false);

    connect(m_waylandItem, &QWaylandQuickItem::surfaceDestroyed, q, [this]() {
        // keep the buffer there to allow us to animate the window destruction
        m_waylandItem->setBufferLocked(true);
    });
}

void WindowItem::WaylandImpl::tearDown()
{
    m_waylandItem->setSurface(nullptr);
}

void WindowItem::WaylandImpl::updateSize(const QSizeF &newSize)
{
    m_waylandItem->setSize(newSize);

    AbstractApplication *app = nullptr; // prevent expensive lookup when not printing qDebugs
    auto *surface = m_waylandWindow->surface();
    qCDebug(LogGraphics) << "Sending geometry change request to Wayland client for surface"
                        << surface << "new:" << newSize << "of"
                        << ((app = ApplicationManager::instance()->fromProcessId(surface->client()->processId()))
                            ? app->id() : QString::fromLatin1("pid: %1").arg(surface->client()->processId()));
    surface->shellSurface()->sendConfigure(newSize.toSize(), QWaylandWlShellSurface::NoneEdge);
}

Window *WindowItem::WaylandImpl::window() const
{
    return static_cast<Window*>(m_waylandWindow);
}

#endif // AM_MULTI_PROCESS

QT_END_NAMESPACE_AM
