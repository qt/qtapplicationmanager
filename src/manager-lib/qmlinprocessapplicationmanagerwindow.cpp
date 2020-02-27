/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include "logging.h"
#include "qmlinprocessapplicationmanagerwindow.h"
#include "inprocesssurfaceitem.h"
#include "qmlinprocessruntime.h"
#include <private/qqmlcomponentattached_p.h>

#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE_AM

QVector<QmlInProcessApplicationManagerWindow *> QmlInProcessApplicationManagerWindow::s_inCompleteWindows;

QmlInProcessApplicationManagerWindow::QmlInProcessApplicationManagerWindow(QObject *parent)
    : QObject(parent)
    , m_surfaceItem(new InProcessSurfaceItem)
{
    m_surfaceItem->setInProcessApplicationManagerWindow(this);

    connect(m_surfaceItem.data(), &QQuickItem::widthChanged,
            this, &QmlInProcessApplicationManagerWindow::widthChanged);

    connect(m_surfaceItem.data(), &QQuickItem::heightChanged,
            this, &QmlInProcessApplicationManagerWindow::heightChanged);

    connect(m_surfaceItem.data(), &InProcessSurfaceItem::windowPropertyChanged,
            this, &QmlInProcessApplicationManagerWindow::windowPropertyChanged);

    connect(m_surfaceItem.data(), &InProcessSurfaceItem::closeRequested,
            this, &QmlInProcessApplicationManagerWindow::close);
}

QmlInProcessApplicationManagerWindow::~QmlInProcessApplicationManagerWindow()
{
    setVisible(false);
}

bool QmlInProcessApplicationManagerWindow::isVisible() const
{
    return m_surfaceItem->visibleClientSide();
}

void QmlInProcessApplicationManagerWindow::setVisible(bool visible)
{
    if (visible != m_surfaceItem->visibleClientSide()) {
        m_surfaceItem->setVisibleClientSide(visible);
        emit visibleChanged();
        notifyRuntimeAboutSurface();
    }
}

QColor QmlInProcessApplicationManagerWindow::color() const
{
    return m_surfaceItem->color();
}

void QmlInProcessApplicationManagerWindow::setColor(const QColor &c)
{
    if (color() != c) {
        m_surfaceItem->setColor(c);
        emit colorChanged();
    }
}

void QmlInProcessApplicationManagerWindow::close()
{
    setVisible(false);
    if (m_runtime) {
        // Queued because the runtime might end up deleting this object
        QMetaObject::invokeMethod(m_runtime, &QmlInProcessRuntime::stopIfNoVisibleSurfaces, Qt::QueuedConnection);
    }
}

void QmlInProcessApplicationManagerWindow::showFullScreen()
{
    setVisible(true);
}

void QmlInProcessApplicationManagerWindow::showMaximized()
{
    setVisible(true);
}

void QmlInProcessApplicationManagerWindow::showNormal()
{
    setVisible(true);
}

bool QmlInProcessApplicationManagerWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    return m_surfaceItem->setWindowProperty(name, value);
}

QVariant QmlInProcessApplicationManagerWindow::windowProperty(const QString &name) const
{
    return m_surfaceItem->windowProperty(name);
}

QVariantMap QmlInProcessApplicationManagerWindow::windowProperties() const
{
    return m_surfaceItem->windowPropertiesAsVariantMap();
}

QQmlListProperty<QObject> QmlInProcessApplicationManagerWindow::data()
{
    return QQmlListProperty<QObject>(this, nullptr,
            QmlInProcessApplicationManagerWindow::data_append,
            QmlInProcessApplicationManagerWindow::data_count,
            QmlInProcessApplicationManagerWindow::data_at,
            QmlInProcessApplicationManagerWindow::data_clear);
}

void QmlInProcessApplicationManagerWindow::data_append(QQmlListProperty<QObject> *property, QObject *value)
{
    auto *that = static_cast<QmlInProcessApplicationManagerWindow*>(property->object);

    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    itemProperty.append(&itemProperty, value);
}

int QmlInProcessApplicationManagerWindow::data_count(QQmlListProperty<QObject> *property)
{
    auto *that = static_cast<QmlInProcessApplicationManagerWindow*>(property->object);
    if (!QQuickItemPrivate::get(that->contentItem())->data().count)
        return 0;
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    return itemProperty.count(&itemProperty);
}

QObject *QmlInProcessApplicationManagerWindow::data_at(QQmlListProperty<QObject> *property, int index)
{
    auto *that = static_cast<QmlInProcessApplicationManagerWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    return itemProperty.at(&itemProperty, index);
}

void QmlInProcessApplicationManagerWindow::data_clear(QQmlListProperty<QObject> *property)
{
    auto *that = static_cast<QmlInProcessApplicationManagerWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    itemProperty.clear(&itemProperty);
}

