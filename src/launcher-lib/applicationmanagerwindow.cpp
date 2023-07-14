// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QGuiApplication>
#include <QDebug>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow.h>

#include "logging.h"
#include "launchermain.h"
#include "applicationmanagerwindow_p.h"

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerWindowPrivate
{
public:
    QPointer<LauncherMain> launcherMain = nullptr;
};


/*!
    \qmltype ApplicationManagerWindow
    \inqmlmodule QtApplicationManager.Application
    \ingroup app
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

ApplicationManagerWindow::ApplicationManagerWindow(QWindow *parent)
    : QQuickWindowQmlImpl(parent)
    , d(new ApplicationManagerWindowPrivate())
{
    setFlags(flags() | Qt::FramelessWindowHint);
    setWidth(1024);
    setHeight(768);
    setVisible(true);

    (void) winId(); // force allocation of platform resources

    d->launcherMain = LauncherMain::instance();
    connect(d->launcherMain, &LauncherMain::windowPropertyChanged,
            this, [this](QWindow *window, const QString &name, const QVariant &value) {
        if (window == this)
            emit windowPropertyChanged(name, value);
    });
}

ApplicationManagerWindow::~ApplicationManagerWindow()
{
    if (d->launcherMain)
        d->launcherMain->clearWindowPropertyCache(this);
    delete d;
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
void ApplicationManagerWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    d->launcherMain->setWindowProperty(this, name, value);
}

/*!
    \qmlmethod var ApplicationManagerWindow::windowProperty(string name)

    Returns the value of this application window's shared property identified by \a name.

    \sa setWindowProperty
*/
QVariant ApplicationManagerWindow::windowProperty(const QString &name) const
{
    return windowProperties().value(name);
}

/*!
    \qmlmethod object ApplicationManagerWindow::windowProperties()

    Returns an object containing all shared properties of this application window.

    \sa setWindowProperty
*/
QVariantMap ApplicationManagerWindow::windowProperties() const
{
    return d->launcherMain->windowProperties(const_cast<ApplicationManagerWindow *>(this));
}

/*!
    \qmlsignal ApplicationManagerWindow::windowPropertyChanged(string name, var value)

    Reports a change of this application window's property identified by \a name to the given
    \a value.

    \sa setWindowProperty
*/


QT_END_NAMESPACE_AM

#include "moc_applicationmanagerwindow_p.cpp"
