/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#if defined(QT_DBUS_LIB)
#  include <QDBusVariant>
#  include <QDBusMessage>
#  include <QDBusConnection>
#  include <QDBusArgument>
#endif

#include <QMetaObject>
#include <QMetaMethod>

#include <QJSValue>
#include <QUrl>

#include <QDebug>

#include "application.h"
#include "utilities.h"
#include "dbus-utilities.h"
#include "applicationipcinterface.h"
#include "applicationipcinterface_p.h"

/*!
    \qmltype ApplicationIPCInterface
    \inqmlmodule QtApplicationManager
    \brief The definition of an IPC interface between the ApplicationManager and applications.

    Using ApplicationIPCInterface items, you can define an IPC interface between your System-UI and
    your applications. The actual interface will be all the properties, signals and functions
    defined within this item. It is however also possible to use this item to wrap already existing
    QtObject or even \l QObject instances by setting the serviceObject property: this will expose
    all properties, signals and functions of the serviceObject instead of the one's in this item.

    Please see the ApplicationIPCManager::registerInterface for an in-depth explanation on how these
    IPC interfaces are set up.
*/

QT_BEGIN_NAMESPACE_AM

ApplicationIPCInterface::ApplicationIPCInterface(QObject *parent)
    : QObject(parent)
{ }

ApplicationIPCInterface::~ApplicationIPCInterface()
{
    delete m_ipcProxy;
}

QString ApplicationIPCInterface::interfaceName() const
{
    return m_ipcProxy ? m_ipcProxy->interfaceName() : QString();
}

QString ApplicationIPCInterface::pathName() const
{
    return m_ipcProxy ? m_ipcProxy->pathName() : QString();
}

bool ApplicationIPCInterface::isValidForApplication(const Application *app) const
{
    return m_ipcProxy ? m_ipcProxy->isValidForApplication(app) : false;
}

/*!
    \qmlproperty QtObject ApplicationIPCInterface::serviceObject

    This property holds the pointer to the object which is exposed on the IPC. By default this
    will return the ApplicationIPCInterface object itself, but setting this property can be used
    to wrap already existing objects in order to expose them as IPC interfaces to applications.
*/
QObject *ApplicationIPCInterface::serviceObject() const
{
    return m_serviceObject;
}

void ApplicationIPCInterface::setServiceObject(QObject *serviceObject)
{
    if (m_serviceObject != serviceObject) {
        m_serviceObject = serviceObject;
        emit serviceObjectChanged();
    }
}

#if defined(QT_DBUS_LIB)
bool ApplicationIPCInterface::dbusRegister(const Application *app, QDBusConnection connection, const QString &debugPathPrefix)
{
    return m_ipcProxy ? m_ipcProxy->dbusRegister(app, connection, debugPathPrefix) : false;
}

bool ApplicationIPCInterface::dbusUnregister(QDBusConnection connection)
{
    return m_ipcProxy ? m_ipcProxy->dbusUnregister(connection) : false;
}
#endif

#define TYPE_ANNOTATION_PREFIX "_decltype_"

QVector<IpcProxyObject *> IpcProxyObject::s_proxies;

static int qmlTypeId(const QString &qmlType)
{
    if (qmlType == qL1S("var") || qmlType == qL1S("variant"))
        return QMetaType::QVariant;
    else if (qmlType == qL1S("string"))
        return QMetaType::QString;
    else if (qmlType == qL1S("int"))
        return QMetaType::Int;
    else if (qmlType == qL1S("bool"))
        return QMetaType::Bool;
    else if (qmlType == qL1S("double"))
        return QMetaType::Double;
    else if (qmlType == qL1S("real"))
        return QMetaType::QReal;
    else if (qmlType == qL1S("url"))
        return QMetaType::QUrl;
    else if (qmlType == qL1S("void"))
        return QMetaType::Void;
    else
        return QMetaType::UnknownType;
}

static const char *dbusType(int typeId)
{
    switch (typeId) {
    case QMetaType::QVariant:
        return "v";
    case QMetaType::QString:
        return "s";
    case QMetaType::Int:
        return "i";
    case QMetaType::Bool:
        return "b";
    case QMetaType::Double:
    case QMetaType::Float:
        return "d";
    case QMetaType::QUrl:
        return "s";
    default:
        return nullptr;
    }
}

