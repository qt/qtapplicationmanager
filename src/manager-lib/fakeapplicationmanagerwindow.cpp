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
#include <QSGSimpleRectNode>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlInfo>
#include <private/qqmlcomponentattached_p.h>


QT_BEGIN_NAMESPACE_AM

static QByteArray nameToKey(const QString &name)
{
    return QByteArray("_am_") + name.toUtf8();
}

static QString keyToName(const QByteArray &key)
{
    return QString::fromUtf8(key.mid(4));
}

static bool isName(const QByteArray &key)
{
    return key.startsWith("_am_");
}


FakeApplicationManagerWindow::FakeApplicationManagerWindow(QQuickItem *parent)
    : QQuickItem(parent)
    , m_windowProperties(new QObject)
{
    setFlag(ItemHasContents);
    setClip(true);
    connect(this, &QQuickItem::visibleChanged, this, &FakeApplicationManagerWindow::onVisibleChanged);

    m_windowProperties.data()->installEventFilter(this);
}

FakeApplicationManagerWindow::~FakeApplicationManagerWindow()
{
    if (m_surfaceItem) {
        m_runtime->removeWindow(this);
        m_surfaceItem->m_contentItem = nullptr;
    }
}

bool FakeApplicationManagerWindow::isFakeVisible() const
{
    return m_fakeVisible;
}

void FakeApplicationManagerWindow::setFakeVisible(bool visible)
{
    if (visible != m_fakeVisible) {
        m_fakeVisible = visible;
        setVisible(visible);
        if (m_surfaceItem) {
            m_surfaceItem->setVisible(visible);
            if (m_runtime && !visible)
                m_runtime->removeWindow(m_surfaceItem);
        } else {
            visibleChanged();
        }
    }
}

QColor FakeApplicationManagerWindow::color() const
{
    return m_color;
}

void FakeApplicationManagerWindow::setColor(const QColor &c)
{
    if (m_color != c) {
        m_color = c;
        emit colorChanged();
    }
}

void FakeApplicationManagerWindow::close()
{
    emit fakeCloseSignal();
}

void FakeApplicationManagerWindow::showFullScreen()
{
    emit fakeFullScreenSignal();
}
void FakeApplicationManagerWindow::showMaximized()
{
    emit fakeNoFullScreenSignal();
}

void FakeApplicationManagerWindow::showNormal()
{
    // doesn't work in wayland right now, so do nothing... revisit later (after andies resize-redesign)
}

bool FakeApplicationManagerWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    QByteArray key = nameToKey(name);
    QVariant oldValue = m_windowProperties->property(key);
    bool changed = !oldValue.isValid() || (oldValue != value);

    if (changed)
        m_windowProperties->setProperty(key, value);

    return true;
}

QVariant FakeApplicationManagerWindow::windowProperty(const QString &name) const
{
    QByteArray key = nameToKey(name);
    return m_windowProperties->property(key);
}

QVariantMap FakeApplicationManagerWindow::windowProperties() const
{
    const QList<QByteArray> keys = m_windowProperties->dynamicPropertyNames();
    QVariantMap map;

    for (const QByteArray &key : keys) {
        if (!isName(key))
            continue;

        QString name = keyToName(key);
        map[name] = m_windowProperties->property(key);
    }

    return map;
}

bool FakeApplicationManagerWindow::eventFilter(QObject *o, QEvent *e)
{
    if ((o == m_windowProperties) && (e->type() == QEvent::DynamicPropertyChange)) {
        QDynamicPropertyChangeEvent *dpce = static_cast<QDynamicPropertyChangeEvent *>(e);
        QByteArray key = dpce->propertyName();

        if (isName(key)) {
            QString name = keyToName(dpce->propertyName());
            emit windowPropertyChanged(name, m_windowProperties->property(key));
        }
    }

    return QQuickItem::eventFilter(o, e);
}

QSGNode *FakeApplicationManagerWindow::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QSGSimpleRectNode *node = static_cast<QSGSimpleRectNode *>(oldNode);
    if (!node)
        node = new QSGSimpleRectNode(clipRect(), color());
    else
        node->setRect(clipRect());
    return node;
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

    QQuickItem::componentComplete();
    determineRuntime();

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
                onVisibleChanged();
        });
    }

    // If we do not even have a single Component.onCompleted handler on the QML side, we need to
    // show the window immediately.
    if (m_attachedCompleteHandlers.isEmpty())
        onVisibleChanged();
}

void FakeApplicationManagerWindow::onVisibleChanged()
{
    if (m_runtime && isVisible() && !m_surfaceItem)
        m_runtime->addWindow(this);
}

/* The rest of the code is merely to hide properties and functions that
 * are derived from QQuickItem, but are not available in real QWindows. */

QJSValue FakeApplicationManagerWindow::getUndefined() const
{
    return QJSValue();
}

void FakeApplicationManagerWindow::referenceError(const char *symbol) const
{
    qmlWarning(this) << "ReferenceError: " << symbol << " is not defined";
}

void FakeApplicationManagerWindow::grabToImage() const          { referenceError("grabToImage"); }
void FakeApplicationManagerWindow::contains() const             { referenceError("contains"); }
void FakeApplicationManagerWindow::mapFromItem() const          { referenceError("mapFromItem"); }
void FakeApplicationManagerWindow::mapToItem() const            { referenceError("mapToItem"); }
void FakeApplicationManagerWindow::mapFromGlobal() const        { referenceError("mapFromGlobal"); }
void FakeApplicationManagerWindow::mapToGlobal() const          { referenceError("mapToGlobal"); }
void FakeApplicationManagerWindow::forceActiveFocus() const     { referenceError("forceActiveFocus"); }
void FakeApplicationManagerWindow::nextItemInFocusChain() const { referenceError("nextItemInFocusChain"); }
void FakeApplicationManagerWindow::childAt() const              { referenceError("childAt"); }

void FakeApplicationManagerWindow::connectNotify(const QMetaMethod &signal)
{
    // array of signal indices that should not be connected to
    static const QVector<int> metaIndices = {
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("parentChanged(QQuickItem*)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("childrenChanged()"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("childrenRectChanged(QRectF)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("zChanged()"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("enabledChanged()"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("visibleChildrenChanged()"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("stateChanged(QString)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("clipChanged(bool)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("focusChanged(bool)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("activeFocusChanged(bool)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("activeFocusOnTabChanged(bool)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("rotationChanged()"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("scaleChanged()"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("transformOriginChanged(TransformOrigin)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("smoothChanged(bool)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("antialiasingChanged(bool)"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("implicitWidthChanged()"),
        FakeApplicationManagerWindow::staticMetaObject.indexOfSignal("implicitHeightChanged()")
    };

    if (metaIndices.contains(signal.methodIndex())) {
        determineRuntime();
        if (m_runtime)
            m_runtime->m_componentError = true;

        QString name = qSL("on") + QString::fromUtf8(signal.name());
        name[2] = name[2].toUpper();
        qmlWarning(this) << "Cannot assign to non-existent property \"" << name << "\"";
    }
}

QT_END_NAMESPACE_AM
