// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "applicationmanagerwindow.h"
#include "applicationmanagerwindowimpl.h"

#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE_AM

/*!
    \qmltype ApplicationManagerWindow
    \inqmlmodule QtApplicationManager.Application
    \ingroup app-instantiatable
    \inherits Window
    \brief The window root element of an application.

    This QML item can be used as the root item in your QML application. In doing so, you enable
    your application to be usable in both single-process (EGL fullscreen, desktop) and
    multi-process (Wayland) mode. It inherits from \l Window in multi-process and from \l QtObject
    in single-process mode. In contrast to a \l Window it is visible by default. This documentation
    reflects the Window inheritance. Note that only a subset of the Window type members have been
    added to ApplicationManagerWindow when derived from QtObject. Additional details can be found
    in the section about \l {The Root Element}{the root element} and \l {Application Windows}
    {application windows}.

    The QML import for this item is

    \c{import QtApplicationManager.Application}

    After importing, you can instantiate the QML item like so:

    \qml
    import QtQuick
    import QtApplicationManager.Application

    ApplicationManagerWindow {
        Text {
            text: ApplicationInterface.applicationId
        }
    }
    \endqml

    In order to make your applications easily runnable outside of the application manager, even
    though you are using an ApplicationManagerWindow as a root item, you can simply provide this
    little dummy import to your application.

    \list 1
    \li Pick a base dir and create a \c{QtApplicationManager.Application} directory in it
    \li Add a file named \c qmldir there, consisting of the single line
        \c{ApplicationManagerWindow 2.0 ApplicationManagerWindow.qml}
    \li Add a second file named \c ApplicationManagerWindow.qml, with the following content

    \qml
    import QtQuick

    Window {
        signal windowPropertyChanged
        function setWindowProperty(name, value) {}
        // ... add additional dummy members that are used by your implementation

        width: 1280   // use your screen width here
        height: 600   // use your screen height here
        visible: true
    }
    \endqml

    \endlist

    Now you can run your appication for instance with: \c{qml -I <path to base dir>}
*/

ApplicationManagerWindow::ApplicationManagerWindow(QObject *parent)
    : QObject(parent)
    , m_impl(ApplicationManagerWindowImpl::create(this))
{ }

ApplicationManagerWindow::~ApplicationManagerWindow()
{
    setVisible(false);
}

ApplicationManagerWindowAttached *ApplicationManagerWindow::qmlAttachedProperties(QObject *object)
{
    return new ApplicationManagerWindowAttached(object);
}

QQmlListProperty<QObject> ApplicationManagerWindow::data()
{
    return QQmlListProperty<QObject>(this, nullptr,
                                     ApplicationManagerWindow::data_append,
                                     ApplicationManagerWindow::data_count,
                                     ApplicationManagerWindow::data_at,
                                     ApplicationManagerWindow::data_clear);
}

void ApplicationManagerWindow::data_append(QQmlListProperty<QObject> *property, QObject *object)
{
    auto *that = static_cast<ApplicationManagerWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    itemProperty.append(&itemProperty, object);
    emit that->dataChanged();
}

qsizetype ApplicationManagerWindow::data_count(QQmlListProperty<QObject> *property)
{
    auto *that = static_cast<ApplicationManagerWindow*>(property->object);
    if (!QQuickItemPrivate::get(that->contentItem())->data().count)
        return 0;
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    return itemProperty.count(&itemProperty);
}

QObject *ApplicationManagerWindow::data_at(QQmlListProperty<QObject> *property, qsizetype index)
{
    auto *that = static_cast<ApplicationManagerWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    return itemProperty.at(&itemProperty, index);
}

void ApplicationManagerWindow::data_clear(QQmlListProperty<QObject> *property)
{
    auto *that = static_cast<ApplicationManagerWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    itemProperty.clear(&itemProperty);
    emit that->dataChanged();
}

void ApplicationManagerWindow::classBegin()
{
    m_impl->classBegin();
}

void ApplicationManagerWindow::componentComplete()
{
    m_impl->componentComplete();
}

QQuickItem *ApplicationManagerWindow::contentItem()
{
    return m_impl->contentItem();
}

