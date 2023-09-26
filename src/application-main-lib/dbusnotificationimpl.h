// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QtAppManSharedMain/notificationimpl.h>

QT_BEGIN_NAMESPACE_AM

class ApplicationMain;

class DBusNotificationImpl : public NotificationImpl
{
public:
    DBusNotificationImpl(Notification *notification, ApplicationMain *applicationMain);

protected:
    void componentComplete() override;
    uint show() override;
    void close() override;

private:
    ApplicationMain *m_applicationMain;
    friend class DBusApplicationInterfaceImpl;
};

QT_END_NAMESPACE_AM
