// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef NOTIFICATIONIMPL_H
#define NOTIFICATIONIMPL_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Notification;

class NotificationImpl
{
public:
    static void setFactory(const std::function<NotificationImpl *(Notification *, const QString &)> &factory);

    static NotificationImpl *create(Notification *notification, const QString &applicationId);

    virtual ~NotificationImpl() = default;

    Notification *notification();

    virtual void componentComplete() = 0;
    virtual uint show() = 0;
    virtual void close() = 0;

protected:
    NotificationImpl(Notification *notification);

private:
    Notification *m_notification = nullptr;
    static std::function<NotificationImpl *(Notification *, const QString &)> s_factory;
    Q_DISABLE_COPY_MOVE(NotificationImpl)
};

QT_END_NAMESPACE_AM

#endif // NOTIFICATIONIMPL_H
