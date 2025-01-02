// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "dbusnotification.h"
#include "dbusapplicationinterface.h"

QT_BEGIN_NAMESPACE_AM

DBusNotification::DBusNotification(QObject *parent, ConstructionMode mode)
    : Notification(parent, mode)
{ }

DBusNotification *DBusNotification::create(QObject *parent)
{
    return new DBusNotification(parent, QtAM::Notification::Dynamic);
}

void DBusNotification::libnotifyClose()
{
    if (DBusApplicationInterface::s_instance)
        DBusApplicationInterface::s_instance->notificationClose(this);
}

uint DBusNotification::libnotifyShow()
{
    if (DBusApplicationInterface::s_instance)
        return DBusApplicationInterface::s_instance->notificationShow(this);
    return 0;
}

QT_END_NAMESPACE_AM
