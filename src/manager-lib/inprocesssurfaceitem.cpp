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

#include "inprocesssurfaceitem.h"
#include <QQmlEngine>
#include <QSGSimpleRectNode>

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

InProcessSurfaceItem::InProcessSurfaceItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    setClip(true);
    setFlag(ItemHasContents);
    m_windowProperties.installEventFilter(this);
}

InProcessSurfaceItem::~InProcessSurfaceItem()
{
}

bool InProcessSurfaceItem::setWindowProperty(const QString &name, const QVariant &value)
{
    QByteArray key = nameToKey(name);
    QVariant oldValue = m_windowProperties.property(key);
    bool changed = !oldValue.isValid() || (oldValue != value);

    if (changed)
        m_windowProperties.setProperty(key, value);

    return true;
}

QVariant InProcessSurfaceItem::windowProperty(const QString &name) const
{
    QByteArray key = nameToKey(name);
    return m_windowProperties.property(key);
}

QVariantMap InProcessSurfaceItem::windowPropertiesAsVariantMap() const
{
    const QList<QByteArray> keys = m_windowProperties.dynamicPropertyNames();
    QVariantMap map;

    for (const QByteArray &key : keys) {
        if (!isName(key))
            continue;

        QString name = keyToName(key);
        map[name] = m_windowProperties.property(key);
    }

    return map;
}

bool InProcessSurfaceItem::eventFilter(QObject *o, QEvent *e)
{
    if ((o == &m_windowProperties) && (e->type() == QEvent::DynamicPropertyChange)) {
        QDynamicPropertyChangeEvent *dpce = static_cast<QDynamicPropertyChangeEvent *>(e);
        QByteArray key = dpce->propertyName();

        if (isName(key)) {
            QString name = keyToName(dpce->propertyName());
            emit windowPropertyChanged(name, m_windowProperties.property(key));
        }
    }

    return QQuickItem::eventFilter(o, e);
}

void InProcessSurfaceItem::setVisibleClientSide(bool value)
{
    if (value != m_visibleClientSide) {
        m_visibleClientSide = value;
        emit visibleClientSideChanged();
    }
}

bool InProcessSurfaceItem::visibleClientSide() const
{
    return m_visibleClientSide;
}

QSGNode *InProcessSurfaceItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QSGSimpleRectNode *node = static_cast<QSGSimpleRectNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleRectNode(clipRect(), m_color);
    } else {
        node->setRect(clipRect());
        node->setColor(m_color);
    }
    return node;
}

QColor InProcessSurfaceItem::color() const
{
    return m_color;
}

void InProcessSurfaceItem::setColor(const QColor &c)
{
    if (m_color != c) {
        m_color = c;
        update();
    }
}

void InProcessSurfaceItem::close()
{
    emit closeRequested();
}

QT_END_NAMESPACE_AM

#include "moc_inprocesssurfaceitem.cpp"