IpcProxyObject::IpcProxyObject(QObject *object, const QString &serviceName, const QString &pathName,
                                 const QString &interfaceName, const QVariantMap &filter)
    : m_object(object)
    , m_signalRelay(new IpcProxySignalRelay(this))
    , m_serviceName(serviceName)
    , m_pathName(pathName)
    , m_interfaceName(interfaceName)
{
    s_proxies.append(this);

    m_appIdFilter = variantToStringList(filter.value(qSL("applicationIds")));
    m_categoryFilter = variantToStringList(filter.value(qSL("categories")));
    m_capabilityFilter = variantToStringList(filter.value(qSL("capabilities")));

    const QMetaObject *mo = object->metaObject();
    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        QMetaMethod mm = mo->method(i);

        // check if all parameters map to native D-Bus types
        if ((mm.returnType() != QMetaType::Void) && !dbusType(mm.returnType())) {
            qCWarning(LogQmlIpc) << "Ignoring method" << mm.methodSignature()
                                 << "since its return-type cannot be mapped to a native D-Bus type";
            continue;
        }

        bool paramsOk = true;
        for (int pi = 0; pi < mm.parameterCount(); ++pi) {
            if (!dbusType(mm.parameterType(pi))) {
                qCWarning(LogQmlIpc) << "Ignoring method" << mm.methodSignature()
                                     << "since the type of the parameter" << mm.parameterNames().at(pi)
                                     << "cannot be mapped to a native D-Bus type";
                paramsOk = false;
            }
        }
        if (!paramsOk)
            continue;

        switch (mm.methodType()) {
        case QMetaMethod::Signal: {
            // ignore the unavoidable changed signals for our annotation properties
            if (mm.name().startsWith(TYPE_ANNOTATION_PREFIX))
                continue;

            int propertyIndex = mo->propertyOffset();
            for ( ; propertyIndex < mo->propertyCount(); ++propertyIndex) {
                if (mo->property(propertyIndex).notifySignalIndex() == i)
                    break;
            }

            if (propertyIndex < mo->propertyCount())
                m_signalsToProperties.insert(i, propertyIndex);
            else
                m_signals << i;

            // connect to non-existing virtual slot in the signal relay object
            // this will end up calling ProxySignalRelay::qt_metacall() - see below
            QMetaObject::connect(object, i, m_signalRelay, m_signalRelay->metaObject()->methodCount());
            break;
        }
        case QMetaMethod::Slot:
        case QMetaMethod::Method:
            m_slots << i;
            break;

        default:
            continue;
        }
    }
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        QMetaProperty mp = mo->property(i);
        QByteArray propName = mp.name();

        // handle our annotation mechanism to add types to method parameters
        if (propName.startsWith(TYPE_ANNOTATION_PREFIX)) {
            QByteArray slotName = propName.mid(qstrlen(TYPE_ANNOTATION_PREFIX));
            bool found = false;
            foreach (int slotIndex,  m_slots) {
                QMetaMethod mm = mo->method(slotIndex);
                if (mm.name() == slotName) {
                    found = true;

                    // we found a slot <foo> for the property TYPE_ANNOTATION_PREFIX<foo>
                    QVariantMap typeMap = mp.read(object).toMap();
                    if (typeMap.size() != 1) {
                        qCWarning(LogQmlIpc) << "Found special annotation property" << propName
                                             << "but the QVariantMap / JS object does not have exactly one entry ("
                                             << typeMap.size() << ")";
                        break;
                    }
                    QString returnType = typeMap.constBegin().key();
                    QStringList paramTypes = typeMap.constBegin().value().toStringList();

                    if (mm.parameterCount() != paramTypes.size()) {
                        qCWarning(LogQmlIpc) << "Found special annotation property" << propName
                                             << "but the parameter counts do not match (" << mm.parameterCount()
                                             << "/" << paramTypes.size() << ")";
                        break;
                    }

                    bool typeOk = true;
                    QList<int> typeIds;
                    typeIds << qmlTypeId(returnType);
                    if (typeIds.at(0) == QMetaType::UnknownType) {
                        qCWarning(LogQmlIpc) << "Found special annotation property" << propName
                                             << "but the return type is invalid:" << returnType;
                        typeOk = false;
                    }
                    for (int i = 0; i < paramTypes.size(); ++i) {
                        typeIds << qmlTypeId(paramTypes.at(i));
                        if (typeIds.last() == QMetaType::UnknownType) {
                            qCWarning(LogQmlIpc) << "Found special annotation property" << propName
                                                 << "but the type for parameter" << i << "is invalid: "
                                                 << paramTypes.at(i);
                            typeOk = false;
                        }
                    }

                    if (typeOk)
                        m_slotSignatures.insert(slotIndex, typeIds);
                    break;
                }
            }
            if (!found) {
                qCWarning(LogQmlIpc) << "Found special annotation property" << propName
                                     << "but the annotated function" << slotName << "is missing";
            }
        } else {
            if (dbusType(mp.type())) {
                m_properties << i;
            } else {
                qCWarning(LogQmlIpc) << "Ignoring property" << mp.name()
                                     << "since the type of the parameter" << mp.typeName()
                                     << "cannot be mapped to a native D-Bus type";

            }
        }
    }

    QByteArray xml = createIntrospectionXml();
    m_xmlIntrospection = QLatin1String(xml);

    qCDebug(LogQmlIpc) << "Registering new application interface extension:";
    qCDebug(LogQmlIpc) << "  Name             :" << m_interfaceName;
    qCDebug(LogQmlIpc) << "  AppId filter     :" << m_appIdFilter; //.join(", ");
    qCDebug(LogQmlIpc) << "  Category filter  :" << m_categoryFilter;//.join(", ");
    qCDebug(LogQmlIpc) << "  Capability filter:" << m_capabilityFilter;//.join(", ");
    qCDebug(LogQmlIpc, "%s", xml.constData());
}