bool ApplicationManagerWindow::isInProcess() const
{
    return m_impl->isInProcess();
}

QObject *ApplicationManagerWindow::backingObject() const
{
    return m_impl->backingObject();
}

/*!
    \qmlmethod void ApplicationManagerWindow::setWindowProperty(string name, var &value)
    Sets this application window's shared property identified by \a name to the given \a value.

    These properties are shared between the System UI and the client applications: in single-process
    mode simply via a QVariantMap; in multi-process mode via Qt's extended surface Wayland extension.
    Changes from the client side are signalled via windowPropertyChanged.

    See WindowManager for the server side API.

    \note When listening to property changes of Wayland clients on the System UI side, be aware of
          the \l{Multi-process Wayland caveats}{asynchronous nature} of the underlying Wayland
          protocol.

    \sa windowProperty, windowProperties, windowPropertyChanged
*/
bool ApplicationManagerWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    return m_impl->setWindowProperty(name, value);
}

/*!
    \qmlmethod var ApplicationManagerWindow::windowProperty(string name)

    Returns the value of this application window's shared property identified by \a name.

    \sa setWindowProperty
*/
QVariant ApplicationManagerWindow::windowProperty(const QString &name) const
{
    return m_impl->windowProperty(name);
}

/*!
    \qmlmethod object ApplicationManagerWindow::windowProperties()

    Returns an object containing all shared properties of this application window.

    \sa setWindowProperty
*/
QVariantMap ApplicationManagerWindow::windowProperties() const
{
    return m_impl->windowProperties();
}

/*!
    \qmlsignal ApplicationManagerWindow::windowPropertyChanged(string name, var value)

    Reports a change of this application window's property identified by \a name to the given
    \a value.

    \sa setWindowProperty
*/

void ApplicationManagerWindow::close()
{
    m_impl->close();
}

void ApplicationManagerWindow::hide()
{
    m_impl->hide();
}

void ApplicationManagerWindow::show()
{
    m_impl->show();
}

void ApplicationManagerWindow::showFullScreen()
{
    m_impl->showFullScreen();
}

void ApplicationManagerWindow::showMinimized()
{
    m_impl->showMinimized();
}

void ApplicationManagerWindow::showMaximized()
{
    m_impl->showMaximized();
}

void ApplicationManagerWindow::showNormal()
{
    m_impl->showNormal();
}

ApplicationManagerWindowImpl *ApplicationManagerWindow::implementation()
{
    return m_impl.get();
}

QString ApplicationManagerWindow::title() const
{
    return m_impl->title();
}

void ApplicationManagerWindow::setTitle(const QString &title)
{
    m_impl->setTitle(title);
}

int ApplicationManagerWindow::x() const
{
    return m_impl->x();
}

void ApplicationManagerWindow::setX(int x)
{
    m_impl->setX(x);
}

int ApplicationManagerWindow::y() const
{
    return m_impl->y();
}

void ApplicationManagerWindow::setY(int y)
{
    m_impl->setY(y);
}

int ApplicationManagerWindow::width() const
{
    return m_impl->width();
}

void ApplicationManagerWindow::setWidth(int w)
{
    m_impl->setWidth(w);
}

int ApplicationManagerWindow::height() const
{
    return m_impl->height();
}

void ApplicationManagerWindow::setHeight(int h)
{
    m_impl->setHeight(h);
}

int ApplicationManagerWindow::minimumWidth() const
{
    return m_impl->minimumWidth();
}

void ApplicationManagerWindow::setMinimumWidth(int minw)
{
    m_impl->setMinimumWidth(minw);
}

int ApplicationManagerWindow::minimumHeight() const
{
    return m_impl->minimumHeight();
}

void ApplicationManagerWindow::setMinimumHeight(int minh)
{
    m_impl->setMinimumHeight(minh);
}

int ApplicationManagerWindow::maximumWidth() const
{
    return m_impl->maximumWidth();
}

void ApplicationManagerWindow::setMaximumWidth(int maxw)
{
    m_impl->setMaximumWidth(maxw);
}

int ApplicationManagerWindow::maximumHeight() const
{
    return m_impl->maximumHeight();
}

