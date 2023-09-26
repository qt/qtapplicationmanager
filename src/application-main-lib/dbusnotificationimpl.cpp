// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "dbusnotificationimpl.h"
#include "applicationmain.h"


QT_BEGIN_NAMESPACE_AM

DBusNotificationImpl::DBusNotificationImpl(Notification *notification,
                                           ApplicationMain *applicationMain)
    : NotificationImpl(notification)
    , m_applicationMain(applicationMain)
{ }

void DBusNotificationImpl::componentComplete()
{ }

void DBusNotificationImpl::close()
{
    m_applicationMain->closeNotification(notification());
}

uint DBusNotificationImpl::show()
{
    return m_applicationMain->showNotification(notification());
}

QT_END_NAMESPACE_AM
