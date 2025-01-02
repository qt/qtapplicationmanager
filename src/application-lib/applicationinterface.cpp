// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

/*!
    \qmltype ApplicationInterface
    \inqmlmodule QtApplicationManager.Application
    \ingroup app-singletons
    \brief The main interface between apps and the application manager.

    This item is available for QML applications using the root context property
    \c ApplicationInterface. For other native applications, the same interface
    - minus the notification functionality - is available on a private peer-to-peer
    D-Bus interface.

    For every application that is started in multi-process mode, the application manager creates
    a private P2P D-Bus connection and communicates the connection address to the application's
    process as part of the YAML snippet in the environment variable \c AM_CONFIG. Only the
    application itself is able to connect to this P2P D-Bus - no further access policies are
    required on this bus.

    Using this connection, you will have access to different interfaces (note that due to
    this not being a bus, the service name is always an empty string):

    \table
    \header
        \li Path \br Name
        \li Description
    \row
        \li \b /ApplicationInterface \br \e io.qt.ApplicationManager.ApplicationInterface
        \li Exactly this interface in D-Bus form. The definition is in the source distribution at
            \c{src/dbus/io.qt.applicationmanager.applicationinterface.xml}
    \row
        \li \b /RuntimeInterface \br \e io.qt.ApplicationManager.RuntimeInterface
        \li The direct interface between the application manager and the launcher process, used to
            implement custom launchers: the definition is in the source distribution at
            \c{src/dbus/io.qt.applicationmanager.runtimeinterface.xml}
    \endtable

    If you are re-implementing the client side, note that the remote interfaces are not
    available immediately after connecting: they are registered on the server side only after the
    client connects. This is a limitation of the D-Bus design - the default implementation attempts
    to connect for 100ms before throwing an error.
*/

/*!
    \qmlproperty string ApplicationInterface::applicationId
    \readonly

    The application id of your application.
*/

/*!
    \qmlproperty var ApplicationInterface::name
    \readonly

    An object containing language (\c string) to application name (\c string) mapppings. See
    \l{application-name-map}{name} in the manifest (info.yaml) definition.
*/

/*!
    \qmlproperty url ApplicationInterface::icon
    \readonly

    The URL of the application's icon as given in the manifest. This can be used as the source
    property of an Image.
*/

/*!
    \qmlproperty string ApplicationInterface::version
    \readonly

    The version of the application as specified in the manifest.
*/

/*!
    \qmlproperty var ApplicationInterface::systemProperties
    \readonly

    Returns the project specific \l{system properties} that were set via the config file.
*/

/*!
    \qmlproperty var ApplicationInterface::applicationProperties
    \readonly

    Returns an object with properties that have been listed under the \c applicationProperties field in the
    \l{Manifest Definition}{manifest} file (info.yaml) of the application.
*/

/*!
    \qmlmethod Notification ApplicationInterface::createNotification()

    Calling this function lets you create a \l Notification object dynamically at runtime.
*/

/*!
    \qmlmethod ApplicationInterface::acknowledgeQuit()

    This method should be called in response to the \l quit() signal, once the application
    is ready to be terminated (e.g. persistent data has been written).

    \note This method should be called instead of \c Qt.quit() to obtain the same
    behavior in single- and multi-process mode (it does nothing in single process mode).

    \sa quit()
*/


/*!
    \qmlsignal ApplicationInterface::quit()

    The application manager will send out this signal to an application to request a
    controlled shutdown. The application is given a certain amount of time defined in
    the configuration (\c quitTime). If the time elapses before acknowledgeQuit() is
    called, the application will simply be killed.

    \sa acknowledgeQuit()
*/

/*!
    \qmlsignal ApplicationInterface::memoryLowWarning()

    This signal will be sent out whenever a system dependent free-memory threshold has
    been crossed. Your application is expected to free up as many resources as
    possible in this case: this will most likely involve clearing internal caches.

    \sa memoryCriticalWarning()
*/

/*!
    \qmlsignal ApplicationInterface::memoryCriticalWarning()

    This signal will be sent out whenever a system dependent free-memory threshold has
    been crossed. It is usually sent after a \c memoryLowWarninig and should
    be perceived as a last notice to urgently free as many resources as possible to
    keep the system stable.

    \sa memoryLowWarning()
*/

/*!
    \qmlsignal ApplicationInterface::openDocument(string documentUrl, string mimeType)

    Whenever an already running application is started again with an argument, the
    already running instance will just receive this signal, instead of starting a
    separate application instance.
    The \a documentUrl parameter received by this function can either be the
    \c documentUrl argument of ApplicationManager::startApplication, the \c documentUrl
    field of the \l{Manifest definition}{info.yaml} manifest when calling
    ApplicationManager::startApplication without a \c documentUrl argument or
    the \c target argument of Qt::openUrlExternally, when your application matches
    a MIME-type request. In the latter case \a mimeType contains the MIME-type detected
    by the application manager.
*/

#include "moc_applicationinterface.cpp"
