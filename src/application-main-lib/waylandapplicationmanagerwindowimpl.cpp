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
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::afterAnimating,
                     window, &ApplicationManagerWindow::afterAnimating);
    QObject::connect(m_qwindow, &AMQuickWindowQmlImpl::sceneGraphError,
                     window, &ApplicationManagerWindow::sceneGraphError);

    QObject::connect(m_qwindow, &QObject::destroyed,
                     window, [this, deadWindow = m_qwindow.get()](QObject *) {
        if (m_applicationMain)
            m_applicationMain->clearWindowPropertyCache(deadWindow);
    });
}

WaylandApplicationManagerWindowImpl::~WaylandApplicationManagerWindowImpl()
{
    if (m_applicationMain && m_qwindow)
        m_applicationMain->clearWindowPropertyCache(m_qwindow.get());
    delete m_qwindow.get();
}

bool WaylandApplicationManagerWindowImpl::isSingleProcess() const
{
    return false;
}

QObject *WaylandApplicationManagerWindowImpl::backingObject() const
{
    return m_qwindow.get();
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
    if (m_qwindow)
        m_qwindow->close();
}

QWindow::Visibility WaylandApplicationManagerWindowImpl::visibility() const
{
    return m_qwindow ? m_qwindow->visibility() : QWindow::Hidden;
}

void WaylandApplicationManagerWindowImpl::setVisibility(QWindow::Visibility newVisibility)
{
    if (m_qwindow)
        m_qwindow->setVisibility(newVisibility);
}

void WaylandApplicationManagerWindowImpl::hide()
{
    if (m_qwindow)
        m_qwindow->hide();
}

void WaylandApplicationManagerWindowImpl::show()
{
    if (m_qwindow)
        m_qwindow->show();
}

void WaylandApplicationManagerWindowImpl::showFullScreen()
{
    if (m_qwindow)
        m_qwindow->showFullScreen();
}

void WaylandApplicationManagerWindowImpl::showMinimized()
{
    if (m_qwindow)
        m_qwindow->showMinimized();
}

void WaylandApplicationManagerWindowImpl::showMaximized()
{
    if (m_qwindow)
        m_qwindow->showMaximized();
}

void WaylandApplicationManagerWindowImpl::showNormal()
{
    if (m_qwindow)
        m_qwindow->showNormal();
}

QString WaylandApplicationManagerWindowImpl::title() const
{
    return m_qwindow ? m_qwindow->title() : QString { };
}

void WaylandApplicationManagerWindowImpl::setTitle(const QString &title)
{
    if (m_qwindow)
        m_qwindow->setTitle(title);
}

int WaylandApplicationManagerWindowImpl::x() const
{
    return m_qwindow ? m_qwindow->x() : 0;
}

void WaylandApplicationManagerWindowImpl::setX(int x)
{
    if (m_qwindow)
        m_qwindow->setX(x);
}

int WaylandApplicationManagerWindowImpl::y() const
{
    return m_qwindow ? m_qwindow->y() : 0;
}

void WaylandApplicationManagerWindowImpl::setY(int y)
{
    if (m_qwindow)
        m_qwindow->setY(y);
}

int WaylandApplicationManagerWindowImpl::width() const
{
    return m_qwindow ? m_qwindow->width() : 0;
}

void WaylandApplicationManagerWindowImpl::setWidth(int w)
{
    if (m_qwindow)
        m_qwindow->setWidth(w);
}

int WaylandApplicationManagerWindowImpl::height() const
{
    return m_qwindow ? m_qwindow->height() : 0;
}

void WaylandApplicationManagerWindowImpl::setHeight(int h)
{
    if (m_qwindow)
        m_qwindow->setHeight(h);
}

int WaylandApplicationManagerWindowImpl::minimumWidth() const
{
    return m_qwindow ? m_qwindow->minimumWidth() : 0;
}

void WaylandApplicationManagerWindowImpl::setMinimumWidth(int minw)
{
    if (m_qwindow)
        m_qwindow->setMinimumWidth(minw);
}

int WaylandApplicationManagerWindowImpl::minimumHeight() const
{
    return m_qwindow ? m_qwindow->minimumHeight() : 0;
}

void WaylandApplicationManagerWindowImpl::setMinimumHeight(int minh)
{
    if (m_qwindow)
        m_qwindow->setMinimumHeight(minh);
}

int WaylandApplicationManagerWindowImpl::maximumWidth() const
{
    return m_qwindow ? m_qwindow->maximumWidth() : 0;
}

void WaylandApplicationManagerWindowImpl::setMaximumWidth(int maxw)
{
    if (m_qwindow)
        m_qwindow->setMaximumWidth(maxw);
}

int WaylandApplicationManagerWindowImpl::maximumHeight() const
{
    return m_qwindow ? m_qwindow->maximumHeight() : 0;
}

void WaylandApplicationManagerWindowImpl::setMaximumHeight(int maxh)
{
    if (m_qwindow)
        m_qwindow->setMaximumHeight(maxh);
}

bool WaylandApplicationManagerWindowImpl::isVisible() const
{
    return m_qwindow ? m_qwindow->isVisible() : false;
}

void WaylandApplicationManagerWindowImpl::setVisible(bool visible)
{
    if (m_qwindow)
        m_qwindow->setVisible(visible);
}

qreal WaylandApplicationManagerWindowImpl::opacity() const
{
    return m_qwindow ? m_qwindow->opacity() : qreal(1);
}

void WaylandApplicationManagerWindowImpl::setOpacity(qreal opactity)
{
    if (m_qwindow)
        m_qwindow->setOpacity(opactity);
}

QColor WaylandApplicationManagerWindowImpl::color() const
{
    return m_qwindow ? m_qwindow->color() : QColor { };
}

void WaylandApplicationManagerWindowImpl::setColor(const QColor &c)
{
    if (m_qwindow)
        m_qwindow->setColor(c);
}

bool WaylandApplicationManagerWindowImpl::isActive() const
{
    return m_qwindow ? m_qwindow->isActive(): false;
}

QQuickItem *WaylandApplicationManagerWindowImpl::activeFocusItem() const
{
    return m_qwindow ? m_qwindow->activeFocusItem() : nullptr;
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