IpcProxyObject::~IpcProxyObject()
{
    s_proxies.removeOne(this);
}

QByteArray IpcProxyObject::createIntrospectionXml()
{
    QByteArray xml = "<interface name=\"" + m_interfaceName.toLatin1() + "\">\n";

    const QMetaObject *mo = m_object->metaObject();

    foreach (int i, m_properties) {
        QMetaProperty mp = mo->property(i);

        QByteArray readWrite;
        if (mp.isReadable())
            readWrite += "read";
        if (mp.isWritable())
            readWrite += "write";

        xml = xml + "  <property name=\"" + mp.name()
                + "\" type=\"" + dbusType(mp.type())
                + "\" access=\"" + readWrite
                + "\" />\n";
    }
    foreach (int i, m_signals) {
        QMetaMethod mm = mo->method(i);

        xml = xml + "  <signal name=\"" + mm.name() + "\">\n";
        for (int pi = 0; pi < mm.parameterCount(); ++pi) {
            xml = xml + "    <arg name=\"" + mm.parameterNames().at(pi)
                    + "\" type=\"" + dbusType(mm.parameterType(pi))
                    + "\" />\n";
            if (mm.parameterType(pi) == QMetaType::QVariant) {
                xml = xml + "    <annotation name=\"org.qtproject.QtDBus.QtTypeName.In"
                        + QByteArray::number(pi) + "\" value=\"QVariantMap\"/>\n";
            }
        }
        xml = xml + "  </signal>\n";
    }
    foreach (int i, m_slots) {
        QMetaMethod mm = mo->method(i);
        QList<int> types;
        if (m_slotSignatures.contains(i)) {
            types = m_slotSignatures.value(i);
        } else {
            types << mm.returnType();
            for (int pi = 0; pi < mm.parameterCount(); ++pi)
                types << mm.parameterType(pi);
        }

        xml = xml + "  <method name=\"" + mm.name() + "\">\n";
        for (int pi = 0; pi < types.count(); ++pi) {
            if (pi == 0 && types.at(0) == QMetaType::Void)
                continue;
            xml = xml + "    <arg name=\"" + (pi == 0 ? "result" : mm.parameterNames().at(pi - 1))
                    + "\" type=\"" + dbusType(types.at(pi))
                    + "\" direction=\"" + (pi == 0 ? "out" : "in")
                    + "\" />\n";
            if (mm.parameterType(pi) == QMetaType::QVariant) {
                xml = xml + "    <annotation name=\"org.qtproject.QtDBus.QtTypeName."
                        + (pi ? "In" : "Out") + QByteArray::number(pi ? pi - 1 : 0)
                        + "\" value=\"QVariantMap\"/>\n";
            }
        }

        xml = xml + "  </method>\n";
    }

    xml += "</interface>\n";

    return xml;
}

