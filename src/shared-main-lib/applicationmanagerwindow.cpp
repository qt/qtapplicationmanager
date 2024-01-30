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
    multi-process (Wayland) mode. In order to achieve this, this class is not derived from
    QQuickWindow directly, but from QObject.

    The API closely resembles that of \l Window, but there are some things missing, which are
    either not applicable for the embedded use case or cannot be supported in the single-process
    case. If you need access to those, you still can via the \l backingObject property. Check the
    the \l singleProcess property to see if you are in single-process mode and if the actual
    backingObject is an \l Item or a \l Window.

    In contrast to a \l Window, an ApplicationManagerWindow is visible by default.

    Additional details can be found in the section about \l {The Root Element}{the root element}
    and \l {Application Windows}{application windows}.

    \note Before version 6.7, this class inherited from \l Window in multi-process and from \l
          QtObject in single-process mode, but that made it impossible to use qml tooling on this
          class. There are no "revision" markers in the QML API and no "since" markers in the
          documentation after re-implementing this class, as it would be ambiguous which of the two
          old implementations they refer to.

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
*/


ApplicationManagerWindow::ApplicationManagerWindow(QObject *parent)
    : QObject(parent)
    , m_impl(ApplicationManagerWindowImpl::create(this))
{ }

ApplicationManagerWindow::~ApplicationManagerWindow()
{ }

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

/*!
    \qmlproperty Item ApplicationManagerWindow::contentItem
    \readonly

    The invisible root item of the scene.
*/
QQuickItem *ApplicationManagerWindow::contentItem()
{
    return m_impl->contentItem();
}

/*! \qmlproperty bool ApplicationManagerWindow::singleProcess
    \readonly

    This property is \c true when the application is running in single-process mode and \c false
    when it is running in multi-process mode.

    \sa ApplicationManager::singleProcess
*/
bool ApplicationManagerWindow::isSingleProcess() const
{
    return m_impl->isSingleProcess();
}

/*! \qmlproperty QtObject ApplicationManagerWindow::backingObject
    \readonly

    This property holds the backing object of this window: in single-process mode, this is an
    an \l Item acting as a container for \l contentItem, but in multi-process mode, it is the
    actual \l Window instance.
*/
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

/*!
    \qmlmethod ApplicationManagerWindow::close()

    Closes the window.

    When this method is called, the \l closing signal will be emitted. If there is no handler, or
    the handler does not revoke permission to close, the window will subsequently close. If the
    QGuiApplication::quitOnLastWindowClosed  property is \c true, and if there are no other windows
    open, the application will quit.
*/
void ApplicationManagerWindow::close()
{
    m_impl->close();
}

/*!
    \qmlsignal ApplicationManagerWindow::closing(CloseEvent close)

    This signal is emitted when the user tries to close the window.

    This signal includes a \a close parameter. The \c {close.accepted}
    property is true by default so that the window is allowed to close, but you
    can implement an \c onClosing handler and set \c {close.accepted = false} if
    you need to do something else before the window can be closed.
*/

/*!
    \qmlmethod void ApplicationManagerWindow::hide()

    Hides the window.

    Equivalent to setting \l visible to \c false or \l visibility to \l {QWindow::}{Hidden}.

    \sa show()
*/
void ApplicationManagerWindow::hide()
{
    m_impl->hide();
}

/*!
    \qmlmethod void ApplicationManagerWindow::show()

    Shows the window.

    Equivalent to setting \l visible to \c true.

    \sa hide(), visibility
*/
void ApplicationManagerWindow::show()
{
    m_impl->show();
}

/*!
    \qmlmethod void ApplicationManagerWindow::showFullScreen()

    This call is passed through to the underlying QWindow object in multi-process mode. The
    single-process mode does not support window states, so this call gets mapped to a simple show().

    \sa QWindow::showFullScreen()
*/
void ApplicationManagerWindow::showFullScreen()
{
    m_impl->showFullScreen();
}

/*!
    \qmlmethod void ApplicationManagerWindow::showMinimized()

    This call is passed through to the underlying QWindow object in multi-process mode. The
    single-process mode does not support window states, so this call gets mapped to a simple show().

    \sa QWindow::showMinimized()
*/
void ApplicationManagerWindow::showMinimized()
{
    m_impl->showMinimized();
}

