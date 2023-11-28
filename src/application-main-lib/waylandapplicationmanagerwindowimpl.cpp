// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QGuiApplication>
#include <QDebug>
#include <QtQml/QQmlEngine>
#include <QtQml/private/qqmlmetatype_p.h>
#include <QtQuick/QQuickItem>
#include <QtQuick/private/qquickwindowmodule_p.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow.h>

#include "logging.h"
#include "applicationmain.h"
#include "applicationmanagerwindow.h"
#include "waylandapplicationmanagerwindowimpl.h"

QT_BEGIN_NAMESPACE_AM


class AMQuickWindowQmlImpl : public QQuickWindowQmlImpl
{
    Q_OBJECT
public:
    AMQuickWindowQmlImpl(ApplicationManagerWindow *amwindow)
        : QQuickWindowQmlImpl(nullptr)
        , m_amwindow(amwindow)
    { }
    using QQuickWindowQmlImpl::classBegin;
    using QQuickWindowQmlImpl::componentComplete;

    ApplicationManagerWindow *m_amwindow;
};

WaylandApplicationManagerWindowImpl::WaylandApplicationManagerWindowImpl(ApplicationManagerWindow *window,
                                                                         ApplicationMain *applicationMain)
    : ApplicationManagerWindowImpl(window)
    , m_applicationMain(applicationMain)
    , m_qwindow(new AMQuickWindowQmlImpl(window))
{
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::windowTitleChanged,
                     window, &ApplicationManagerWindow::titleChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::xChanged,
                     window, &ApplicationManagerWindow::xChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::yChanged,
                     window, &ApplicationManagerWindow::yChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::widthChanged,
                     window, &ApplicationManagerWindow::widthChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::heightChanged,
                     window, &ApplicationManagerWindow::heightChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::minimumWidthChanged,
                     window, &ApplicationManagerWindow::minimumWidthChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::maximumWidthChanged,
                     window, &ApplicationManagerWindow::maximumWidthChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::minimumHeightChanged,
                     window, &ApplicationManagerWindow::minimumHeightChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::maximumHeightChanged,
                     window, &ApplicationManagerWindow::maximumHeightChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::opacityChanged,
                     window, &ApplicationManagerWindow::opacityChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::visibleChanged,
                     window, &ApplicationManagerWindow::visibleChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::colorChanged,
                     window, &ApplicationManagerWindow::colorChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::activeChanged,
                     window, &ApplicationManagerWindow::activeChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::activeFocusItemChanged,
                     window, &ApplicationManagerWindow::activeFocusItemChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::visibilityChanged,
                     window, &ApplicationManagerWindow::visibilityChanged);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::closing,
                     window, &ApplicationManagerWindow::closing);

    // pass-through signals from the actual QQuickWindow
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::frameSwapped,
                     window, &ApplicationManagerWindow::frameSwapped);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::sceneGraphInitialized,
                     window, &ApplicationManagerWindow::sceneGraphInitialized);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::sceneGraphInvalidated,
                     window, &ApplicationManagerWindow::sceneGraphInvalidated);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::beforeSynchronizing,
                     window, &ApplicationManagerWindow::beforeSynchronizing);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::afterSynchronizing,
                     window, &ApplicationManagerWindow::afterSynchronizing);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::beforeRendering,
                     window, &ApplicationManagerWindow::beforeRendering);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::afterRendering,
                     window, &ApplicationManagerWindow::afterRendering);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::afterAnimating,
                     window, &ApplicationManagerWindow::afterAnimating);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::sceneGraphAboutToStop,
                     window, &ApplicationManagerWindow::sceneGraphAboutToStop);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::beforeFrameBegin,
                     window, &ApplicationManagerWindow::beforeFrameBegin);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::afterFrameEnd,
                     window, &ApplicationManagerWindow::afterFrameEnd);
}

WaylandApplicationManagerWindowImpl::~WaylandApplicationManagerWindowImpl()
{
    if (m_applicationMain && m_qwindow)
        m_applicationMain->clearWindowPropertyCache(m_qwindow);
    delete m_qwindow;
}

