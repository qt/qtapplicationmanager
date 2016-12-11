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

#include <unistd.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QPointer>
#include <QCoreApplication>

#include "global.h"
#include "qmlapplicationinterface.h"
#include "notification.h"
#include "ipcwrapperobject.h"

QT_BEGIN_NAMESPACE_AM

QmlApplicationInterface *QmlApplicationInterface::s_instance = 0;

QmlApplicationInterface::QmlApplicationInterface(const QVariantMap &additionalConfiguration, const QString &dbusConnectionName, QObject *parent)
    : ApplicationInterface(parent)
    , m_connection(dbusConnectionName)
    , m_additionalConfiguration(additionalConfiguration)
{
    if (QmlApplicationInterface::s_instance)
        qCritical("ERROR: only one instance of QmlApplicationInterface is allowed");
    s_instance = this;
}

bool QmlApplicationInterface::initialize()
{
    auto tryConnect = [](const QString &service, const QString &path, const QString &interfaceName,
                         const QDBusConnection &conn, QObject *parent) -> QDBusInterface * {
        for (int i = 0; i < 100; ++i) {
            QDBusInterface *iface = new QDBusInterface(service, path, interfaceName, conn, parent);
            if (!iface->lastError().isValid())
                return iface;
            delete iface;
            usleep(1000);
        }
        return nullptr;
    };

    m_runtimeIf = tryConnect(qSL(""), qSL("/RuntimeInterface"), qSL("io.qt.ApplicationManager.RuntimeInterface"),
                                     m_connection, this);
    m_applicationIf = tryConnect(qSL(""), qSL("/ApplicationInterface"), qSL("io.qt.ApplicationManager.ApplicationInterface"),
                                         m_connection, this);
    m_notifyIf = tryConnect(qSL("org.freedesktop.Notifications"), qSL("/org/freedesktop/Notifications"), qSL("org.freedesktop.Notifications"),
                                    QDBusConnection::sessionBus(), this);


    if (!m_applicationIf) {
        qCritical("ERROR: could not connect to the ApplicationInterface on the P2P D-Bus");
        return false;
    }
    if (!m_runtimeIf) {
        qCritical("ERROR: could not connect to the RuntimeInterface on the P2P D-Bus");
        return false;
    }
    if (!m_applicationIf->isValid()) {
        qCritical("ERROR: ApplicationInterface on the P2P D-Bus is not valid: %s",
                  qPrintable(m_applicationIf->lastError().message()));
        return false;
    }
    if (!m_runtimeIf->isValid()) {
        qCritical("ERROR: RuntimeInterface on the P2P D-Bus is not valid: %s",
                  qPrintable(m_runtimeIf->lastError().message()));
        return false;
    }

    bool ok = true;
    ok = ok && connect(m_runtimeIf, SIGNAL(startApplication(QString,QString,QString,QVariantMap)), this, SIGNAL(startApplication(QString,QString,QString,QVariantMap)));

    if (!ok)
        qCritical("ERROR: could not connect the RuntimeInterface via D-Bus: %s", qPrintable(m_runtimeIf->lastError().name()));

    ok = ok && connect(m_applicationIf, SIGNAL(quit()), this, SIGNAL(quit()));
    ok = ok && connect(m_applicationIf, SIGNAL(memoryLowWarning()), this, SIGNAL(memoryLowWarning()));
    ok = ok && connect(m_applicationIf, SIGNAL(memoryCriticalWarning()), this, SIGNAL(memoryCriticalWarning()));
    ok = ok && connect(m_applicationIf, SIGNAL(openDocument(QString)), this, SIGNAL(openDocument(QString)));

    if (!ok)
        qCritical("ERROR: could not connect the ApplicationInterface via D-Bus: %s", qPrintable(m_applicationIf->lastError().name()));

    if (m_notifyIf) {
        ok = ok && connect(m_notifyIf, SIGNAL(NotificationClosed(uint,uint)), this, SLOT(notificationClosed(uint,uint)));
        ok = ok && connect(m_notifyIf, SIGNAL(ActionInvoked(uint,QString)), this, SLOT(notificationActionTriggered(uint,QString)));

        if (!ok)
            qCritical("ERROR: could not connect the org.freedesktop.Notifications interface via D-Bus: %s", qPrintable(m_notifyIf->lastError().name()));
    } else {
        qCritical("ERROR: could not create the org.freedesktop.Notifications interface on D-Bus");
    }

    QmlApplicationInterfaceExtension::initialize(m_connection);

    if (ok)
        m_runtimeIf->asyncCall(qSL("finishedInitialization"));
    return ok;
}