void QmlInProcessApplicationManagerWindow::componentComplete()
{
    if (!m_runtime)
        m_runtime = QmlInProcessRuntime::determineRuntime(this);

    findParentWindow(parent());

    // This part is scary, but we need to make sure that all Component.onComplete: handlers on
    // the QML side have been run, before we hand this window over to the WindowManager to emit the
    // windowAdded signal. The problem here is that the C++ componentComplete() handler (this
    // function) is called *before* the QML side, plus, to make matters worse, the QML incubator
    // could switch back to the event loop a couple of times before finally calling the QML
    // onCompleted handler(s).
    // The workaround is to setup watchers for all Component.onCompleted handlers for this object
    // and wait until the last of them has been dealt with, to finally call our addSurfaceItem
    // function (we are also relying on the signal emission order, so that our lambda is called
    // after the actual QML handler).
    // We also make sure that onWindowAdded is called in the same order, as this function is
    // called, so we don't depend on the existence of onCompleted handlers (and conform to multi-
    // process mode; there is no guarantee though, since multi-process is inherently asynchronous).

    for (auto a = QQmlComponent::qmlAttachedProperties(this); a; a = a->next) {
        auto appWindow = qobject_cast<QmlInProcessApplicationManagerWindow *>(a->parent());
        if (!appWindow || appWindow != this)
            continue;

        m_attachedCompleteHandlers << a;

        connect(a, &QQmlComponentAttached::completed, this, [this, a]() {
            m_attachedCompleteHandlers.removeAll(a);

            if (m_attachedCompleteHandlers.isEmpty()) {
                QMutableVectorIterator<QmlInProcessApplicationManagerWindow *> iter(s_inCompleteWindows);
                while (iter.hasNext()) {
                    QmlInProcessApplicationManagerWindow *win = iter.next();
                    if (win->m_attachedCompleteHandlers.isEmpty()) {
                        iter.remove();
                        win->notifyRuntimeAboutSurface();
                    } else {
                        break;
                    }

                }
            }
        });
    }

    // If we do not even have a single Component.onCompleted handler on the QML side and no
    // previous window is waiting for onCompleted, we need to show the window immediately.
    // Otherwise we save this window and process it later in the lambda above, in the same order as
    // this function is called.
    if (m_attachedCompleteHandlers.isEmpty() && s_inCompleteWindows.isEmpty())
        notifyRuntimeAboutSurface();
    else
        s_inCompleteWindows << this;
}

void QmlInProcessApplicationManagerWindow::notifyRuntimeAboutSurface()
{
    if (!m_runtime)
        return;

    if (isVisible() && m_attachedCompleteHandlers.isEmpty() && (!m_parentWindow || m_parentWindow->isVisible()))
        m_runtime->addSurfaceItem(m_surfaceItem);
}

QQuickItem *QmlInProcessApplicationManagerWindow::contentItem()
{
    return m_surfaceItem.data();
}

void QmlInProcessApplicationManagerWindow::findParentWindow(QObject *object)
{
    if (!object)
        return;

    auto surfaceItem = qobject_cast<InProcessSurfaceItem*>(object);
    if (surfaceItem) {
        setParentWindow(static_cast<QmlInProcessApplicationManagerWindow*>(surfaceItem->inProcessApplicationManagerWindow()));
    } else {
        auto inProcessAppWindow = qobject_cast<QmlInProcessApplicationManagerWindow*>(object);
        if (inProcessAppWindow)
            setParentWindow(inProcessAppWindow);
        else
            findParentWindow(object->parent());
    }
}

void QmlInProcessApplicationManagerWindow::setParentWindow(QmlInProcessApplicationManagerWindow *inProcessAppWindow)
{
    if (m_parentWindow)
        disconnect(m_parentWindow, nullptr, this, nullptr);

    m_parentWindow = inProcessAppWindow;

    if (m_parentWindow)
        connect(m_parentWindow, &QmlInProcessApplicationManagerWindow::visibleChanged,
                this, &QmlInProcessApplicationManagerWindow::notifyRuntimeAboutSurface);
}

void QmlInProcessApplicationManagerWindow::setTitle(const QString &value)
{
    if (m_title != value) {
        m_title = value;
        emit titleChanged();
    }
}

void QmlInProcessApplicationManagerWindow::setX(int value)
{
    if (m_x != value) {
        m_x = value;
        emit xChanged();
    }
}

void QmlInProcessApplicationManagerWindow::setY(int value)
{
    if (m_y != value) {
        m_y = value;
        emit yChanged();
    }
}

int QmlInProcessApplicationManagerWindow::width() const
{
    return m_surfaceItem->width();
}

void QmlInProcessApplicationManagerWindow::setWidth(int value)
{
    m_surfaceItem->setWidth(value);
}

int QmlInProcessApplicationManagerWindow::height() const
{
    return m_surfaceItem->height();
}

void QmlInProcessApplicationManagerWindow::setHeight(int value)
{
    m_surfaceItem->setHeight(value);
}

void QmlInProcessApplicationManagerWindow::setMinimumWidth(int value)
{
    if (m_minimumWidth != value) {
        m_minimumWidth = value;
        emit minimumWidthChanged();
    }
}

void QmlInProcessApplicationManagerWindow::setMinimumHeight(int value)
{
    if (m_minimumHeight != value) {
        m_minimumHeight = value;
        emit minimumHeightChanged();
    }
}

void QmlInProcessApplicationManagerWindow::setMaximumWidth(int value)
{
    if (m_maximumWidth != value) {
        m_maximumWidth = value;
        emit maximumWidthChanged();
    }
}

void QmlInProcessApplicationManagerWindow::setMaximumHeight(int value)
{
    if (m_maximumHeight != value) {
        m_maximumHeight = value;
        emit maximumHeightChanged();
    }
}

void QmlInProcessApplicationManagerWindow::setOpacity(qreal value)
{
    if (m_opacity != value) {
        m_opacity = value;
        emit opacityChanged();
    }
}

QT_END_NAMESPACE_AM
