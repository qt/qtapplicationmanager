// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DBUSNOTIFICATIONIMPL_H
#define DBUSNOTIFICATIONIMPL_H

#include <QtAppManCommon/global.h>
#include <QtAppManSharedMain/notificationimpl.h>

QT_BEGIN_NAMESPACE_AM

class ApplicationMain;

class DBusNotificationImpl : public NotificationImpl
{
public:
    DBusNotificationImpl(Notification *notification, ApplicationMain *applicationMain);

protected:
    uint show() override;
    void close() override;

private:
    ApplicationMain *m_applicationMain;
};

QT_END_NAMESPACE_AM

#endif // DBUSNOTIFICATIONIMPL_H
