// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DBUSAPPLICATIONINTERFACEIMPL_H
#define DBUSAPPLICATIONINTERFACEIMPL_H

#include <QtAppManCommon/global.h>
#include <QtAppManSharedMain/applicationinterfaceimpl.h>
#include <QtAppManSharedMain/notification.h>


QT_BEGIN_NAMESPACE_AM

class ApplicationMain;


class DBusApplicationInterfaceImpl : public ApplicationInterfaceImpl
{
public:
    explicit DBusApplicationInterfaceImpl(ApplicationInterface *ai,
                                          ApplicationMain *applicationMain);

    QString applicationId() const override;
    QVariantMap name() const override;
    QUrl icon() const override;
    QString version() const override;
    QVariantMap systemProperties() const override;
    QVariantMap applicationProperties() const override;
    void acknowledgeQuit() override;

    ApplicationMain *m_applicationMain;

    static DBusApplicationInterfaceImpl *s_instance;

    friend class DBusNotification;
    friend class Controller;
};

QT_END_NAMESPACE_AM

#endif // DBUSAPPLICATIONINTERFACEIMPL_H
