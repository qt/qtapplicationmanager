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

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>

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

    It's possible to assign the same WindowObject to multiple WindowItems, which will result in it being
    rendered multiple times.

    The implicit size of a WindowItem is the size of the WindowObject it is displaying.

    \sa WindowObject
*/

/*!
    \qmlproperty WindowObject WindowItem::window

    The window surface to be displayed.
*/

/*!
    \qmlproperty bool WindowItem::primary

    Returns whether this is the primary view of the set WindowObject.
    The primary WindowItem will be the one sending input events and defining the size of the WindowObject.

    A WindowObject can have only one primary WindowItem. If multiple WindowItems are rendering the same
    WindowObject, making one primary will automatically turn the primary property of all others to false.

    The first WindowItem to display a window is by default the primary one.

    \sa makePrimary
*/

/*!
    \qmlmethod void WindowItem::makePrimary

    Make it the primary WindowItem of the window it is displaying.

    \sa primary
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

        if (currWindow->primaryItem() == this) {
            currWindow->setPrimaryItem(nullptr);
            auto it = currWindow->items().begin();
            if (it != currWindow->items().end())
                (*it)->makePrimary();
        }
    }

    if (window) {
        window->registerItem(this);
        connect(window, &QObject::destroyed, this, [this]() { setWindow(nullptr); });
        connect(window, &Window::sizeChanged, this, &WindowItem::updateImplicitSize);

        createImpl(window->isInProcess());
        m_impl->setup(window);
        updateImplicitSize();

        if (window->items().count() == 1) {
            makePrimary();
        } else {
            m_impl->setupSecondaryView();
            emit primaryChanged();
        }

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

bool WindowItem::primary() const
{
    return window() ? window()->primaryItem() == this : true;
}

void WindowItem::makePrimary()
{
    if (!m_impl || !m_impl->window())
        return;

    auto *oldPrimaryItem = m_impl->window()->primaryItem();
    if (oldPrimaryItem == this)
        return;

    m_impl->setupPrimaryView();

    if (oldPrimaryItem && oldPrimaryItem->m_impl)
        oldPrimaryItem->m_impl->setupSecondaryView();

    window()->setPrimaryItem(this);

    if (oldPrimaryItem)
        emit oldPrimaryItem->primaryChanged();
    emit primaryChanged();

    m_impl->updateSize(QSizeF(width(), height()));
}

void WindowItem::updateImplicitSize()
{
    if (window()) {
        QSize size = window()->size();
        setImplicitSize(size.width(), size.height());
    } else {
        setImplicitSize(0, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// WindowItem::InProcessImpl
///////////////////////////////////////////////////////////////////////////////////////////////////

void WindowItem::InProcessImpl::setup(Window *window)
{
    Q_ASSERT(!m_inProcessWindow);
    Q_ASSERT(window && window->isInProcess());

    m_inProcessWindow = static_cast<InProcessWindow*>(window);
}

void WindowItem::InProcessImpl::tearDown()
{
    if (m_shaderEffectSource) {
        delete m_shaderEffectSource;
        m_shaderEffectSource = nullptr;
    } else {
        m_inProcessWindow->rootItem()->setParentItem(nullptr);
    }
    m_inProcessWindow = nullptr;
}

void WindowItem::InProcessImpl::updateSize(const QSizeF &newSize)
{
    if (!m_shaderEffectSource)
        m_inProcessWindow->rootItem()->setSize(newSize);
}

Window *WindowItem::InProcessImpl::window() const
{
    return static_cast<Window*>(m_inProcessWindow);
}

void WindowItem::InProcessImpl::setupPrimaryView()
{
    delete m_shaderEffectSource;
    m_shaderEffectSource = nullptr;

    auto rootItem = m_inProcessWindow->rootItem();
    rootItem->setParentItem(q);
    rootItem->setX(0);
    rootItem->setY(0);
    rootItem->setSize(QSizeF(q->width(), q->height()));
}

void WindowItem::InProcessImpl::setupSecondaryView()
{
    if (!m_shaderEffectSource) {
        QQmlEngine *engine = QQmlEngine::contextForObject(q)->engine();
        QQmlComponent component(engine);

        component.setData("import QtQuick 2.7\nShaderEffectSource { anchors.fill: parent }", QUrl());
        m_shaderEffectSource = qobject_cast<QQuickItem *>(component.create());
        m_shaderEffectSource->setParent(q);
        m_shaderEffectSource->setParentItem(q);

        QQmlProperty::write(m_shaderEffectSource, QStringLiteral("sourceItem"),
                QVariant::fromValue(m_inProcessWindow->rootItem()));
    }
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
}

void WindowItem::WaylandImpl::createWaylandItem()
{
    m_waylandItem = new QWaylandQuickItem(q);

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

    auto *surface = m_waylandWindow->surface();

    if (q->primary() && surface) {
        AbstractApplication *app = nullptr; // prevent expensive lookup when not printing qDebugs
        qCDebug(LogGraphics) << "Sending geometry change request to Wayland client for surface"
                            << surface << "new:" << newSize << "of"
                            << ((app = ApplicationManager::instance()->fromProcessId(surface->client()->processId()))
                                ? app->id() : QString::fromLatin1("pid: %1").arg(surface->client()->processId()));
        surface->sendResizing(newSize.toSize());
    }
}

Window *WindowItem::WaylandImpl::window() const
{
    return static_cast<Window*>(m_waylandWindow);
}

void WindowItem::WaylandImpl::setupPrimaryView()
{
    Q_ASSERT(m_waylandWindow);
    Q_ASSERT(m_waylandItem);

    m_waylandItem->setPrimary();
    m_waylandItem->setInputEventsEnabled(true);
    m_waylandItem->setTouchEventsEnabled(true);
}

void WindowItem::WaylandImpl::setupSecondaryView()
{
    m_waylandItem->setInputEventsEnabled(false);
    m_waylandItem->setTouchEventsEnabled(false);
}

#endif // AM_MULTI_PROCESS

QT_END_NAMESPACE_AM
