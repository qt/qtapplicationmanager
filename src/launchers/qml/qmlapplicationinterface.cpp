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

#include <unistd.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QPointer>

#include "global.h"
#include "qmlapplicationinterface.h"
#include "notification.h"
#include "ipcwrapperobject.h"

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
        for (int i = 0; i < 10; ++i) {
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


    if (!m_applicationIf->isValid()) {
        qCritical("ERROR: ApplicationInterface on D-Bus is not valid: %s",
                  qPrintable(m_applicationIf->lastError().message()));
        return false;
    }
    if (!m_runtimeIf->isValid()) {
        qCritical("ERROR: RuntimeInterface on D-Bus is not valid: %s",
                  qPrintable(m_runtimeIf->lastError().message()));
        return false;
    }

    bool ok = true;
    ok = ok && connect(m_runtimeIf, SIGNAL(startApplication(QString,QString,QVariantMap)), this, SIGNAL(startApplication(QString,QString,QVariantMap)));

    if (!ok)
        qCritical("ERROR: could not connect the RuntimeInterface via D-Bus: %s", qPrintable(m_runtimeIf->lastError().name()));

    ok = ok && connect(m_applicationIf, SIGNAL(quit()), this, SIGNAL(quit()));
    ok = ok && connect(m_applicationIf, SIGNAL(memoryLowWarning()), this, SIGNAL(memoryLowWarning()));
    ok = ok && connect(m_applicationIf, SIGNAL(openDocument(QString)), this, SIGNAL(openDocument(QString)));

    if (!ok)
        qCritical("ERROR: could not connect the ApplicationInterface via D-Bus: %s", qPrintable(m_applicationIf->lastError().name()));

    ok = ok && connect(m_notifyIf, SIGNAL(NotificationClosed(uint,uint)), this, SLOT(notificationClosed(uint,uint)));
    ok = ok && connect(m_notifyIf, SIGNAL(ActionInvoked(uint,QString)), this, SLOT(notificationActivated(uint,QString)));

    if (!ok)
        qCritical("ERROR: could not connect the org.freedesktop.Notifications interface via D-Bus: %s", qPrintable(m_notifyIf->lastError().name()));

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

QVariantMap QmlApplicationInterface::additionalConfiguration() const
{
    return m_additionalConfiguration;
}

uint QmlApplicationInterface::notificationShow(QmlNotification *n)
{
    if (n && m_notifyIf->isValid()) {
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
    if (n && m_notifyIf->isValid())
        m_notifyIf->asyncCall(qSL("CloseNotification"), n->notificationId());
}

void QmlApplicationInterface::notificationClosed(uint notificationId, uint reason)
{
    qDebug("Notification was closed signal: %u", notificationId);
    foreach (const QPointer<QmlNotification> &n, m_allNotifications) {
        if (n->notificationId() == notificationId) {
            n->libnotifyClosed(reason);
            m_allNotifications.removeAll(n);
            break;
        }
    }
}

void QmlApplicationInterface::notificationActivated(uint notificationId, const QString &actionId)
{
    qDebug("Notification action triggered signal: %u %s", notificationId, qPrintable(actionId));
    foreach (const QPointer<QmlNotification> &n, m_allNotifications) {
        if (n->notificationId() == notificationId) {
            n->libnotifyActivated(actionId);
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
    m_complete = true;

    if (m_name.isEmpty()) {
        qCWarning(LogQmlIpc) << "ApplicationInterfaceExension.name is not set.";
        return;
    }

    IpcWrapperObject *ext = nullptr;

    auto it = d->m_interfaces.constFind(m_name);

    if (it != d->m_interfaces.constEnd()) {
        ext = *it;
    } else {
        ext = new IpcWrapperObject(QString(), qSL("/ExtensionInterfaces"), m_name,
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
    emit objectChanged();
    emit readyChanged();
}

void QmlApplicationInterfaceExtension::setName(const QString &name)
{
    if (!m_complete)
        m_name = name;
    else
        qWarning("Cannot change the name property of an ApplicationInterfaceExtension after creation.");
}

