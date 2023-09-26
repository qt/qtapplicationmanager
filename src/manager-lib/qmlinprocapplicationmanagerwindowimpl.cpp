// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "logging.h"
#include "applicationmanagerwindow.h"
#include "qmlinprocapplicationmanagerwindowimpl.h"
#include "inprocesssurfaceitem.h"
#include "qmlinprocruntime.h"
#include <private/qqmlcomponentattached_p.h>

#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE_AM

QVector<QmlInProcApplicationManagerWindowImpl *> QmlInProcApplicationManagerWindowImpl::s_inCompleteWindows;

QmlInProcApplicationManagerWindowImpl::QmlInProcApplicationManagerWindowImpl(ApplicationManagerWindow *window)
    : ApplicationManagerWindowImpl(window)
    , m_surfaceItem(new InProcessSurfaceItem)
{
    m_surfaceItem->setInProcessApplicationManagerWindow(window);
    m_surfaceItem->setColor(QColorConstants::White);

    QObject::connect(m_surfaceItem.data(), &QQuickItem::widthChanged,
                     window, &ApplicationManagerWindow::widthChanged);

    QObject::connect(m_surfaceItem.data(), &QQuickItem::heightChanged,
                     window, &ApplicationManagerWindow::heightChanged);

    QObject::connect(m_surfaceItem.data(), &InProcessSurfaceItem::windowPropertyChanged,
                     window, &ApplicationManagerWindow::windowPropertyChanged);

    QObject::connect(m_surfaceItem.data(), &InProcessSurfaceItem::closeRequested,
                     window, &ApplicationManagerWindow::close);
}

QmlInProcApplicationManagerWindowImpl::~QmlInProcApplicationManagerWindowImpl()
{ }

bool QmlInProcApplicationManagerWindowImpl::isInProcess() const
{
    return true;
}

QObject *QmlInProcApplicationManagerWindowImpl::backingObject() const
{
    return m_surfaceItem.get();
}

void QmlInProcApplicationManagerWindowImpl::close()
{
    amWindow()->setVisible(false);
    if (m_runtime) {
        // Queued because the runtime might end up deleting this object
        QMetaObject::invokeMethod(m_runtime, &QmlInProcRuntime::stopIfNoVisibleSurfaces, Qt::QueuedConnection);
    }
}

void QmlInProcApplicationManagerWindowImpl::showFullScreen()
{
    amWindow()->setVisible(true);
}

void QmlInProcApplicationManagerWindowImpl::showMaximized()
{
    amWindow()->setVisible(true);
}

void QmlInProcApplicationManagerWindowImpl::showNormal()
{
    amWindow()->setVisible(true);
}

bool QmlInProcApplicationManagerWindowImpl::setWindowProperty(const QString &name, const QVariant &value)
{
    return m_surfaceItem->setWindowProperty(name, value);
}

QVariant QmlInProcApplicationManagerWindowImpl::windowProperty(const QString &name) const
{
    return m_surfaceItem->windowProperty(name);
}

QVariantMap QmlInProcApplicationManagerWindowImpl::windowProperties() const
{
    return m_surfaceItem->windowPropertiesAsVariantMap();
}

void QmlInProcApplicationManagerWindowImpl::classBegin()
{ }

