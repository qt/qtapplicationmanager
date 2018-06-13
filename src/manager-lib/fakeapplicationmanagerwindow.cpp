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

#include "logging.h"
#include "fakeapplicationmanagerwindow.h"
#include "inprocesssurfaceitem.h"
#include "qmlinprocessruntime.h"
#include <private/qqmlcomponentattached_p.h>

#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE_AM

FakeApplicationManagerWindow::FakeApplicationManagerWindow(QObject *parent)
    : QObject(parent)
    , m_surfaceItem(new InProcessSurfaceItem)
{
    m_surfaceItem->setFakeApplicationManagerWindow(this);

    connect(m_surfaceItem.data(), &QQuickItem::widthChanged,
            this, &FakeApplicationManagerWindow::widthChanged);

    connect(m_surfaceItem.data(), &QQuickItem::heightChanged,
            this, &FakeApplicationManagerWindow::heightChanged);

    connect(m_surfaceItem.data(), &InProcessSurfaceItem::windowPropertyChanged,
            this, &FakeApplicationManagerWindow::windowPropertyChanged);
}

FakeApplicationManagerWindow::~FakeApplicationManagerWindow()
{
    setVisible(false);
}

bool FakeApplicationManagerWindow::isVisible() const
{
    return m_surfaceItem->visibleClientSide();
}

void FakeApplicationManagerWindow::setVisible(bool visible)
{
    if (visible != m_surfaceItem->visibleClientSide()) {
        m_surfaceItem->setVisibleClientSide(visible);
        emit visibleChanged();
        addOrRemoveSurface();
    }
}

QColor FakeApplicationManagerWindow::color() const
{
    return m_surfaceItem->color();
}

void FakeApplicationManagerWindow::setColor(const QColor &c)
{
    if (color() != c) {
        m_surfaceItem->setColor(c);
        emit colorChanged();
    }
}

void FakeApplicationManagerWindow::close()
{
    setVisible(false);
}

void FakeApplicationManagerWindow::showFullScreen()
{
    setVisible(true);
}

void FakeApplicationManagerWindow::showMaximized()
{
    setVisible(true);
}

void FakeApplicationManagerWindow::showNormal()
{
    setVisible(true);
}

bool FakeApplicationManagerWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    return m_surfaceItem->setWindowProperty(name, value);
}

QVariant FakeApplicationManagerWindow::windowProperty(const QString &name) const
{
    return m_surfaceItem->windowProperty(name);
}

QVariantMap FakeApplicationManagerWindow::windowProperties() const
{
    return m_surfaceItem->windowPropertiesAsVariantMap();
}

QQmlListProperty<QObject> FakeApplicationManagerWindow::data()
{
    return QQmlListProperty<QObject>(this, nullptr,
            FakeApplicationManagerWindow::data_append,
            FakeApplicationManagerWindow::data_count,
            FakeApplicationManagerWindow::data_at,
            FakeApplicationManagerWindow::data_clear);
}

void FakeApplicationManagerWindow::data_append(QQmlListProperty<QObject> *property, QObject *value)
{
    auto *that = static_cast<FakeApplicationManagerWindow*>(property->object);

    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    itemProperty.append(&itemProperty, value);
}

int FakeApplicationManagerWindow::data_count(QQmlListProperty<QObject> *property)
{
    auto *that = static_cast<FakeApplicationManagerWindow*>(property->object);
    if (!QQuickItemPrivate::get(that->contentItem())->data().count)
        return 0;
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    return itemProperty.count(&itemProperty);
}

QObject *FakeApplicationManagerWindow::data_at(QQmlListProperty<QObject> *property, int index)
{
    auto *that = static_cast<FakeApplicationManagerWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    return itemProperty.at(&itemProperty, index);
}

void FakeApplicationManagerWindow::data_clear(QQmlListProperty<QObject> *property)
{
    auto *that = static_cast<FakeApplicationManagerWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    itemProperty.clear(&itemProperty);
}

void FakeApplicationManagerWindow::determineRuntime()
{
    if (!m_runtime) {
        QQmlContext *ctx = QQmlEngine::contextForObject(this);
        while (ctx && !m_runtime) {
            if (ctx->property(QmlInProcessRuntime::s_runtimeKey).isValid())
                m_runtime = ctx->property(QmlInProcessRuntime::s_runtimeKey).value<QmlInProcessRuntime*>();
            ctx = ctx->parentContext();
        }
    }
}