bool WaylandApplicationManagerWindowImpl::isInProcess() const
{
    return false;
}

QObject *WaylandApplicationManagerWindowImpl::backingObject() const
{
    return m_qwindow;
}

void WaylandApplicationManagerWindowImpl::classBegin()
{
    QQmlEngine::setContextForObject(m_qwindow, QQmlEngine::contextForObject(amWindow()));

    // find parent QWindow or parent ApplicationManagerWindow
    QQuickWindow *parentWindow = nullptr;
    QObject *p = amWindow()->parent();
    while (p) {
        if (auto *directWindow = qobject_cast<QQuickWindow *>(p)) {
            parentWindow = directWindow;
            break;
        } else if (auto *indirectWindow = qobject_cast<ApplicationManagerWindow *>(p)) {
            parentWindow = static_cast<WaylandApplicationManagerWindowImpl *>(indirectWindow->implementation())->m_qwindow;
            break;
        } else {
            p = p->parent();
        }
    }

    if (parentWindow)
        m_qwindow->setTransientParent(parentWindow);

    m_qwindow->classBegin();

    QObject::connect(m_applicationMain, &ApplicationMain::windowPropertyChanged,
                     amWindow(), [this](QWindow *window, const QString &name, const QVariant &value) {
        if (window == m_qwindow)
            emit amWindow()->windowPropertyChanged(name, value);
    });

    // For historical reasons, we deviate from the standard Window behavior.
    // We cannot set this is in the constructor, because the base class thinks it's
    // componentComplete between the constructor and classBegin.
    m_qwindow->setFlags(m_qwindow->flags() | Qt::FramelessWindowHint);
    m_qwindow->setWidth(1024);
    m_qwindow->setHeight(768);
    m_qwindow->setVisible(true);

    m_qwindow->create(); // force allocation of platform resources
}

void WaylandApplicationManagerWindowImpl::componentComplete()
{
    m_qwindow->componentComplete();
}

QQuickItem *WaylandApplicationManagerWindowImpl::contentItem()
{
    return m_qwindow ? m_qwindow->contentItem() : nullptr;
}

bool WaylandApplicationManagerWindowImpl::setWindowProperty(const QString &name, const QVariant &value)
{
    return m_applicationMain->setWindowProperty(m_qwindow, name, value);
}

QVariant WaylandApplicationManagerWindowImpl::windowProperty(const QString &name) const
{
    return windowProperties().value(name);
}

QVariantMap WaylandApplicationManagerWindowImpl::windowProperties() const
{
    return m_applicationMain->windowProperties(m_qwindow);
}

void WaylandApplicationManagerWindowImpl::close()
{
    m_qwindow->close();
}

QWindow::Visibility WaylandApplicationManagerWindowImpl::visibility() const
{
    return m_qwindow->visibility();
}

void WaylandApplicationManagerWindowImpl::setVisibility(QWindow::Visibility newVisibility)
{
    m_qwindow->setVisibility(newVisibility);
}

void WaylandApplicationManagerWindowImpl::hide()
{
    m_qwindow->hide();
}

void WaylandApplicationManagerWindowImpl::show()
{
    m_qwindow->show();
}

void WaylandApplicationManagerWindowImpl::showFullScreen()
{
    m_qwindow->showFullScreen();
}

void WaylandApplicationManagerWindowImpl::showMinimized()
{
    m_qwindow->showMinimized();
}

void WaylandApplicationManagerWindowImpl::showMaximized()
{
    m_qwindow->showMaximized();
}

void WaylandApplicationManagerWindowImpl::showNormal()
{
    m_qwindow->showNormal();
}

void WaylandApplicationManagerWindowImpl::update()
{
    m_qwindow->update();
}

void WaylandApplicationManagerWindowImpl::releaseResources()
{
    m_qwindow->releaseResources();
}

void WaylandApplicationManagerWindowImpl::requestUpdate()
{
    m_qwindow->requestUpdate();
}

QString WaylandApplicationManagerWindowImpl::title() const
{
    return m_qwindow->title();
}

void WaylandApplicationManagerWindowImpl::setTitle(const QString &title)
{
    m_qwindow->setTitle(title);
}