/*!
    \qmlmethod void ApplicationManagerWindow::showMaximized()

    This call is passed through to the underlying QWindow object in multi-process mode. The
    single-process mode does not support window states, so this call gets mapped to a simple show().

    \sa QWindow::showMaximized()
*/
void ApplicationManagerWindow::showMaximized()
{
    m_impl->showMaximized();
}

/*!
    \qmlmethod void ApplicationManagerWindow::showNormal()

    This call is passed through to the underlying QWindow object in multi-process mode. The
    single-process mode does not support window states, so this call gets mapped to a simple show().

    \sa QWindow::showNormal()
*/
void ApplicationManagerWindow::showNormal()
{
    m_impl->showNormal();
}

/*!
    \qmlsignal void ApplicationManagerWindow::frameSwapped()

    This signal is forwarded from the underlying QQuickWindow object: either the corresponding
    QQuickWindow for this object in multi-process mode, or the System UI's QQuickWindow in
    single-process mode.

    \sa QQuickWindow::frameSwapped()
*/

/*!
    \qmlsignal void ApplicationManagerWindow::sceneGraphError(QQuickWindow::SceneGraphError error, string message)

    This signal with its parameters \a error and \a message is forwarded from the underlying
    QQuickWindow object: either the corresponding QQuickWindow for this object in multi-process
    mode, or the System UI's QQuickWindow in single-process mode.

    \sa QQuickWindow::sceneGraphError()
*/

/*!
    \qmlsignal void ApplicationManagerWindow::afterAnimating()

    This signal is forwarded from the underlying QQuickWindow object: either the corresponding
    QQuickWindow for this object in multi-process mode, or the System UI's QQuickWindow in
    single-process mode.
*/

ApplicationManagerWindowImpl *ApplicationManagerWindow::implementation()
{
    return m_impl.get();
}

/*!
    \qmlproperty string ApplicationManagerWindow::title

    The window's title in the windowing system.
*/
QString ApplicationManagerWindow::title() const
{
    return m_impl->title();
}

void ApplicationManagerWindow::setTitle(const QString &title)
{
    m_impl->setTitle(title);
}

/*!
    \qmlproperty int ApplicationManagerWindow::x
    \qmlproperty int ApplicationManagerWindow::y
    \qmlproperty int ApplicationManagerWindow::width
    \qmlproperty int ApplicationManagerWindow::height

    Defines the window's position and size.

    The (x,y) position is relative to the compositors screens.
*/
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

/*!
    \qmlproperty int ApplicationManagerWindow::minimumWidth
    \qmlproperty int ApplicationManagerWindow::minimumHeight

    Defines the window's minimum size.

    This is a hint to the compositor to prevent resizing below the specified
    width and height.
*/
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

/*!
    \qmlproperty int ApplicationManagerWindow::maximumWidth
    \qmlproperty int ApplicationManagerWindow::maximumHeight

    Defines the window's maximum size.

    This is a hint to the compositor to prevent resizing above the specified
    width and height.
*/
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

/*! \qmlproperty bool ApplicationManagerWindow::visible

    Whether the window is visible on the screen.

    Setting visible to false is the same as setting \l visibility to \l {QWindow::}{Hidden}.

    \sa visibility
*/
bool ApplicationManagerWindow::isVisible() const
{
    return m_impl->isVisible();
}

void ApplicationManagerWindow::setVisible(bool visible)
{
    m_impl->setVisible(visible);
}

/*!
    \qmlproperty real ApplicationManagerWindow::opacity

    The opacity of the window.

    A value of 1.0 or above is treated as fully opaque, whereas a value of 0.0 or below
    is treated as fully transparent. Values in between represent varying levels of
    translucency between the two extremes.

    The default value is 1.0.
*/
qreal ApplicationManagerWindow::opacity() const
{
    return m_impl->opacity();
}

void ApplicationManagerWindow::setOpacity(qreal opacity)
{
    m_impl->setOpacity(opacity);
}