QObject *IpcProxyObject::object() const
{
    return m_object;
}

QString IpcProxyObject::serviceName() const
{
    return m_serviceName;
}

QString IpcProxyObject::pathName() const
{
    return m_pathName;
}

QString IpcProxyObject::interfaceName() const
{
    return m_interfaceName;
}

QStringList IpcProxyObject::connectionNames() const
{
    return m_connectionNamesToApplicationIds.keys();
}

bool IpcProxyObject::isValidForApplication(const Application *app) const
{
    if (!app)
        return false;

    auto matchStringLists = [](const QStringList &sl1, const QStringList &sl2) -> bool {
        foreach (const QString &s1, sl1) {
            if (sl2.contains(s1))
                return true;
        }
        return false;
    };

    bool appIdOk = m_appIdFilter.isEmpty() || m_appIdFilter.contains(app->id());
    bool categoryOk = m_categoryFilter.isEmpty() || matchStringLists(m_categoryFilter, app->categories());
    bool capabilityOk = m_capabilityFilter.isEmpty() || matchStringLists(m_capabilityFilter, app->capabilities());

    return appIdOk && categoryOk && capabilityOk;
}

#if defined(QT_DBUS_LIB)

bool IpcProxyObject::dbusRegister(const Application *app, QDBusConnection connection, const QString &debugPathPrefix)
{
    if (m_connectionNamesToApplicationIds.contains(connection.name()))
        return false;

    if (!m_serviceName.isEmpty() && connection.interface()) {
        if (!connection.registerService(m_serviceName))
            return false;
    }

    if (connection.registerVirtualObject(debugPathPrefix + m_pathName, this)) {
        m_connectionNamesToApplicationIds.insert(connection.name(), app ? app->id() : QString());
        if (!debugPathPrefix.isEmpty())
            m_pathNamePrefixForConnection.insert(connection.name(), debugPathPrefix);
        return true;
    }
    return false;
}

bool IpcProxyObject::dbusUnregister(QDBusConnection connection)
{
    if (m_connectionNamesToApplicationIds.remove(connection.name())) {
        connection.unregisterObject(m_pathNamePrefixForConnection.value(connection.name()) + m_pathName);
        return true;
    }
    return false;
}

QString IpcProxyObject::introspect(const QString &path) const
{
    if (m_pathName != path) {
        bool found = false;
        for (auto it = m_pathNamePrefixForConnection.cbegin(); it != m_pathNamePrefixForConnection.cend(); ++it) {
            if (path == it.value() + m_pathName) {
                found = true;
                break;
            }
        }
        if (!found)
            return QString();
    }
    return m_xmlIntrospection;
}

