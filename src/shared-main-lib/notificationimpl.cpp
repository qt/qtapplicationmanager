// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "global.h"
#include "notification.h"
#include "notificationimpl.h"


QT_BEGIN_NAMESPACE_AM

std::function<NotificationImpl *(Notification *, const QString &)> NotificationImpl::s_factory;

NotificationImpl::NotificationImpl(Notification *notification)
    : m_notification(notification)
{ }

void NotificationImpl::setFactory(const std::function<NotificationImpl *(Notification *, const QString &)> &factory)
{
    s_factory = factory;
}

NotificationImpl *NotificationImpl::create(Notification *notification, const QString &applicationId)
{
    return s_factory ? s_factory(notification, applicationId) : nullptr;
}

Notification *NotificationImpl::notification()
{
    return m_notification;
}

void NotificationImpl::classBegin()
{ }

void NotificationImpl::componentComplete()
{ }

QT_END_NAMESPACE_AM
