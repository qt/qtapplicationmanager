/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#include <qplatformdefs.h>
#if defined(Q_OS_UNIX)
#  include <unistd.h>
#endif

#include "notificationmanagerdbuscontextadaptor.h"
#include "notificationmanager.h"
#include "notifications_adaptor.h"

QT_BEGIN_NAMESPACE_AM

NotificationManagerDBusContextAdaptor::NotificationManagerDBusContextAdaptor(NotificationManager *nm)
    : AbstractDBusContextAdaptor(nm)
{
    m_adaptor = new NotificationsAdaptor(this);
}

QT_END_NAMESPACE_AM

/////////////////////////////////////////////////////////////////////////////////////

QT_USE_NAMESPACE_AM

NotificationsAdaptor::NotificationsAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    auto nm = NotificationManager::instance();

    connect(nm, &NotificationManager::ActionInvoked,
            this, &NotificationsAdaptor::ActionInvoked);
    connect(nm, &NotificationManager::NotificationClosed,
            this, &NotificationsAdaptor::NotificationClosed);
}

NotificationsAdaptor::~NotificationsAdaptor()
{ }

void NotificationsAdaptor::CloseNotification(uint id)
{
    NotificationManager::instance()->CloseNotification(id);
}

QStringList NotificationsAdaptor::GetCapabilities()
{
    return NotificationManager::instance()->GetCapabilities();
}

QString NotificationsAdaptor::GetServerInformation(QString &vendor, QString &version, QString &spec_version)
{
    return NotificationManager::instance()->GetServerInformation(vendor, version, spec_version);
}

uint NotificationsAdaptor::Notify(const QString &app_name, uint replaces_id, const QString &app_icon, const QString &summary, const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout)
{
    return NotificationManager::instance()->Notify(app_name, replaces_id, app_icon, summary, body, actions, hints, timeout);
}