bool IpcProxyObject::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    QString interface = message.interface();
    QByteArray function = message.member().toLatin1();
    const QMetaObject *mo = m_object->metaObject();

    m_sender = m_connectionNamesToApplicationIds.value(connection.name());
    struct ClearSender {
        ClearSender(QString &string) : m_string(string) { }
        ~ClearSender() { m_string.clear(); }
        QString &m_string;
    } clearSender(m_sender);

    if (interface == m_interfaceName) {
        // find in registered slots only - not in all methods
        foreach (int mi, m_slots) {
            QMetaMethod mm = mo->method(mi);
            if (mm.name() == function) {
                if (mm.parameterCount() == message.arguments().count()) {
                    bool matched = true;
                    QVariant argsCopy[10]; // we need to convert QDBusVariants
                    QGenericArgument args[10];
                    QList<int> expectedTypes = m_slotSignatures.value(mi);

                    if (expectedTypes.isEmpty()) {
                        // there was no TYPE_ANNOTATION_PREFIX<mm.name()> property - just use Qt's introspection
                        expectedTypes << mm.returnType();
                        for (int i = 0; i < mm.parameterCount(); ++i)
                            expectedTypes << mm.parameterType(i);
                    }
                    for (int ai = 0; ai < mm.parameterCount(); ++ai) {
                        // QDBusVariants have to be converted to plain QVariants first
                        argsCopy[ai] = convertFromDBusVariant(message.arguments().at(ai));

                        // parameter types need to match - the only exception is if we expect
                        // a QVariant, since we can convert the parameter implicitly
                        if (argsCopy[ai].typeName() != QMetaType::typeName(expectedTypes.at(ai + 1))) {
                            if (expectedTypes.at(ai + 1) != QMetaType::QVariant) {
                                matched = false;
                                qWarning() << "MISMATCHED PARAMETER" << ai + 1 << "ON FUNCTION" << function
                                           << "- EXPECTED" << QMetaType::typeName(expectedTypes.at(ai + 1))
                                           << "- RECEIVED" << argsCopy[ai].typeName();
                                break;
                            }
                        }
                        // this is why we need the argsCopy array - args[] saves the data()
                        // pointers of all the parameter QVariants
                        args[ai] = QGenericArgument(argsCopy[ai].typeName(), argsCopy[ai].data());
                    }
                    if (matched) {
                        QVariant result;
                        if (mm.invoke(m_object, Q_RETURN_ARG(QVariant, result),
                                      args[0], args[1], args[2], args[3], args[4],
                                      args[5], args[6], args[7], args[8], args[9])) {

                            if (expectedTypes.at(0) == QMetaType::Void || !result.isValid()) {
                                connection.call(message.createReply(), QDBus::NoBlock);
                            } else {
                                // if we get back a JS value, we need to convert it to a C++
                                // QVariant first.
                                result = convertFromJSVariant(result);

                                connection.call(message.createReply(result), QDBus::NoBlock);
                            }
                            return true;
                        }
                    }
                }
            }
        }

    } else if (interface == qL1S("org.freedesktop.DBus.Properties")) {
        if (message.arguments().at(0) != m_interfaceName)
            return false;

        const QMetaObject *mo = m_object->metaObject();

        if (function == "Get") {
            QByteArray name = message.arguments().at(1).toString().toLatin1();
            QVariant result;

            foreach (int pi, m_properties) {
                QMetaProperty mp = mo->property(pi);
                if (mp.name() == name) {
                    result = convertFromJSVariant(mp.read(m_object));
                    // this seems counter-intuitive, but we have to wrap the QVariant into
                    // QDBusVariant, which has to be wrapped into a QVariant again.
                    QDBusVariant dbusResult = QDBusVariant(result);

                    connection.call(message.createReply(QVariant::fromValue(dbusResult)), QDBus::NoBlock);
                    return true;
                }
            }
            connection.call(message.createErrorReply(QDBusError::UnknownProperty, qL1S("unknown property")));
            return true;

        } else if (function == "GetAll") {
            //TODO
        } else if (function == "Set") {
            QByteArray name = message.arguments().at(1).toString().toLatin1();

            foreach (int pi, m_properties) {
                QMetaProperty mp = mo->property(pi);
                if (mp.name() == name) {
                    if (mp.isWritable()) {
                        QVariant value = convertFromDBusVariant(message.arguments().at(2));
                        if (mp.write(m_object, value))
                            connection.call(message.createReply(), QDBus::NoBlock);
                        else
                            connection.call(message.createErrorReply(QDBusError::InvalidArgs, qL1S("calling QMetaProperty::write() failed")));
                    } else {
                        connection.call(message.createErrorReply(QDBusError::PropertyReadOnly, qL1S("property is read-only")));
                    }

                    return true;
                }
            }
            connection.call(message.createErrorReply(QDBusError::UnknownProperty, qL1S("unknown property")));
            return true;
        }
    }

    return false;
}

