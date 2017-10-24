/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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
#include "qmlinprocessruntime.h"
#include <QSGSimpleRectNode>
#include <QQmlComponent>
#include <private/qqmlcomponentattached_p.h>


QT_BEGIN_NAMESPACE_AM

FakeApplicationManagerWindow::FakeApplicationManagerWindow(QQuickItem *parent)
    : QQuickItem(parent)
{
    qCDebug(LogSystem) << "FakeApplicationManagerWindow ctor! this:" << this;
    setFlag(ItemHasContents);
    connect(this, &QQuickItem::visibleChanged, this, &FakeApplicationManagerWindow::onVisibleChanged);
}

FakeApplicationManagerWindow::~FakeApplicationManagerWindow()
{
    qCDebug(LogSystem) << "FakeApplicationManagerWindow dtor! this: " << this;
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


bool FakeApplicationManagerWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    QByteArray key = nameToKey(name);
    QVariant oldValue = property(key);
    bool changed = !oldValue.isValid() || (oldValue != value);

    if (changed) {
        setProperty(key, value);
    }
    return true;
}

QVariant FakeApplicationManagerWindow::windowProperty(const QString &name) const
{
    QByteArray key = nameToKey(name);
    return property(key);
}

QVariantMap FakeApplicationManagerWindow::windowProperties() const
{
    const QList<QByteArray> keys = dynamicPropertyNames();
    QVariantMap map;

    for (const QByteArray &key : keys) {
        if (!isName(key))
            continue;

        QString name = keyToName(key);
        map[name] = property(key);
    }

    return map;
}

bool FakeApplicationManagerWindow::event(QEvent *e)
{
    if (e->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *dpce = static_cast<QDynamicPropertyChangeEvent *>(e);
        QByteArray key = dpce->propertyName();

        if (isName(key)) {
            QString name = keyToName(dpce->propertyName());
            emit windowPropertyChanged(name, property(key));

            //qWarning() << "FPW: got change" << name << " --> " << property(key).toString();
        }
    }

    return QQuickItem::event(e);
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

void FakeApplicationManagerWindow::componentComplete()
{
    qCDebug(LogSystem) << "FakeApplicationManagerWindow componentComplete() this:" << this;

    QQuickItem::componentComplete();

    QObject *prnt = parent();
    while (prnt && !m_runtime) {
        m_runtime = prnt->property("AM-RUNTIME").value<QtAM::QmlInProcessRuntime*>();
        prnt = prnt->parent();
    }

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
    if (m_runtime && isVisible())
        m_runtime->addWindow(this);
}

QT_END_NAMESPACE_AM
