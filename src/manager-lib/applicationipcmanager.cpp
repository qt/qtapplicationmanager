/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include "applicationipcmanager.h"
#include "applicationipcinterface.h"
#include "applicationipcinterface_p.h"
#include "qml-utilities.h"
#include "global.h"


/*!
    \class ApplicationIPCManager
    \internal
*/

/*!
    \qmltype ApplicationIPCManager
    \inqmlmodule QtApplicationManager
    \brief The ApplicationIPCManager singleton.

    This singleton type is the central manager for app-to-system-ui IPC interfaces within the application-manager.

    It only exports a single function towards the QML System-UI: registerInterface().

    See ApplicationInterfaceExtension for information on how to access these registered IPC interfaces from
    the client (application) side.
*/

AM_BEGIN_NAMESPACE

ApplicationIPCManager *ApplicationIPCManager::s_instance = 0;

ApplicationIPCManager::~ApplicationIPCManager()
{ }

ApplicationIPCManager *ApplicationIPCManager::createInstance()
{
    if (Q_UNLIKELY(s_instance))
        qFatal("ApplicationIPCManager::createInstance() was called a second time.");

    qmlRegisterType<ApplicationIPCInterfaceAttached>();
    qmlRegisterType<ApplicationIPCInterface>("QtApplicationManager", 1, 0, "ApplicationIPCInterface");
    qmlRegisterSingletonType<ApplicationIPCManager>("QtApplicationManager", 1, 0, "ApplicationIPCManager",
                                                    &ApplicationIPCManager::instanceForQml);
    return s_instance = new ApplicationIPCManager();
}

ApplicationIPCManager *ApplicationIPCManager::instance()
{
    if (Q_UNLIKELY(!s_instance))
        qFatal("ApplicationIPCManager::instance() was called before createInstance().");
    return s_instance;
}

QObject *ApplicationIPCManager::instanceForQml(QQmlEngine *qmlEngine, QJSEngine *)
{
    if (qmlEngine)
        retakeSingletonOwnershipFromQmlEngine(qmlEngine, instance());
    return instance();
}


ApplicationIPCManager::ApplicationIPCManager(QObject *parent)
    : QObject(parent)
{ }

/*!
    \qmlmethod bool ApplicationIPCManager::registerInterface(ApplicationIPCInterface interface, string name, var filter)

    Registers an IPC \a interface object to extend the communication API between applications and
    the Application Manager itself. The \a interface object is an ApplicationIPCInterface item, that needs
    to stay valid during the whole lifetime of the System-UI. The \a name of the interface has to
    adhere to D-Bus standards, so it needs to contain at least one period ('.') character
    (for example, \c{io.qt.test}).
    The interface is available to all applications matching the \a filter criteria (see below)
    on the private Peer-To-Peer D-Bus as a standard, typed D-Bus interface.

    Because there is no way to add type information to the parameters of JavaScript functions, the
    \a interface is scanned for special annotation properties that are only used to deduce type
    information, but are not exported to the applications:

    \c{readonly property var _decltype_<function-name>: { "<return type>": [ "<param 1 type>", "<param 2 type>", ...] }}

    The following types are supported:

    \table
    \header
        \li Type
        \li DBus Type
        \li Note
    \row
        \li \c void
        \li -
        \li Only valid as return type.
    \row
        \li \c int
        \li \c INT32
        \li
    \row
        \li \c string, \c url
        \li \c STRING
        \li Urls are mapped to plain strings.
    \row
        \li \c bool
        \li \c BOOLEAN
        \li
    \row
        \li \c real, \c double
        \li \c DOUBLE
        \li
    \row
        \li \c var, \c variant
        \li \c VARIANT
        \li Can also be used for lists and maps.

    \endtable

    A simple example showing how to add these annotations to a function definition:

    \code
    readonly property var _decltype_testFunction: { "var": [ "int", "string" ] }
    function testFunction(ivar, strvar) {
        return { "strvar": strvar, "intvar": intvar }
    }
    \endcode

    You can restrict the availability of the interface in applications via the \a filter parameter:
    either pass an empty JavaScript object (\c{{}}) or use any combination of these available
    field names:

    \table
    \header
        \li Key
        \li Value
        \li Description
    \row
        \li \c applicationIds
        \li list<string>
        \li A list of applications ids that are allowed to use this interface.
    \row
        \li \c categories
        \li list<string>
        \li A list of category names (see info.yaml). Any application that is part of one of these
            categories is allowed to use this interface.
    \row
        \li \c capabilities
        \li list<string>
        \li A list of applications capabilities (see info.yaml). Any application that has on of these
            capabilities is allowed to use this interface.
    \endtable

    All of the filter fields have to match, but only fields that are actually set are taken into
    consideration.

    \qml
    import QtQuick 2.0
    import QtApplicationManager 1.0

    ApplicationIPCInterface {
        id: extension

        property bool pbool: true
        property double pdouble: 3.14

        signal testSignal(string str, variant list)

        readonly property var _decltype_testFunction: { "void": [ "int", "string" ] }
        function testFunction(foo, bar) {
            console.log("testFunction was called: " + foo + " " + bar)
        }

        Component.onCompleted: {
            ApplicationIPCManager.registerInterface(extension, "io.qt.test.interface",
                                                    { "capabilities": [ "media", "camera" ] })
        }
    }
    \endqml

    Returns \c true if the registration was successful, \c false otherwise.
*/
bool ApplicationIPCManager::registerInterface(AM_PREPEND_NAMESPACE(ApplicationIPCInterface*) interface,
                                              const QString &name, const QVariantMap &filter)
{
    if (!interface || name.isEmpty()) {
        qCWarning(LogQmlIpc) << "Application IPC interfaces need a valid object as well as interface name";
        return false;
    }
    if (!name.contains(QLatin1Char('.'))) {
        qCWarning(LogQmlIpc) << "Application IPC interface name" << name << "is not a valid D-Bus interface name";
        return false;
    }
    foreach (const auto &ext, m_interfaces) {
        if (ext->interfaceName() == name) {
            qCWarning(LogQmlIpc) << "Application IPC interface name" << name << "was already registered";
            return false;
        }
    }

    auto createPathFromName = [](const QString &name) -> QString {
        QString path;

        const QChar *c = name.unicode();
        for (int i = 0; i < name.length(); ++i) {
            ushort u = c[i].unicode();
            path += QLatin1Char(((u >= 'a' && u <= 'z')
                                 || (u >= 'A' && u <= 'Z')
                                 || (u >= '0' && u <= '9')
                                 || (u == '_')) ? u : '_');
        }
        return qSL("/ExtensionInterfaces/") + path;
    };

    interface->m_ipcProxy = new IpcProxyObject(interface, QString(), createPathFromName(name), name, filter);
    m_interfaces.append(interface);
    return true;
}

QVector<ApplicationIPCInterface *> ApplicationIPCManager::interfaces() const
{
    return m_interfaces;
}

AM_END_NAMESPACE
