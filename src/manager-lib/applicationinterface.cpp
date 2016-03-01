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

/*!
    \qmltype ApplicationInterface
    \inqmlmodule io.qt.ApplicationManager 1.0
    \brief The main interface between apps and the application-manager.

    This item is available for QML applications using the root context property
    named \c applicationInterface. For other native applications, the same interface
    - minus the notification functionality - is available on a private peer-to-peer
    D-Bus interface. ##TODO: add documentation for this P2P Bus
*/

/*!
    \qmlproperty string ApplicationInterface::applicationId

    The application id of your application.
*/

/*!
    \qmlmethod Notification ApplicationInterface::createNotification()

    Calling this function lets you create a Notification object dynamically at runtime.
*/

/*!
    \qmlsignal ApplicationInterface::quit()

    The application-manager will send out this signal to an application to request a
    controlled shutdown. If you are not reacting on this signal in a timely fashion
    (the exact behavior is defined by application-manager's configuration), your
    application will simply be killed.
*/

/*!
    \qmlsignal ApplicationInterface::memoryLowWarning()

    This signal will be sent out whenver a system dependent free-memory threshold has
    been crossed. Your application is excepted to free up as many resources as
    possible in this case: this will most likely involve clearing internal caches.
*/

/*!
    \qmlsignal ApplicationInterface::openDocument(string documentUrl)

    Whenever an already running application is started again with an argument, the
    already running instance will just receive this signal, instead of starting a
    separate application instance.
    The \a documentUrl parameter received by this function can either be the
    \c documentUrl argument of ApplicationManager::startApplication, the \c documentUrl
    field of the \l{Manifest definition}{info.yaml} manifest when calling
    ApplicationManager::startApplication without a \c documentUrl argument or
    the \c target argument of Qt::openUrlExternally, when your application matches
    a MIME-type request.
*/
