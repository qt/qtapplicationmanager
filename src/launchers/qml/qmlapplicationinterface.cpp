/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
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

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QPointer>

#include "qmlapplicationinterface.h"
#include "notification.h"


QmlApplicationInterface *QmlApplicationInterface::s_instance = 0;

QmlApplicationInterface::QmlApplicationInterface(const QString &dbusConnectionName, QObject *parent)
    : ApplicationInterface(parent)
{
    if (QmlApplicationInterface::s_instance)
        qCritical("ERROR: only one instance of QmlApplicationInterface is allowed");
    s_instance = this;

    m_applicationIf = new QDBusInterface("", "/ApplicationInterface", "io.qt.ApplicationManager.ApplicationInterface",
                                         QDBusConnection(dbusConnectionName), this);
    m_runtimeIf = new QDBusInterface("", "/RuntimeInterface", "io.qt.ApplicationManager.RuntimeInterface",
                                     QDBusConnection(dbusConnectionName), this);
    m_notifyIf = new QDBusInterface("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications",
                                    QDBusConnection::sessionBus(), this);
}

bool QmlApplicationInterface::initialize()
{
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

    if (ok)
        m_runtimeIf->asyncCall("finishedInitialization");
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

uint QmlApplicationInterface::notificationShow(QmlNotification *n)
{
    if (n && m_notifyIf->isValid()) {
        QDBusReply<uint> newId = m_notifyIf->call("Notify", applicationId(), n->notificationId(),
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
        m_notifyIf->asyncCall("CloseNotification", n->notificationId());
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
