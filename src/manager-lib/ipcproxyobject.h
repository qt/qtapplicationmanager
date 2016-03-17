/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#pragma once

#include <QtGlobal>
#include <QVector>
#include <QMap>
#include <QPointer>
#if defined(QT_DBUS_LIB)
#  include <QDBusVirtualObject>
#endif

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

    QObject *object() const;
    QString serviceName() const;
    QString pathName() const;
    QString interfaceName() const;
    QStringList connectionNames() const;

    bool isValidForApplication(const Application *app) const;

#if defined(QT_DBUS_LIB)
    bool dbusRegister(QDBusConnection connection, const QString &debugPathPrefix = QString());
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
    QStringList m_connectionNames;
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
};
