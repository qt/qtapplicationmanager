/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include "applicationipcmanager.h"
#include "applicationipcinterface.h"
#include "applicationipcinterface_p.h"
#include "qml-utilities.h"
#include "global.h"


ApplicationIPCManager *ApplicationIPCManager::s_instance = 0;

ApplicationIPCManager::~ApplicationIPCManager()
{ }

ApplicationIPCManager *ApplicationIPCManager::createInstance()
{
    if (s_instance)
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
    the Application Manager itself. The \a interface object is a normal QtObject item, that needs
    to stay valid during the whole lifetime of the system-UI. The \a name of the interface has to
    adhere to D-Bus standards, so it needs to at least contain one \c . character (e.g. \c{io.qt.test}).
    The interface is available to all applications matching the \a filter criteria (see below)
    on the private Peer-To-Peer D-Bus as a standard, typed D-Bus interface.

    Since there is no way to add type information to the parameters of JavaScript functions, the
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

    This is a simple example showing how to add these annotations to a function definition:

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
        \li \c list<string>
        \li A list of applications ids that are allowed to use this interface.
    \row
        \li \c categories
        \li \c list<string>
        \li A list of category names (see info.yaml). Any application that is part of one of these
            categories is allowed to use this interface.
    \row
        \li \c capabilities
        \li \c list<string>
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
    }
    \endqml
    \code
    ...

    ApplicationIPCManager.registerInterface(extension, "io.qt.test.interface",
                                            { "capabilities": [ "media", "camera" ] })

    \endcode

    \note Currently only implemented for multi-process setups.

    Will return \c true if the registration was successful or \c false otherwise.
*/
bool ApplicationIPCManager::registerInterface(ApplicationIPCInterface *interface, const QString &name,
                                              const QVariantMap &filter)
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
    interface->m_ipcProxy = new IpcProxyObject(interface, QString(), qSL("/ExtensionInterfaces"), name, filter);
    m_interfaces.append(interface);
    return true;
}

QVector<ApplicationIPCInterface *> ApplicationIPCManager::interfaces() const
{
    return m_interfaces;
}