QString QmlApplicationInterface::applicationId() const
{
    if (m_appId.isEmpty() && m_applicationIf->isValid())
        m_appId = m_applicationIf->property("applicationId").toString();
    return m_appId;
}

Notification *QmlApplicationInterface::createNotification()
{
    QmlNotification *n = new QmlNotification(this, Notification::Dynamic);
    return n;
}

void QmlApplicationInterface::acknowledgeQuit() const
{
    QCoreApplication::instance()->quit();
}

QVariantMap QmlApplicationInterface::additionalConfiguration() const
{
    return m_additionalConfiguration;
}

uint QmlApplicationInterface::notificationShow(QmlNotification *n)
{
    if (n && m_notifyIf && m_notifyIf->isValid()) {
        QDBusReply<uint> newId = m_notifyIf->call(qSL("Notify"), applicationId(), n->notificationId(),
                                                  n->icon().toString(), n->summary(), n->body(),
                                                  n->libnotifyActionList(), n->libnotifyHints(),
                                                  n->timeout());
        if (newId.isValid()) {
            m_allNotifications << n;
            return newId;
        }
    }
    return 0;
}


void QmlApplicationInterface::notificationClose(QmlNotification *n)
{
    if (n && m_notifyIf && m_notifyIf->isValid())
        m_notifyIf->asyncCall(qSL("CloseNotification"), n->notificationId());
}

void QmlApplicationInterface::notificationClosed(uint notificationId, uint reason)
{
    qDebug("Notification was closed signal: %u", notificationId);
    foreach (const QPointer<QmlNotification> &n, m_allNotifications) {
        if (n->notificationId() == notificationId) {
            n->libnotifyNotificationClosed(reason);
            m_allNotifications.removeAll(n);
            break;
        }
    }
}

void QmlApplicationInterface::notificationActionTriggered(uint notificationId, const QString &actionId)
{
    qDebug("Notification action triggered signal: %u %s", notificationId, qPrintable(actionId));
    foreach (const QPointer<QmlNotification> &n, m_allNotifications) {
        if (n->notificationId() == notificationId) {
            n->libnotifyActionInvoked(actionId);
            break;
        }
    }
}


QmlNotification::QmlNotification(QObject *parent, ConstructionMode mode)
    : Notification(parent, mode)
{ }

void QmlNotification::libnotifyClose()
{
    if (QmlApplicationInterface::s_instance)
        QmlApplicationInterface::s_instance->notificationClose(this);
}

uint QmlNotification::libnotifyShow()
{
    if (QmlApplicationInterface::s_instance)
        return QmlApplicationInterface::s_instance->notificationShow(this);
    return 0;
}


class QmlApplicationInterfaceExtensionPrivate
{
public:
    QmlApplicationInterfaceExtensionPrivate(const QDBusConnection &connection)
        : m_connection(connection)
    { }

    QHash<QString, QPointer<IpcWrapperObject>> m_interfaces;
    QDBusConnection m_connection;
};

QmlApplicationInterfaceExtensionPrivate *QmlApplicationInterfaceExtension::d = nullptr;

void QmlApplicationInterfaceExtension::initialize(const QDBusConnection &connection)
{
    if (!d)
        d = new QmlApplicationInterfaceExtensionPrivate(connection);
}

QmlApplicationInterfaceExtension::QmlApplicationInterfaceExtension(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(d);

    if (QmlApplicationInterface::s_instance->m_applicationIf) {
        connect(QmlApplicationInterface::s_instance->m_applicationIf, SIGNAL(interfaceCreated(QString)),
                     this, SLOT(onInterfaceCreated(QString)));
    } else {
        qCritical("ERROR: ApplicationInterface not initialized!");
    }
}

QString QmlApplicationInterfaceExtension::name() const
{
    return m_name;
}

bool QmlApplicationInterfaceExtension::isReady() const
{
    return m_object;
}

QObject *QmlApplicationInterfaceExtension::object() const
{
    return m_object;
}

void QmlApplicationInterfaceExtension::classBegin()
{
}

void QmlApplicationInterfaceExtension::componentComplete()
{
    tryInit();
}

void QmlApplicationInterfaceExtension::tryInit()
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
                path += QLatin1Char(((u >= 'a' && u <= 'z')
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

void QmlApplicationInterfaceExtension::setName(const QString &name)
{
    if (!m_complete) {
        m_name = name;
        tryInit();
    } else {
        qWarning("Cannot change the name property of an ApplicationInterfaceExtension after creation.");
    }
}

void QmlApplicationInterfaceExtension::onInterfaceCreated(const QString &interfaceName)
{
    if (m_name == interfaceName)
        tryInit();
}

QT_END_NAMESPACE_AM