void QmlInProcApplicationManagerWindowImpl::componentComplete()
{
    if (!m_runtime)
        m_runtime = QmlInProcRuntime::determineRuntime(amWindow());

    findParentWindow(amWindow()->parent());

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

    for (auto a = QQmlComponent::qmlAttachedProperties(amWindow()); a; a = a->next()) {
        auto appWindow = qobject_cast<ApplicationManagerWindow *>(a->parent());
        if (!appWindow || appWindow != amWindow())
            continue;

        m_attachedCompleteHandlers << a;

        QObject::connect(a, &QQmlComponentAttached::completed, amWindow(), [this, a]() {
            m_attachedCompleteHandlers.removeAll(a);

            if (m_attachedCompleteHandlers.isEmpty()) {
                for (auto it = s_inCompleteWindows.cbegin(); it != s_inCompleteWindows.cend(); ) {
                    QmlInProcApplicationManagerWindowImpl *win = *it;
                    if (win->m_attachedCompleteHandlers.isEmpty()) {
                        it = s_inCompleteWindows.erase(it);
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

void QmlInProcApplicationManagerWindowImpl::notifyRuntimeAboutSurface()
{
    if (!m_runtime)
        return;

    if (isVisible() && m_attachedCompleteHandlers.isEmpty() && (!m_parentWindow || m_parentWindow->isVisible()))
        m_runtime->addSurfaceItem(m_surfaceItem);
}

QQuickItem *QmlInProcApplicationManagerWindowImpl::contentItem()
{
    return m_surfaceItem.data();
}

void QmlInProcApplicationManagerWindowImpl::findParentWindow(QObject *object)
{
    if (!object)
        return;

    auto surfaceItem = qobject_cast<InProcessSurfaceItem *>(object);
    if (surfaceItem) {
        setParentWindow(static_cast<ApplicationManagerWindow *>(surfaceItem->inProcessApplicationManagerWindow()));
    } else {
        auto inProcessAppWindow = qobject_cast<ApplicationManagerWindow *>(object);
        if (inProcessAppWindow)
            setParentWindow(inProcessAppWindow);
        else
            findParentWindow(object->parent());
    }
}

void QmlInProcApplicationManagerWindowImpl::setParentWindow(ApplicationManagerWindow *inProcessAppWindow)
{
    if (m_parentWindow && m_parentVisibleConnection)
        QObject::disconnect(m_parentVisibleConnection);

    m_parentWindow = inProcessAppWindow;

    if (m_parentWindow) {
        m_parentVisibleConnection = QObject::connect(
            m_parentWindow, &ApplicationManagerWindow::visibleChanged,
            amWindow(), [this]() { notifyRuntimeAboutSurface(); });
    }
}

void QmlInProcApplicationManagerWindowImpl::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        emit amWindow()->titleChanged();
    }
}

void QmlInProcApplicationManagerWindowImpl::setX(int x)
{
    if (m_x != x) {
        m_x = x;
        emit amWindow()->xChanged();
    }
}

void QmlInProcApplicationManagerWindowImpl::setY(int y)
{
    if (m_y != y) {
        m_y = y;
        emit amWindow()->yChanged();
    }
}

int QmlInProcApplicationManagerWindowImpl::width() const
{
    return m_surfaceItem->width();
}

void QmlInProcApplicationManagerWindowImpl::setWidth(int w)
{
    m_surfaceItem->setWidth(w);
}

int QmlInProcApplicationManagerWindowImpl::height() const
{
    return m_surfaceItem->height();
}

void QmlInProcApplicationManagerWindowImpl::setHeight(int h)
{
    m_surfaceItem->setHeight(h);
}

void QmlInProcApplicationManagerWindowImpl::setMinimumWidth(int minw)
{
    if (m_minimumWidth != minw) {
        m_minimumWidth = minw;
        emit amWindow()->minimumWidthChanged();
    }
}

void QmlInProcApplicationManagerWindowImpl::setMinimumHeight(int minh)
{
    if (m_minimumHeight != minh) {
        m_minimumHeight = minh;
        emit amWindow()->minimumHeightChanged();
    }
}

void QmlInProcApplicationManagerWindowImpl::setMaximumWidth(int maxw)
{
    if (m_maximumWidth != maxw) {
        m_maximumWidth = maxw;
        emit amWindow()->maximumWidthChanged();
    }
}

void QmlInProcApplicationManagerWindowImpl::setMaximumHeight(int maxh)
{
    if (m_maximumHeight != maxh) {
        m_maximumHeight = maxh;
        emit amWindow()->maximumHeightChanged();
    }
}

bool QmlInProcApplicationManagerWindowImpl::isVisible() const
{
    return m_surfaceItem->visibleClientSide();
}

void QmlInProcApplicationManagerWindowImpl::setVisible(bool visible)
{
    if (visible != m_surfaceItem->visibleClientSide()) {
        m_surfaceItem->setVisibleClientSide(visible);
        emit amWindow()->visibleChanged();
        notifyRuntimeAboutSurface();
    }
}

void QmlInProcApplicationManagerWindowImpl::setOpacity(qreal opacity)
{
    if (m_opacity != opacity) {
        m_opacity = opacity;
        emit amWindow()->opacityChanged();
    }
}

QColor QmlInProcApplicationManagerWindowImpl::color() const
{
    return m_surfaceItem->color();
}

void QmlInProcApplicationManagerWindowImpl::setColor(const QColor &c)
{
    if (color() != c) {
        m_surfaceItem->setColor(c);
        emit amWindow()->colorChanged();
    }
}

QT_END_NAMESPACE_AM
