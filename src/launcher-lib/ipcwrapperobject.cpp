/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include <QMetaProperty>
#include <QMetaMethod>
#include <QDBusInterface>
#include <QDBusArgument>
#include <private/qmetaobjectbuilder_p.h>
#include <QDebug>
#include <QJSValue>
#include <QUrl>

#include "global.h"
#include "dbus-utilities.h"
#include "ipcwrapperobject.h"
#include "ipcwrapperobject_p.h"


#if 0 // very useful for debugging meta-object problems
static QString stringify(const QVariant &value, int level = 0, bool indentFirstLine = false);

static QString stringifyQObject(const QObject *o, const QMetaObject *mo, int level, bool indentFirstLine)
{
    QString str;
    QString indent = QString(level * 2, QLatin1Char(' '));
    QString nextIndent = QString((level + 1) * 2, QLatin1Char(' '));

    if (indentFirstLine)
        str.append(indent);


    str = str + QLatin1String(mo->className()) + QLatin1String(" {\n");

    if (mo->superClass()) {
        str = str
            + nextIndent
            + QLatin1String("superclass: ")
            + stringifyQObject(o, mo->superClass(), level + 1, false)
            + QLatin1String("\n");
    }

    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        QMetaProperty p = mo->property(i);
        str = str
            + nextIndent
            + QLatin1String("property: ") + QLatin1String(p.typeName()) + QLatin1String(" ") + QLatin1String(p.name()) + QLatin1String(" = ") + stringify(p.read(o), level + 1, false)
                + (p.hasNotifySignal() ? QLatin1String("  [notify via: ") + p.notifySignal().name() + QLatin1String("]") : QString())
            + QLatin1String("\n");
    }

    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        QMetaMethod m = mo->method(i);
        str += nextIndent;
        switch (m.methodType()) {
        case QMetaMethod::Signal: str += QLatin1String("signal"); break;
        case QMetaMethod::Slot:   str += QLatin1String("slot"); break;
        case QMetaMethod::Method: str += QLatin1String("method"); break;
        default: continue;
        }
        str = str + QLatin1String(": ") + QLatin1String(QMetaType::typeName(m.returnType()))
                + QLatin1String(" ") + QLatin1String(m.methodSignature()) + QLatin1String("\n");
    }

    str.append(indent);
    str.append(QLatin1String("}"));
    return str;
}


static QString stringify(const QVariant &value, int level, bool indentFirstLine)
{
    QString str;
    QString indent = QString(level * 2, QLatin1Char(' '));
    QString nextIndent = QString((level + 1) * 2, QLatin1Char(' '));

    if (indentFirstLine)
        str.append(indent);

    switch (int(value.type())) {
    case QVariant::List: {
        str.append(QLatin1String("[\n"));

        QListIterator<QVariant> it(value.toList());
        while (it.hasNext()) {
            str.append(nextIndent);
            str.append(stringify(it.next(), level + 1, true));
            if (it.hasNext())
                str.append(QLatin1Char(','));
            str.append(QLatin1Char('\n'));
        }
        str.append(indent);
        str.append(QLatin1Char(']'));
        break;
    }
    case QVariant::Map: {
        str.append(QLatin1String("{\n"));

        QMapIterator<QString, QVariant> it(value.toMap());
        while (it.hasNext()) {
            it.next();
            str.append(nextIndent);
            str.append(QString::fromLatin1("%1: %2").arg(it.key(), stringify(it.value(), level + 1, false)));
            if (it.hasNext())
                str.append(QLatin1Char(','));
            str.append(QLatin1Char('\n'));
        }
        str.append(indent);
        str.append(QLatin1Char('}'));
        break;
    }
    case QVariant::String:
        str += QString::fromLatin1("\"%1\"").arg(value.toString());
        break;
    case QMetaType::QObjectStar: {
        QObject *o = qvariant_cast<QObject *>(value);
        if (!o) {
            str += QLatin1String("<invalid QObject>");
            break;
        }

        str += stringifyQObject(o, o->metaObject(), level, false);
        break;
    }
    default:
        str += value.toString();
        break;
    }
    return str;
}
#endif // debug only


QT_BEGIN_NAMESPACE_AM

QVector<QMetaObject *> IpcWrapperObject::s_allMetaObjects;