/*!
    \qmlproperty color ApplicationManagerWindow::color

    The background color for the window.

    Setting this property is more efficient than using a separate Rectangle.

    \note Since version 6.7, if an alpha channel is used, it needs to be straight alpha. Pre 6.7
          it needed to be pre-multiplied alpha (which is what the \l{Window} QML type still expects)
          in multi-process and straight alpha in single-process mode.
*/
QColor ApplicationManagerWindow::color() const
{
    return m_impl->color();
}

void ApplicationManagerWindow::setColor(const QColor &c)
{
    m_impl->setColor(c);
}

/*!
    \qmlproperty bool ApplicationManagerWindow::active

    The active status of the window.
 */
bool ApplicationManagerWindow::isActive() const
{
    return m_impl->isActive();
}

/*!
    \qmlproperty Item ApplicationManagerWindow::activeFocusItem

    The item which currently has active focus or \c null if there is no item with active focus.
*/
QQuickItem *ApplicationManagerWindow::activeFocusItem() const
{
    return m_impl->activeFocusItem();
}

/*!
    \keyword qml-amwindow-visibility-prop
    \qmlproperty QWindow::Visibility ApplicationManagerWindow::visibility

    The screen-occupation state of the window.

    Visibility is whether the window should appear in the windowing system as
    normal, minimized, maximized, fullscreen or hidden.

    See Window::visibility for more details. In single-process mode, the minimized, maximized and
    fullscreen states are not supported and simply map to normal.
*/
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

/*!
    \qmlattachedproperty ApplicationManagerWindow ApplicationManagerWindow::window
    \readonly

    This attached property holds the item's ApplicationManagerWindow.
    The ApplicationManagerWindow attached property can be attached to any Item.
*/
ApplicationManagerWindow *ApplicationManagerWindowAttached::window() const
{
    return m_amwindow.get();
}

/*!
    \qmlattachedproperty QtObject ApplicationManagerWindow::backingObject
    \readonly

    This attached property holds backingObject of the ApplicationManagerWindow.

    \sa ApplicationManagerWindow::backingObject
*/
QObject *ApplicationManagerWindowAttached::backingObject() const
{
    return m_amwindow ? m_amwindow->backingObject() : nullptr;
}

/*!
    \qmlattachedproperty QWindow::Visibility ApplicationManagerWindow::visibility
    \readonly

    This attached property holds whether the window is currently shown
    in the windowing system as normal, minimized, maximized, fullscreen or
    hidden. The \c ApplicationManagerWindow attached property can be attached to any Item. If the
    item is not shown in any ApplicationManagerWindow, the value will be \l {QWindow::}{Hidden}.

    \sa visible, {qml-amwindow-visibility-prop}{visibility}
*/
QWindow::Visibility ApplicationManagerWindowAttached::visibility() const
{
    return m_amwindow ? m_amwindow->visibility() : QWindow::Hidden;
}

/*!
    \qmlattachedproperty bool ApplicationManagerWindow::active

    This attached property tells whether the window is active. The ApplicationManagerWindow
    attached property can be attached to any Item.
*/
bool ApplicationManagerWindowAttached::isActive() const
{
    return m_amwindow ? m_amwindow->isActive() : false;
}

/*!
    \qmlattachedproperty Item ApplicationManagerWindow::activeFocusItem
    \readonly

    This attached property holds the item which currently has active focus or
    \c null if there is no item with active focus. The ApplicationManagerWindow attached property
    can be attached to any Item.
*/
QQuickItem *ApplicationManagerWindowAttached::activeFocusItem() const
{
    return m_amwindow ? m_amwindow->activeFocusItem() : nullptr;
}

/*!
    \qmlattachedproperty Item ApplicationManagerWindow::contentItem
    \readonly

    This attached property holds the invisible root item of the scene or
    \c null if the item is not in a window. The ApplicationManagerWindow attached property
    can be attached to any Item.
*/
QQuickItem *ApplicationManagerWindowAttached::contentItem() const
{
    return m_amwindow ? m_amwindow->contentItem() : nullptr;
}

/*!
    \qmlattachedproperty int ApplicationManagerWindow::width
    \qmlattachedproperty int ApplicationManagerWindow::height
    \readonly

    These attached properties hold the size of the item's window.
    The ApplicationManagerWindow attached property can be attached to any Item.
*/
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