void ApplicationManagerWindow::setMaximumHeight(int maxh)
{
    m_impl->setMaximumHeight(maxh);
}

bool ApplicationManagerWindow::isVisible() const
{
    return m_impl->isVisible();
}

void ApplicationManagerWindow::setVisible(bool visible)
{
    m_impl->setVisible(visible);
}

qreal ApplicationManagerWindow::opacity() const
{
    return m_impl->opacity();
}

void ApplicationManagerWindow::setOpacity(qreal opacity)
{
    m_impl->setOpacity(opacity);
}

QColor ApplicationManagerWindow::color() const
{
    return m_impl->color();
}

void ApplicationManagerWindow::setColor(const QColor &c)
{
    m_impl->setColor(c);
}

bool ApplicationManagerWindow::isActive() const
{
    return m_impl->isActive();
}

QQuickItem *ApplicationManagerWindow::activeFocusItem() const
{
    return m_impl->activeFocusItem();
}

QWindow::Visibility ApplicationManagerWindow::visibility() const
{
    return m_impl->visibility();
}

void ApplicationManagerWindow::setVisibility(QWindow::Visibility newVisibility)
{
    m_impl->setVisibility(newVisibility);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationManagerWindowAttached
///////////////////////////////////////////////////////////////////////////////////////////////////


ApplicationManagerWindowAttached::ApplicationManagerWindowAttached(QObject *attachee)
    : QObject(attachee)
{
    if (auto *attacheeItem = qobject_cast<QQuickItem*>(attachee)) {
        m_impl.reset(ApplicationManagerWindowAttachedImpl::create(this, attacheeItem));

        if (auto *amwindow = m_impl->findApplicationManagerWindow())
            reconnect(amwindow);
    }
}


void ApplicationManagerWindowAttached::reconnect(ApplicationManagerWindow *newWin)
{
    if (newWin == m_amwindow)
        return;

    if (m_amwindow)
        QObject::disconnect(m_amwindow, nullptr, this, nullptr);

    m_amwindow = newWin;
    emit windowChanged();
    emit backingObjectChanged();
    emit contentItemChanged();

    if (m_amwindow) {
        QObject::connect(m_amwindow, &ApplicationManagerWindow::activeChanged,
                         this, &ApplicationManagerWindowAttached::activeChanged);
        QObject::connect(m_amwindow, &ApplicationManagerWindow::activeFocusItemChanged,
                         this, &ApplicationManagerWindowAttached::activeFocusItemChanged);
        QObject::connect(m_amwindow, &ApplicationManagerWindow::widthChanged,
                         this, &ApplicationManagerWindowAttached::widthChanged);
        QObject::connect(m_amwindow, &ApplicationManagerWindow::heightChanged,
                         this, &ApplicationManagerWindowAttached::heightChanged);
        QObject::connect(m_amwindow, &ApplicationManagerWindow::visibilityChanged,
                         this, &ApplicationManagerWindowAttached::visibilityChanged);
    }
}

ApplicationManagerWindow *ApplicationManagerWindowAttached::window() const
{
    return m_amwindow.get();
}

QObject *ApplicationManagerWindowAttached::backingObject() const
{
    return m_amwindow ? m_amwindow->backingObject() : nullptr;
}

QWindow::Visibility ApplicationManagerWindowAttached::visibility() const
{
    return m_amwindow ? m_amwindow->visibility() : QWindow::Hidden;
}

bool ApplicationManagerWindowAttached::isActive() const
{
    return m_amwindow ? m_amwindow->isActive() : false;
}

QQuickItem *ApplicationManagerWindowAttached::activeFocusItem() const
{
    return m_amwindow ? m_amwindow->activeFocusItem() : nullptr;
}

QQuickItem *ApplicationManagerWindowAttached::contentItem() const
{
    return m_amwindow ? m_amwindow->contentItem() : nullptr;
}

int ApplicationManagerWindowAttached::width() const
{
    return m_amwindow ? m_amwindow->height() : 0;
}

int ApplicationManagerWindowAttached::height() const
{
    return m_amwindow ? m_amwindow->height() : 0;
}


QT_END_NAMESPACE_AM

#include "moc_applicationmanagerwindow.cpp"
