// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "inprocesssurfaceitem.h"
#include "applicationmanagerwindow.h"
#include "qml-utilities.h"
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
    : QQuickFocusScope(parent)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    setClip(true);
    setFlag(ItemHasContents);
    m_windowProperties.installEventFilter(this);

    setFocusOnClick(true);
}

InProcessSurfaceItem::~InProcessSurfaceItem()
{
}

bool InProcessSurfaceItem::setWindowProperty(const QString &name, const QVariant &value)
{
    const QByteArray key = nameToKey(name);
    const QVariant v = convertFromJSVariant(value);
    QVariant oldValue = m_windowProperties.property(key);
    bool changed = !oldValue.isValid() || (oldValue != v);

    if (changed)
        m_windowProperties.setProperty(key, v);

    return true;
}

QVariant InProcessSurfaceItem::windowProperty(const QString &name) const
{
    QByteArray key = nameToKey(name);
    return m_windowProperties.property(key);
}

QVariantMap InProcessSurfaceItem::windowPropertiesAsVariantMap() const
{
    const QByteArrayList keys = m_windowProperties.dynamicPropertyNames();
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

    return QQuickFocusScope::eventFilter(o, e);
}

bool InProcessSurfaceItem::focusOnClick() const
{
    return m_focusOnClick;
}

void InProcessSurfaceItem::setFocusOnClick(bool newFocusOnClick)
{
    if (m_focusOnClick != newFocusOnClick) {
        m_focusOnClick = newFocusOnClick;

        setAcceptTouchEvents(newFocusOnClick);
        setAcceptedMouseButtons(newFocusOnClick ? Qt::AllButtons : Qt::NoButton);
        setFiltersChildMouseEvents(newFocusOnClick);

        emit focusOnClickChanged();
    }
}

void InProcessSurfaceItem::mousePressEvent(QMouseEvent *e)
{
    Q_ASSERT(m_focusOnClick);

    QQuickFocusScope::mousePressEvent(e);
    delayedForceActiveFocus();
}

void InProcessSurfaceItem::touchEvent(QTouchEvent *e)
{
    Q_ASSERT(m_focusOnClick);

    QQuickFocusScope::touchEvent(e);
    if (e->type() == QEvent::TouchBegin)
        delayedForceActiveFocus();
}

bool InProcessSurfaceItem::childMouseEventFilter(QQuickItem *item, QEvent *e)
{
    Q_UNUSED(item)
    Q_ASSERT(m_focusOnClick);

    if ((e->type() == QEvent::MouseButtonPress) || (e->type() == QEvent::TouchBegin))
        delayedForceActiveFocus();
    return false;
}

void InProcessSurfaceItem::delayedForceActiveFocus()
{
    QMetaObject::invokeMethod(this, [this]() {
            if (m_amWindow && !m_amWindow->activeFocusItem())
                forceActiveFocus(Qt::ActiveWindowFocusReason);
        }, Qt::QueuedConnection);
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
    if (m_color.alpha() == 0)
        return oldNode;  // no need to render fully transparent node

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

ApplicationManagerWindow *InProcessSurfaceItem::applicationManagerWindow()
{
    return m_amWindow.data();
}

void InProcessSurfaceItem::setApplicationManagerWindow(ApplicationManagerWindow *win)
{
    m_amWindow = win;
}

QT_END_NAMESPACE_AM

#include "moc_inprocesssurfaceitem.cpp"
