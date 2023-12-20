// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QQmlEngine>
#include <QQmlExpression>
#include <QQmlInfo>

#include "logging.h"
#include "qmlinprocnotificationimpl.h"
#include "notification.h"
#include "notificationmanager.h"

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM

QVector<QPointer<Notification>> QmlInProcNotificationImpl::s_allNotifications;


QmlInProcNotificationImpl::QmlInProcNotificationImpl(Notification *notification,
                                                     const QString &applicationId)
    : NotificationImpl(notification)
    , m_applicationId(applicationId)
{
    initialize();
}

void QmlInProcNotificationImpl::componentComplete()
{
    QQmlContext *ctxt = QQmlEngine::contextForObject(notification());
    if (ctxt) {
        QQmlExpression expr(ctxt, nullptr, u"ApplicationInterface.applicationId"_s, nullptr);
        QVariant v = expr.evaluate();
        if (!v.isNull())
            m_applicationId = v.toString();
    }
}

void QmlInProcNotificationImpl::initialize()
{
    static bool once = false;
    if (once)
        return;
    once = true;

    auto nm = NotificationManager::instance();

    QObject::connect(nm, &NotificationManager::ActionInvoked,
                     nm, [](uint notificationId, const QString &actionId) {
        qDebug("Notification action triggered signal: %u %s", notificationId, qPrintable(actionId));
        for (const QPointer<Notification> &n : std::as_const(s_allNotifications)) {
            if (n->notificationId() == notificationId) {
                n->triggerAction(actionId);
                break;
            }
        }
    });

    QObject::connect(nm, &NotificationManager::NotificationClosed,
                     nm, [](uint notificationId, uint reason) {
        Q_UNUSED(reason);

        qDebug("Notification was closed signal: %u", notificationId);
        // quick fix: in case apps have been closed items are null (see AUTOSUITE-14)
        s_allNotifications.removeAll(nullptr);
        for (const QPointer<Notification> &n : std::as_const(s_allNotifications)) {
            if (n->notificationId() == notificationId) {
                n->close();
                s_allNotifications.removeAll(n);
                break;
            }
        }
    });
}

void QmlInProcNotificationImpl::close()
{
    NotificationManager::instance()->CloseNotification(notification()->notificationId());
}

uint QmlInProcNotificationImpl::show()
{
    auto *n = notification();

    s_allNotifications << n;
    return NotificationManager::instance()->Notify(m_applicationId, n->notificationId(),
                                                   n->icon().toString(), n->summary(), n->body(),
                                                   n->libnotifyActionList(), n->libnotifyHints(),
                                                   n->timeout());
}

QT_END_NAMESPACE_AM