#endif // QT_DBUS_LIB

void IpcProxyObject::relaySignal(int signalIndex, void **argv)
{
#if defined(QT_DBUS_LIB)
    QMapIterator<QString, QString> it(m_connectionNamesToApplicationIds);
    while (it.hasNext()) {
        it.next();
        QString connectionName = it.key();
        QString applicationId = it.value();

        // check if we have a receiver filter
        if (!m_receivers.isEmpty()) {
            if (applicationId.isEmpty() || !m_receivers.contains(applicationId))
                continue;
        }

        QDBusConnection connection(connectionName);
        if (connection.isConnected()) {
            int propertyIndex = m_signalsToProperties.value(signalIndex, -1);
            QString pathName = m_pathNamePrefixForConnection.value(connection.name()) + m_pathName;

            if (propertyIndex >= 0) {
                const QMetaProperty mp = m_object->metaObject()->property(propertyIndex);

                QDBusMessage message = QDBusMessage::createSignal(pathName, qSL("org.freedesktop.DBus.Properties"), qSL("PropertiesChanged"));
                message << m_interfaceName
                        << QVariantMap() // { { qL1S(mp.name()), QVariant::fromValue(QDBusVariant(convertFromJSVariant(mp.read(m_object)))) } };
                        << QStringList(qL1S(mp.name()));

                connection.send(message);

            } else {
                const QMetaMethod mm = m_object->metaObject()->method(signalIndex);

                QList<QVariant> args;
                for (int i = 0; i < mm.parameterCount(); ++i) {
                    args << convertFromJSVariant(QVariant(mm.parameterType(i), argv[i + 1]));
                }

                QDBusMessage message = QDBusMessage::createSignal(pathName, m_interfaceName, qL1S(mm.name()));
                message.setArguments(args);

                connection.send(message);
            }
        }
    }
#else
    Q_UNUSED(signalIndex)
    Q_UNUSED(argv)
#endif // QT_DBUS_LIB

    m_receivers.clear();
}


IpcProxySignalRelay::IpcProxySignalRelay(IpcProxyObject *proxyObject)
    : QObject(proxyObject)
    , m_proxyObject(proxyObject)
{ }

int IpcProxySignalRelay::qt_metacall(QMetaObject::Call c, int id, void **argv)
{
    id = QObject::qt_metacall(c, id, argv);
    if (id < 0)
        return id;

    if (c == QMetaObject::InvokeMetaMethod) {
        // we have really have no slots, so 0 is our virtual relay slot
        if (id == 0) {
            QObject *so = sender();
            if (so == m_proxyObject->object()) {
                int si = senderSignalIndex();
                m_proxyObject->relaySignal(si, argv);
            }
        }
        --id;
    }
    return id;
}

ApplicationIPCInterfaceAttached *ApplicationIPCInterface::qmlAttachedProperties(QObject *object)
{
    return new ApplicationIPCInterfaceAttached(object);
}

ApplicationIPCInterfaceAttached::ApplicationIPCInterfaceAttached(QObject *object)
    : m_object(object)
{
}

QString ApplicationIPCInterfaceAttached::sender() const
{
    if (resolveProxy())
        return m_proxy->m_sender;
    return QString();
}

QVariant ApplicationIPCInterfaceAttached::receivers() const
{
    if (resolveProxy())
        return m_proxy->m_receivers;
    return QVariant();
}

void ApplicationIPCInterfaceAttached::setReceivers(const QVariant &receivers)
{
    if (resolveProxy())
        m_proxy->m_receivers = variantToStringList(receivers);
}

QVariant ApplicationIPCInterfaceAttached::inProcessReceiversOnly() const
{
    return QStringList(qSL("{in-process}"));
}

bool ApplicationIPCInterfaceAttached::resolveProxy() const
{
    if (!m_proxy) {
        for (auto &proxy : qAsConst(IpcProxyObject::s_proxies)) {
            if (proxy->object() == m_object) {
                m_proxy = proxy;
                break;
            }
        }
    }
    return (m_proxy);
}

QT_END_NAMESPACE_AM
