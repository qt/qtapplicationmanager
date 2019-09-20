/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QDBusInterface>

#include "logging.h"
#include "dbusapplicationinterfaceextension.h"
#include "dbusapplicationinterface.h"
#include "ipcwrapperobject.h"

QT_BEGIN_NAMESPACE_AM

class DBusApplicationInterfaceExtensionPrivate
{
public:
    DBusApplicationInterfaceExtensionPrivate(const QDBusConnection &connection)
        : m_connection(connection)
    { }

    QHash<QString, QPointer<IpcWrapperObject>> m_interfaces;
    QDBusConnection m_connection;
};

DBusApplicationInterfaceExtensionPrivate *DBusApplicationInterfaceExtension::d = nullptr;

void DBusApplicationInterfaceExtension::initialize(const QDBusConnection &connection)
{
    if (!d)
        d = new DBusApplicationInterfaceExtensionPrivate(connection);
}

DBusApplicationInterfaceExtension::DBusApplicationInterfaceExtension(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(d);

    if (DBusApplicationInterface::s_instance->m_applicationIf) {
        connect(DBusApplicationInterface::s_instance->m_applicationIf, SIGNAL(interfaceCreated(QString)),
                     this, SLOT(onInterfaceCreated(QString)));
    } else {
        qCritical("ERROR: ApplicationInterface not initialized!");
    }
}

DBusApplicationInterfaceExtension::~DBusApplicationInterfaceExtension()
{
    d->m_interfaces.remove(m_name);

}

QString DBusApplicationInterfaceExtension::name() const
{
    return m_name;
}

bool DBusApplicationInterfaceExtension::isReady() const
{
    return m_object;
}

QObject *DBusApplicationInterfaceExtension::object() const
{
    return m_object;
}

void DBusApplicationInterfaceExtension::classBegin()
{
}

void DBusApplicationInterfaceExtension::componentComplete()
{
    tryInit();
}

void DBusApplicationInterfaceExtension::tryInit()
{
    if (m_name.isEmpty())
        return;

    IpcWrapperObject *ext = nullptr;

    auto it = d->m_interfaces.constFind(m_name);

    if (it != d->m_interfaces.constEnd()) {
        ext = *it;
    } else {
        auto createPathFromName = [](const QString &name) -> QString {
            QString path;

            const QChar *c = name.unicode();
            for (int i = 0; i < name.length(); ++i) {
                ushort u = c[i].unicode();
                path += QChar(((u >= 'a' && u <= 'z')
                               || (u >= 'A' && u <= 'Z')
                               || (u >= '0' && u <= '9')
                               || (u == '_')) ? u : '_');
            }
            return qSL("/ExtensionInterfaces/") + path;
        };

        ext = new IpcWrapperObject(QString(), createPathFromName(m_name), m_name,
                                   d->m_connection, this);
        if (ext->lastDBusError().isValid() || !ext->isDBusValid()) {
            qCWarning(LogQmlIpc) << "Could not connect to ApplicationInterfaceExtension" << m_name
                                 << ":" << ext->lastDBusError().message();
            delete ext;
            return;
        }
        d->m_interfaces.insert(m_name, ext);
    }
    m_object = ext;
    m_complete = true;

    emit objectChanged();
    emit readyChanged();
}

void DBusApplicationInterfaceExtension::setName(const QString &name)
{
    if (!m_complete) {
        m_name = name;
        tryInit();
    } else {
        qWarning("Cannot change the name property of an ApplicationInterfaceExtension after creation.");
    }
}

void DBusApplicationInterfaceExtension::onInterfaceCreated(const QString &interfaceName)
{
    if (m_name == interfaceName)
        tryInit();
}

QT_END_NAMESPACE_AM