void FakeApplicationManagerWindow::componentComplete()
{
    qCDebug(LogSystem) << "FakeApplicationManagerWindow componentComplete() this:" << this;

    determineRuntime();

    findParentWindow(parent());

    // This part is scary, but we need to make sure that all Component.onComplete: handlers on
    // the QML side have been run, before we hand this window over to the WindowManager for the
    // onWindowReady signal. The problem here is that the C++ componentComplete() handler (this
    // function) is called *before* the QML side, plus, to make matters worse, the QML incubator
    // could switch back to the event loop a couple of times before finally calling the QML
    // onCompleted handler(s).
    // The workaround is to setup watchers for all Component.onCompleted handlers for this object
    // and wait until the last of them has been dealt with, to finally call our addWindow
    // function (we are also relying on the signal emission order, so that our lambda is called
    // after the actual QML handler).

    for (auto a = QQmlComponent::qmlAttachedProperties(this); a; a = a->next) {
        auto famw = qobject_cast<FakeApplicationManagerWindow *>(a->parent());
        if (!famw || famw != this)
            continue;

        m_attachedCompleteHandlers << a;

        connect(a, &QQmlComponentAttached::completed, this, [this, a]() {
            m_attachedCompleteHandlers.removeAll(a);

            if (m_attachedCompleteHandlers.isEmpty())
                addOrRemoveSurface();
        });
    }

    // If we do not even have a single Component.onCompleted handler on the QML side, we need to
    // show the window immediately.
    if (m_attachedCompleteHandlers.isEmpty()) {
        addOrRemoveSurface();
    }
}

void FakeApplicationManagerWindow::addOrRemoveSurface()
{
    if (!m_runtime)
        return;

    if (isVisible() && m_attachedCompleteHandlers.isEmpty() && (!m_parentWindow || m_parentWindow->isVisible()))
        m_runtime->addWindow(m_surfaceItem);
    else
        m_runtime->removeWindow(m_surfaceItem);
}

QQuickItem *FakeApplicationManagerWindow::contentItem()
{
    return m_surfaceItem.data();
}

void FakeApplicationManagerWindow::findParentWindow(QObject *object)
{
    if (!object)
        return;

    auto surfaceItem = qobject_cast<InProcessSurfaceItem*>(object);
    if (surfaceItem) {
        setParentWindow(static_cast<FakeApplicationManagerWindow*>(surfaceItem->fakeApplicationManagerWindow()));
    } else {
        auto fakeAppWindow = qobject_cast<FakeApplicationManagerWindow*>(object);
        if (fakeAppWindow)
            setParentWindow(fakeAppWindow);
        else
            findParentWindow(object->parent());
    }
}

void FakeApplicationManagerWindow::setParentWindow(FakeApplicationManagerWindow *fakeAppWindow)
{
    if (m_parentWindow)
        disconnect(m_parentWindow, nullptr, this, nullptr);

    m_parentWindow = fakeAppWindow;

    if (m_parentWindow)
        connect(m_parentWindow, &FakeApplicationManagerWindow::visibleChanged,
                this, &FakeApplicationManagerWindow::addOrRemoveSurface);
}

void FakeApplicationManagerWindow::setTitle(const QString &value)
{
    if (m_title != value) {
        m_title = value;
        emit titleChanged();
    }
}

void FakeApplicationManagerWindow::setX(int value)
{
    if (m_x != value) {
        m_x = value;
        emit xChanged();
    }
}

void FakeApplicationManagerWindow::setY(int value)
{
    if (m_y != value) {
        m_y = value;
        emit yChanged();
    }
}

int FakeApplicationManagerWindow::width() const
{
    return m_surfaceItem->width();
}

void FakeApplicationManagerWindow::setWidth(int value)
{
    m_surfaceItem->setWidth(value);
}

int FakeApplicationManagerWindow::height() const
{
    return m_surfaceItem->height();
}

void FakeApplicationManagerWindow::setHeight(int value)
{
    m_surfaceItem->setHeight(value);
}

void FakeApplicationManagerWindow::setMinimumWidth(int value)
{
    if (m_minimumWidth != value) {
        m_minimumWidth = value;
        emit minimumWidthChanged();
    }
}

void FakeApplicationManagerWindow::setMinimumHeight(int value)
{
    if (m_minimumHeight != value) {
        m_minimumHeight = value;
        emit minimumHeightChanged();
    }
}

void FakeApplicationManagerWindow::setMaximumWidth(int value)
{
    if (m_maximumWidth != value) {
        m_maximumWidth = value;
        emit maximumWidthChanged();
    }
}

void FakeApplicationManagerWindow::setMaximumHeight(int value)
{
    if (m_maximumHeight != value) {
        m_maximumHeight = value;
        emit maximumHeightChanged();
    }
}

void FakeApplicationManagerWindow::setOpacity(qreal value)
{
    if (m_opacity != value) {
        m_opacity = value;
        emit opacityChanged();
    }
}

QT_END_NAMESPACE_AM
