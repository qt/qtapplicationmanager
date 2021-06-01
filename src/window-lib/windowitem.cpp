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

#include "windowitem.h"
#include "window.h"

#if defined(AM_MULTI_PROCESS)
#include "waylandcompositor.h"
#include "waylandwindow.h"
#endif // AM_MULTI_PROCESS

#include "applicationmanager.h"
#include "inprocesswindow.h"

#include <QtAppManCommon/logging.h>

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QtQuick/private/qquickitem_p.h>

/*!
    \qmltype WindowItem
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-instantiable
    \brief An Item that renders a given WindowObject.

    To render a WindowObject inside the System UI, you must specify where and how it should be
    done using a WindowItem. A WindowItem is a regular QML Item, that you place in the scene
    and assign the desired WindowObject to it.

    The WindowObject is then rendered in the System UI scene according to the WindowItem's
    geometry, opacity, visibility, transformations, and so on.

    The relationship between WindowObjects and WindowItems is similar to the one between image
    files and Image items. The former is the content; the latter defines how it's rendered in a
    QML scene.

    It's possible to assign the same WindowObject to multiple WindowItems, so it can be rendered
    multiple times.

    The implicit size of a WindowItem is the size of the WindowObject it displays.

    \sa WindowObject
*/

/*!
    \qmlproperty WindowObject WindowItem::window

    The window surface to display.
*/

/*!
    \qmlproperty bool WindowItem::primary
    \readonly

    Returns whether this is the primary view of the WindowObject set.

    The primary WindowItem is the one sending input events and defining the size of the
    WindowObject. A WindowObject can have only one primary WindowItem. If multiple WindowItems
    are rendering the same WindowObject, making one primary automatically sets the primary
    property of all other WindowItems to false.

    By default, the first WindowItem that gets a particular WindowObject assigned will be the
    primary WindowItem for that WindowObject.

    \sa makePrimary
*/

/*!
    \qmlproperty bool WindowItem::objectFollowsItemSize

    If true, WindowItem resizes the WindowObject it's displaying to match its own size.
    If false, resizing the WindowItem has no effect on the size of the WindowObject being
    displayed. By default, this property is true.

    Set this property to false when you want the WindowItem size to be determined by the
    WindowObject's size. Additionally, don't specify a width and height to maintain the
    item's implicit size as its WindowObject's size, or explicitly set it to match the
    WindowObject's size.

    \sa WindowObject::resize
*/

/*!
    \qmlmethod void WindowItem::makePrimary

    Make the currently displayed window the primary WindowItem.

    \sa primary
*/

QT_BEGIN_NAMESPACE_AM

///////////////////////////////////////////////////////////////////////////////////////////////////
// WindowItem
///////////////////////////////////////////////////////////////////////////////////////////////////

WindowItem::WindowItem(QQuickItem *parent)
    : QQuickItem(parent)
    , m_contentItem(new QQuickItem(this))
{
    m_contentItem->setParent(this);

    m_contentItem->setZ(2);
    m_contentItem->setWidth(width());
    m_contentItem->setHeight(height());
}

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

void WindowItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    m_contentItem->setWidth(newGeometry.width());
    m_contentItem->setHeight(newGeometry.height());

    if (m_impl && newGeometry.isValid())
        m_impl->updateSize(newGeometry.size());

    QQuickItem::geometryChange(newGeometry, oldGeometry);
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

void WindowItem::setObjectFollowsItemSize(bool value)
{
    if (m_objectFollowsItemSize == value)
        return;

    m_objectFollowsItemSize = value;

    if (m_objectFollowsItemSize && m_impl)
        m_impl->updateSize(QSizeF(width(), height()));

    emit objectFollowsItemSizeChanged();
}

QQmlListProperty<QObject> WindowItem::contentItemData()
{
    return QQmlListProperty<QObject>(this, nullptr,
            WindowItem::contentItemData_append,
            WindowItem::contentItemData_count,
            WindowItem::contentItemData_at,
            WindowItem::contentItemData_clear);
}

void WindowItem::contentItemData_append(QQmlListProperty<QObject> *property, QObject *value)
{
    auto *that = static_cast<WindowItem*>(property->object);

    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->m_contentItem)->data();
    itemProperty.append(&itemProperty, value);
}

qsizetype WindowItem::contentItemData_count(QQmlListProperty<QObject> *property)
{
    auto *that = static_cast<WindowItem*>(property->object);
    if (!QQuickItemPrivate::get(that->m_contentItem)->data().count)
        return 0;
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->m_contentItem)->data();
    return itemProperty.count(&itemProperty);
}

QObject *WindowItem::contentItemData_at(QQmlListProperty<QObject> *property, qsizetype index)
{
    auto *that = static_cast<WindowItem*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->m_contentItem)->data();
    return itemProperty.at(&itemProperty, index);
}

void WindowItem::contentItemData_clear(QQmlListProperty<QObject> *property)
{
    auto *that = static_cast<WindowItem*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->m_contentItem)->data();
    itemProperty.clear(&itemProperty);
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
    if (!m_shaderEffectSource && q->m_objectFollowsItemSize)
        m_inProcessWindow->resize(newSize.toSize());
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

    connect(m_waylandItem, &QWaylandQuickItem::surfaceDestroyed, q, [this]() {
        // keep the buffer there to allow us to animate the window destruction
        m_waylandItem->setBufferLocked(true);
    });
}

void WindowItem::WaylandImpl::tearDown()
{
    m_waylandItem->setSurface(nullptr);
    m_waylandWindow = nullptr;
}

void WindowItem::WaylandImpl::updateSize(const QSizeF &newSize)
{
    m_waylandItem->setSize(newSize);

    if (q->primary() && q->m_objectFollowsItemSize)
        m_waylandWindow->resize(newSize.toSize());
}

Window *WindowItem::WaylandImpl::window() const
{
    return static_cast<Window*>(m_waylandWindow);
}

void WindowItem::WaylandImpl::setupPrimaryView()
{
    Q_ASSERT(m_waylandWindow);
    Q_ASSERT(m_waylandItem);

    // Calling setPrimary() on an item without a surface causes either a crash (old versions)
    // or a warning (newer versions) in qtwayland. Let's avoid that.
    if (m_waylandItem->surface())
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

#include "moc_windowitem.cpp"
