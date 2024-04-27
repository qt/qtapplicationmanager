// Copyright (C) 2024 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef DBUSAPPLICATIONINTERFACEIMPL_H
#define DBUSAPPLICATIONINTERFACEIMPL_H

#include <QtCore/QPointer>
#include <QtAppManCommon/global.h>
#include <QtAppManSharedMain/applicationinterfaceimpl.h>


QT_BEGIN_NAMESPACE_AM

class ApplicationMain;


class DBusApplicationInterfaceImpl : public ApplicationInterfaceImpl
{
public:
    explicit DBusApplicationInterfaceImpl(ApplicationMain *applicationMain);

    void attach(ApplicationInterface *iface) override;

    QString applicationId() const override;
    QVariantMap name() const override;
    QUrl icon() const override;
    QString version() const override;
    QVariantMap systemProperties() const override;
    QVariantMap applicationProperties() const override;
    void acknowledgeQuit() override;

private:
    QPointer<ApplicationMain> m_applicationMain;

    friend class ApplicationMain;
    friend class DBusNotification;
    friend class Controller; // for instantiating the singleton
};

QT_END_NAMESPACE_AM

#endif // DBUSAPPLICATIONINTERFACEIMPL_H
