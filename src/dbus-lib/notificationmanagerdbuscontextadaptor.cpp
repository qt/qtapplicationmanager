// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDBusConnectionInterface>

#include <qplatformdefs.h>
#if defined(Q_OS_UNIX)
#  include <unistd.h>
#endif

#include "logging.h"
#include "applicationmanager.h"
#include "abstractruntime.h"
#include "notificationmanagerdbuscontextadaptor.h"
#include "notificationmanager.h"
#include "notifications_adaptor.h"

#ifdef interface // in case windows.h got included somehow
#  undef interface
#endif

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
    if (auto *app = ApplicationManager::instance()->application(app_name)) {
        // this request is coming from an application we know about, make sure it really is that app
        bool pidMatches = false;

        if (qint64 appPid = app->currentRuntime() ? app->currentRuntime()->applicationProcessId() : 0) {
            auto ctx = AbstractDBusContextAdaptor::dbusContextFor(this);
            auto reply = ctx->connection().interface()->servicePid(ctx->message().service());
            if (reply.isValid() && (reply.value() == appPid))
                pidMatches = true;
        }
        if (!pidMatches) {
            qCCritical(LogNotifications) << "Ignoring notification from application"
                                         << app_name << "as it did not actually originate from that application.";
            return 0;
        }
    } else {
        // this request is coming from the outside and app_name could be anything
        //TODO: 6.11: prefix with ":ext:"
    }

    return NotificationManager::instance()->Notify(app_name, replaces_id, app_icon, summary, body, actions, hints, timeout);
}

