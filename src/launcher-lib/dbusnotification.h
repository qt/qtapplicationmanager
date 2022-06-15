// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QtAppManNotification/notification.h>

QT_BEGIN_NAMESPACE_AM

class DBusApplicationInterface;

class DBusNotification : public Notification // clazy:exclude=missing-qobject-macro
{
public:
    DBusNotification(QObject *parent = nullptr, Notification::ConstructionMode mode = Notification::Declarative);

    static DBusNotification *create(QObject *parent = nullptr);

protected:
    uint libnotifyShow() override;
    void libnotifyClose() override;

    friend class DBusApplicationInterface;
};

QT_END_NAMESPACE_AM
