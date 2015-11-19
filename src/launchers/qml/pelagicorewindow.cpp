/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QGuiApplication>
#include <QDebug>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow.h>

#include "pelagicorewindow.h"


class PelagicoreWindowPrivate
{
public:
    PelagicoreWindowPrivate()
        : frameCount(0)
        , currentFPS(0)
        , platformNativeInterface(0)
        , platformWindow(0)
    { }

    QString startArgument;

    QTime time;
    int frameCount;
    float currentFPS;

    QPlatformNativeInterface *platformNativeInterface;
    QPlatformWindow *platformWindow;
};


/*!
    \class PelagicoreWindow
    \internal
*/

/*!
    \qmltype PelagicoreWindow
    \inqmlmodule com.pelagicore.ApplicationManager 0.1
    \brief The PelagicoreWindow item

    This QML item should be used as the root item in your QML application. In doing so, you enable
    your application to be usable in both the single-process (EGL fullscreen, X11) and
    multi-process modes (Wayland) and you enable your application to take part in the
    application-manager's MIME-type mechanism.

    If you are not using PelagicoreWindow as the QML root item, your application will only work in
    multi-process (Wayland), plus it cannot be registered as a MIME-type handler.

    The QML import for this item is

    \c{import com.pelagicore.ApplicationManager 0.1}

    After importing, you can instantiate the QML item like so:

    \qml
    import QtQuick 2.0
    import com.pelagicore.ApplicationManager 0.1

    PelagicoreWindow {
        id: root

        Text {
            text: "Started with argument: " + root.startArgument
        }
    }
    \endqml

    In order to make your applications easily runnable outside of the application-manager, even
    though you are using a PelagicoreWindow as a root item, you can simply provide this little
    dummy import to your application.

    \list 1
    \li Pick a base dir and create a directory hierarchy like \c{com/pelagicore/ApplicationManager}
    \li Add a file named \c qmldir there, consisting of the single line \c{PelagicoreWindow 0.1 PelagicoreWindow.qml}
    \li Add a second file named \c PelagicoreWindow.qml, with the following content

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

/*!
    \qmlproperty string PelagicoreWindow::startArgument

    This property holds start argument, supplied to the application by the application-manager.
    This could in theory be anything, but will practically most likely be an URL to a document that
    should be shown by the application. This is especially true, if the application was activated
    via the application-manager's openUrl() MIME-type mechanism.
*/

/*!
    \qmlproperty real PelagicoreWindow::fps

    This property holds the current frames-per-second value: in ideal cases the value should be
    exactly \c 60 when animations are running and \c 0 when the application is idle.
*/

/*
    \fn void PelagicoreWindow::setStartArgument(const QString& arg)
    \internal
*/


PelagicoreWindow::PelagicoreWindow(QWindow *parent)
    : QQuickWindow(parent)
    , d(new PelagicoreWindowPrivate())
{
    setFlags(flags() | Qt::FramelessWindowHint);
    setWidth(1024);
    setHeight(768);

    connect(this, &QQuickWindow::frameSwapped, this, &PelagicoreWindow::onFrameSwapped);

    (void) winId(); // force allocation of platform resources

    d->platformNativeInterface = qApp->platformNativeInterface();
    d->platformWindow = handle();
    if (!d->platformNativeInterface) {
        qWarning() << "ERROR: PelagicoreWindow failed to get a valid QPlatformNativeInterface object";
        return;
    }
    if (!d->platformWindow) {
        qWarning() << "ERROR: PelagicoreWindow failed to get a QPlatformWindow handle for itself";
        return;
    }
    connect(d->platformNativeInterface, &QPlatformNativeInterface::windowPropertyChanged,
            this, &PelagicoreWindow::onWindowPropertyChangedInternal);
}

void PelagicoreWindow::onWindowPropertyChangedInternal(QPlatformWindow *pw, const QString &name)
{
    if (pw == d->platformWindow && d->platformWindow && d->platformNativeInterface) {
        emit windowPropertyChanged(name, d->platformNativeInterface->windowProperty(pw, name));
    }
}

float PelagicoreWindow::fps() const
{
    return d->currentFPS;
}

bool PelagicoreWindow::setWindowProperty(const QString &name, const QVariant &value)
{
    if (!d->platformNativeInterface && !d->platformWindow)
        return false;
    d->platformNativeInterface->setWindowProperty(d->platformWindow, name, value);
    return true;
}

QVariant PelagicoreWindow::windowProperty(const QString &name) const
{
    if (!d->platformNativeInterface && !d->platformWindow)
        return QVariant();
    return d->platformNativeInterface->windowProperty(d->platformWindow, name);
}

QVariantMap PelagicoreWindow::windowProperties() const
{
    if (!d->platformNativeInterface && !d->platformWindow)
        return QVariantMap();
    return d->platformNativeInterface->windowProperties(d->platformWindow);
}

void PelagicoreWindow::onFrameSwapped()
{
    if (d->frameCount == 0) {
        d->time.start();
    } else {
        d->currentFPS =  float(d->frameCount) / d->time.elapsed() * 1000;
        //qDebug("FPS is %f \n", d->currentFPS);
        emit fpsChanged(d->currentFPS);
    }

    d->frameCount++;
}