IpcWrapperObject::IpcWrapperObject(const QString &service, const QString &path, const QString &interface, const QDBusConnection &connection, QObject *parent)
    : QObject(parent)
    , m_wrapperHelper(new IpcWrapperSignalRelay(this))
    , m_dbusInterface(new QDBusInterface(service, path, interface, connection, this))
{
    m_dbusInterface->connection().connect(service, path, qSL("org.freedesktop.DBus.Properties"), qSL("PropertiesChanged"),
                                          m_wrapperHelper, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    QMetaObjectBuilder mob;
    mob.setFlags(QMetaObjectBuilder::DynamicMetaObject);

    const QMetaObject *mo = m_dbusInterface->metaObject();

    mob.setClassName(mo->className());
    mob.setSuperClass(&QObject::staticMetaObject);

    for (int i = mo->classInfoOffset(); i < mo->classInfoCount(); ++i) {
        auto mci = mo->classInfo(i);
        mob.addClassInfo(mci.name(), mci.value());
    }

    // properties first, since we create "virtual" change signals for QML
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        QMetaProperty mp = mo->property(i);

        QByteArray typeName = mp.typeName();
        if (mp.userType() == qMetaTypeId<QDBusVariant>())
            typeName = "QVariant";

        QMetaMethodBuilder mmb = mob.addSignal(QByteArray(mp.name()) + "Changed()");
        auto mpb = mob.addProperty(mp.name(), typeName);
        mpb.setNotifySignal(mmb);
        mpb.setWritable(mp.isWritable());
    }

    // signals before slots (moc compatibility)
    for (int pass = 1; pass <= 2; ++pass) {
        for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
            QMetaMethod mm = mo->method(i);
            if ((pass == 1) == (mm.methodType() == QMetaMethod::Signal)) {
                QByteArray resultTypeName = QMetaType::typeName(mm.returnType());
                if (mm.returnType() == qMetaTypeId<QDBusVariant>())
                    resultTypeName = "QVariant";

                QByteArray params = mm.methodSignature();
                params.replace("QDBusVariant", "QVariant"); //TODO: find out how to split up first

                switch (mm.methodType()) {
                case QMetaMethod::Slot:
                case QMetaMethod::Method:
                    mob.addMethod(params, resultTypeName);
                    break;
                case QMetaMethod::Signal: {
                    auto mbb = mob.addSignal(params);
                    mbb.setParameterNames(mm.parameterNames());
                    QMetaObject::connect(m_dbusInterface, mm.methodIndex(), this, mbb.index());
                    break;
                }
                case QMetaMethod::Constructor:
                    mob.addConstructor(params);
                    break;
                }
            }
        }
    }

    m_metaObject = mob.toMetaObject();

    if (s_allMetaObjects.isEmpty()) {
        // we cannot simply delete the meta-objects in the destructor, since the QML engine
        // might hold a pointer to it for caching purposes.
        atexit([]() { std::for_each(s_allMetaObjects.cbegin(), s_allMetaObjects.cend(), free); });
    }
    s_allMetaObjects << m_metaObject;
}

IpcWrapperObject::~IpcWrapperObject()
{ }

bool IpcWrapperObject::isDBusValid() const
{
#if 0 // debug only -- see above
    qWarning(qPrintable(stringifyQObject(this, metaObject(), 2, false)));
#endif
    return m_dbusInterface->isValid();
}

QDBusError IpcWrapperObject::lastDBusError() const
{
    return m_dbusInterface->lastError();
}

const QMetaObject *IpcWrapperObject::metaObject() const
{
    return m_metaObject;
}

int IpcWrapperObject::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        QMetaMethod mm = metaObject()->method(metaObject()->methodOffset() + _id);
        if (mm.methodType() == QMetaMethod::Signal) {
            metaObject()->activate(this, mm.methodIndex(), _a);
            return -1;
        }
    }

    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;

    const QMetaObject *dbusmo = m_dbusInterface->metaObject();

    switch (_c) {
    case QMetaObject::ReadProperty: {
        QMetaProperty dbusmp = dbusmo->property(dbusmo->propertyOffset() + _id);
        QMetaProperty mp = metaObject()->property(metaObject()->propertyOffset() + _id);
        QVariant value = dbusmp.read(m_dbusInterface);
        value = convertFromDBusVariant(value);
        QMetaType::construct(int(mp.type()), _a[0], value.data());
        break;
    }
    case QMetaObject::WriteProperty: {
        QMetaProperty mp = dbusmo->property(dbusmo->propertyOffset() + _id);
        int valueType = mp.userType();
        if (valueType == qMetaTypeId<QDBusVariant>())
            valueType = QMetaType::QVariant;
        QVariant value(valueType, _a[0]);
        value = convertFromJSVariant(value);
        if (mp.userType() == qMetaTypeId<QDBusVariant>()) {
            QDBusVariant dbv = QDBusVariant(value);
            value = QVariant::fromValue(dbv);
        }
        // the boolean 'was-successful' return code is stored in _a[1]
        _a[1] = reinterpret_cast<void *>(mp.write(m_dbusInterface, value) ? intptr_t(1) : intptr_t(0));
        break;
    }
    case QMetaObject::InvokeMetaMethod: {
        QByteArray name = metaObject()->method(metaObject()->methodOffset() + _id).methodSignature();
        QMetaMethod mm = dbusmo->method(dbusmo->indexOfMethod(name));

        QList<QVariant> args;
        for (int i = 0; i < mm.parameterCount(); ++i) {
            args << QVariant(mm.parameterType(i), _a[i + 1]);
        }

        QDBusMessage reply = m_dbusInterface->callWithArgumentList(QDBus::Block, qL1S(mm.name()), args);
        QVariant value;
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().empty()) {

            value = convertFromDBusVariant(reply.arguments().at(0));
        }
        QMetaType::construct(mm.returnType(), _a[0], value.data());
        break;
    }
    default:
        break;
    }
    return -1;
}


void IpcWrapperObject::onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed, const QStringList &invalidated)
{
    auto emitSignal = [this](const QString &propertyName) {
        int idx = metaObject()->indexOfProperty(propertyName.toUtf8());
        if (idx == -1)
            return;

        QMetaProperty prop = metaObject()->property(idx);
        if (prop.hasNotifySignal())
            metaObject()->activate(this, prop.notifySignalIndex(), nullptr);
    };


    if (interfaceName == m_dbusInterface->interface()) {
        for (auto it = changed.cbegin(); it != changed.cend(); ++it)
            emitSignal(it.key());
        for (auto it = invalidated.cbegin(); it != invalidated.cend(); ++it)
            emitSignal(*it);
    }
}


IpcWrapperSignalRelay::IpcWrapperSignalRelay(IpcWrapperObject *wrapperObject)
    : QObject(wrapperObject)
    , m_wrapperObject(wrapperObject)
{ }

void IpcWrapperSignalRelay::onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed, const QStringList &invalidated)
{
    m_wrapperObject->onPropertiesChanged(interfaceName, changed, invalidated);
}

QT_END_NAMESPACE_AM
