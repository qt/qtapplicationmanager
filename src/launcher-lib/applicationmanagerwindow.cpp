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

#include <QGuiApplication>
#include <QDebug>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow.h>

#include "logging.h"
#include "applicationmanagerwindow_p.h"

QT_BEGIN_NAMESPACE_AM

class ApplicationManagerWindowPrivate
{
public:
    QPlatformNativeInterface *platformNativeInterface = nullptr;
    QPlatformWindow *platformWindow = nullptr;
};


/*!
    \qmltype ApplicationManagerWindow
    \inqmlmodule QtApplicationManager
    \brief The ApplicationManagerWindow item

    This QML item can be used as the root item in your QML application. In doing so, you enable
    your application to be usable in both single-process (EGL fullscreen, desktop) and
    multi-process (Wayland) mode. It inherits from \l Window in multi-process and from \l Item in
    single-process mode. In contrast to a \l Window it is visible by default. Additional details can
    be found in the section about \l {The Root Element}{the root element}.

    The QML import for this item is

    \c{import QtApplicationManager 1.0}

    After importing, you can instantiate the QML item like so:

    \qml
    import QtQuick 2.0
    import QtApplicationManager 1.0

    ApplicationManagerWindow {
        Text {
            text: ApplicationInterface.applicationId
        }
    }
    \endqml

    In order to make your applications easily runnable outside of the application-manager, even
    though you are using a ApplicationManagerWindow as a root item, you can simply provide this
    little dummy import to your application.

    \list 1
    \li Pick a base dir and create a \c{QtApplicationManager} directory in it
    \li Add a file named \c qmldir there, consisting of the single line \c{ApplicationManagerWindow 1.0 ApplicationManagerWindow.qml}
    \li Add a second file named \c ApplicationManagerWindow.qml, with the following content

    \qml
    import QtQuick 2.0

    Item {
        width: 1280   // use your screen width here
        height: 600   // use your screen height here

        function close() {}
        function showFullScreen() {}
        function showMaximized() {}
        function showNormal() {}
    }
    \endqml

    \endlist

    Now you can run your appication within qmlscene with \c{qmlscene -I <path to base dir>}

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

    d->platformNativeInterface = qApp->platformNativeInterface();
    d->platformWindow = handle();
    if (!d->platformNativeInterface) {
        qCCritical(LogQmlRuntime) << "ApplicationManagerWindow failed to get a valid QPlatformNativeInterface object";
        return;
    }
    if (!d->platformWindow) {
        qCCritical(LogQmlRuntime) << "ApplicationManagerWindow failed to get a QPlatformWindow handle for itself";
        return;
    }
    connect(d->platformNativeInterface, &QPlatformNativeInterface::windowPropertyChanged,
            this, &ApplicationManagerWindow::onWindowPropertyChangedInternal);
}

ApplicationManagerWindow::~ApplicationManagerWindow()
{
    delete d;
}

void ApplicationManagerWindow::onWindowPropertyChangedInternal(QPlatformWindow *pw, const QString &name)
{
    if (pw == d->platformWindow && d->platformWindow && d->platformNativeInterface) {
        emit windowPropertyChanged(name, d->platformNativeInterface->windowProperty(pw, name));
    }
}

/*!
    \qmlmethod bool ApplicationManagerWindow::setWindowProperty(string name, var &value)

    Sets this application window's shared property identified by \a name to the given \a value.

    These properties are shared between the System-UI and the client applications: in single-process
    mode simply via a QVariantMap; in multi-process mode via Qt's extended surface Wayland extension.
    Changes from the client side are signalled via windowPropertyChanged.

    See WindowManager for the server side API.

    \note When listening to property changes of Wayland clients on the System-UI side, be aware of
          the \l{Multi-process Wayland caveats}{asynchronous nature} of the underlying Wayland
          protocol.

    \sa windowProperty, windowProperties, windowPropertyChanged
*/
bool ApplicationManagerWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    if (!d->platformNativeInterface || !d->platformWindow)
        return false;
    d->platformNativeInterface->setWindowProperty(d->platformWindow, name, value);
    return true;
}

/*!
    \qmlmethod var ApplicationManagerWindow::windowProperty(string name)

    Returns the value of this application window's shared property identified by \a name.

    \sa setWindowProperty
*/
QVariant ApplicationManagerWindow::windowProperty(const QString &name) const
{
    if (!d->platformNativeInterface || !d->platformWindow)
        return QVariant();
    return d->platformNativeInterface->windowProperty(d->platformWindow, name);
}

/*!
    \qmlmethod object ApplicationManagerWindow::windowProperties()

    Returns an object containing all shared properties of this application window.

    \sa setWindowProperty
*/
QVariantMap ApplicationManagerWindow::windowProperties() const
{
    if (!d->platformNativeInterface || !d->platformWindow)
        return QVariantMap();
    return d->platformNativeInterface->windowProperties(d->platformWindow);
}

/*!
    \qmlsignal ApplicationManagerWindow::windowPropertyChanged(string name, var value)

    Reports a change of this application window's property identified by \a name to the given
    \a value.

    \sa WindowManager::setWindowProperty
*/


QT_END_NAMESPACE_AM