int WaylandApplicationManagerWindowImpl::x() const
{
    return m_qwindow->x();
}

void WaylandApplicationManagerWindowImpl::setX(int x)
{
    m_qwindow->setX(x);
}

int WaylandApplicationManagerWindowImpl::y() const
{
    return m_qwindow->y();
}

void WaylandApplicationManagerWindowImpl::setY(int y)
{
    m_qwindow->setY(y);
}

int WaylandApplicationManagerWindowImpl::width() const
{
    return m_qwindow->width();
}

void WaylandApplicationManagerWindowImpl::setWidth(int w)
{
    m_qwindow->setWidth(w);
}

int WaylandApplicationManagerWindowImpl::height() const
{
    return m_qwindow->height();
}

void WaylandApplicationManagerWindowImpl::setHeight(int h)
{
    m_qwindow->setHeight(h);
}

int WaylandApplicationManagerWindowImpl::minimumWidth() const
{
    return m_qwindow->minimumWidth();
}

void WaylandApplicationManagerWindowImpl::setMinimumWidth(int minw)
{
    m_qwindow->setMinimumWidth(minw);
}

int WaylandApplicationManagerWindowImpl::minimumHeight() const
{
    return m_qwindow->minimumHeight();
}

void WaylandApplicationManagerWindowImpl::setMinimumHeight(int minh)
{
    m_qwindow->setMinimumHeight(minh);
}

int WaylandApplicationManagerWindowImpl::maximumWidth() const
{
    return m_qwindow->maximumWidth();
}

void WaylandApplicationManagerWindowImpl::setMaximumWidth(int maxw)
{
    m_qwindow->setMaximumWidth(maxw);
}

int WaylandApplicationManagerWindowImpl::maximumHeight() const
{
    return m_qwindow->maximumHeight();
}

void WaylandApplicationManagerWindowImpl::setMaximumHeight(int maxh)
{
    m_qwindow->setMaximumHeight(maxh);
}

bool WaylandApplicationManagerWindowImpl::isVisible() const
{
    return m_qwindow->isVisible();
}

void WaylandApplicationManagerWindowImpl::setVisible(bool visible)
{
    m_qwindow->setVisible(visible);
}

qreal WaylandApplicationManagerWindowImpl::opacity() const
{
    return m_qwindow->opacity();
}

void WaylandApplicationManagerWindowImpl::setOpacity(qreal opactity)
{
    m_qwindow->setOpacity(opactity);
}

QColor WaylandApplicationManagerWindowImpl::color() const
{
    return m_qwindow->color();
}

void WaylandApplicationManagerWindowImpl::setColor(const QColor &c)
{
    m_qwindow->setColor(c);
}

bool WaylandApplicationManagerWindowImpl::isActive() const
{
    return m_qwindow->isActive();
}

QQuickItem *WaylandApplicationManagerWindowImpl::activeFocusItem() const
{
    return m_qwindow->activeFocusItem();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// WaylandApplicationManagerWindowAttachedImpl
///////////////////////////////////////////////////////////////////////////////////////////////////


WaylandApplicationManagerWindowAttachedImpl::WaylandApplicationManagerWindowAttachedImpl(ApplicationManagerWindowAttached *windowAttached, QQuickItem *attacheeItem)
    : ApplicationManagerWindowAttachedImpl(windowAttached, attacheeItem)
{ }

ApplicationManagerWindow *WaylandApplicationManagerWindowAttachedImpl::findApplicationManagerWindow()
{
    if (!attacheeItem())
        return nullptr;

    QObject::connect(attacheeItem(), &QQuickItem::windowChanged,
                     amWindowAttached(), [this](QQuickWindow *newWin) {
        auto *quickWindow = qobject_cast<AMQuickWindowQmlImpl *>(newWin);
        onWindowChanged(quickWindow ? quickWindow->m_amwindow : nullptr);
    });
    auto *quickWindow = qobject_cast<AMQuickWindowQmlImpl *>(attacheeItem()->window());
    return quickWindow ? quickWindow->m_amwindow : nullptr;
}

QT_END_NAMESPACE_AM

#include "waylandapplicationmanagerwindowimpl.moc"
