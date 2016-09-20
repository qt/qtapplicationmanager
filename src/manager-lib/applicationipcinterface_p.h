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

#pragma once

#include <QtGlobal>
#include <QVector>
#include <QMap>
#include <QPointer>
#if defined(QT_DBUS_LIB)
#  include <QDBusVirtualObject>
#endif
#include "global.h"

AM_BEGIN_NAMESPACE

class Application;
class IpcProxySignalRelay;

class IpcProxyObject
#if defined(QT_DBUS_LIB)
        : protected QDBusVirtualObject
#else
        : protected QObject
#endif
{
public:
    IpcProxyObject(QObject *object, const QString &serviceName, const QString &pathName,
                   const QString &interfaceName, const QVariantMap &filter);
    ~IpcProxyObject();

    QObject *object() const;
    QString serviceName() const;
    QString pathName() const;
    QString interfaceName() const;
    QStringList connectionNames() const;

    bool isValidForApplication(const Application *app) const;

#if defined(QT_DBUS_LIB)
    bool dbusRegister(const Application *app, QDBusConnection connection, const QString &debugPathPrefix = QString());
    bool dbusUnregister(QDBusConnection connection);

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;
#endif

private:
    void relaySignal(int signalIndex, void **argv);
    QByteArray createIntrospectionXml();

    friend class IpcProxySignalRelay;

private:
    QPointer<QObject> m_object;
    IpcProxySignalRelay *m_signalRelay;
    QMap<QString, QString> m_connectionNamesToApplicationIds;
    QString m_serviceName;
    QString m_pathName;
    QMap<QString, QString> m_pathNamePrefixForConnection; // debugging only
    QString m_interfaceName;

    QString m_xmlIntrospection;
    QStringList m_appIdFilter;
    QStringList m_categoryFilter;
    QStringList m_capabilityFilter;

    QVector<int> m_properties;
    QVector<int> m_signals;
    QVector<int> m_slots;
    QMap<int, QList<int>> m_slotSignatures;
    QMap<int, int> m_signalsToProperties;

    QString m_sender;
    QStringList m_receivers;
    static QVector<IpcProxyObject *> s_proxies;

    friend class ApplicationIPCInterfaceAttached;
};

class IpcProxySignalRelay : public QObject
{
public:
    IpcProxySignalRelay(IpcProxyObject *proxyObject);

    int qt_metacall(QMetaObject::Call c, int id, void **argv) override;

private:
    IpcProxyObject *m_proxyObject;
};

class IpcProxyObject;

class ApplicationIPCInterfaceAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString sender READ sender)
    Q_PROPERTY(QVariant receivers READ receivers WRITE setReceivers)
    Q_PROPERTY(QVariant inProcessReceiversOnly READ inProcessReceiversOnly CONSTANT)

public:
    ApplicationIPCInterfaceAttached(QObject *object);

    QString sender() const;
    QVariant receivers() const;
    void setReceivers(const QVariant &receivers);
    QVariant inProcessReceiversOnly() const;

private:
    bool resolveProxy() const;

    QObject *m_object;
    mutable IpcProxyObject *m_proxy = nullptr;
};

AM_END_NAMESPACE
