/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QQmlEngine>
#include <QQmlExpression>
#include <QQmlInfo>

#include "logging.h"
#include "qmlinprocessapplicationinterface.h"
#include "qmlinprocessruntime.h"
#include "application.h"
#include "applicationmanager.h"
#include "notificationmanager.h"
#include "intentclientrequest.h"

QT_BEGIN_NAMESPACE_AM

QmlInProcessApplicationInterface::QmlInProcessApplicationInterface(QmlInProcessRuntime *runtime)
    : ApplicationInterface(runtime)
    , m_runtime(runtime)
{
    connect(ApplicationManager::instance(), &ApplicationManager::memoryLowWarning,
            this, &ApplicationInterface::memoryLowWarning);
    connect(runtime, &QmlInProcessRuntime::aboutToStop,
            this, &ApplicationInterface::quit);

    QmlInProcessNotification::initialize();
}

QString QmlInProcessApplicationInterface::applicationId() const
{
    if (m_runtime && m_runtime->application())
        return m_runtime->application()->id();
    return QString();
}

QVariantMap QmlInProcessApplicationInterface::name() const
{
    QVariantMap names;
    if (m_runtime && m_runtime->application()) {
        const QMap<QString, QString> &sm = m_runtime->application()->packageInfo()->names();
        for (auto it = sm.cbegin(); it != sm.cend(); ++it)
            names.insert(it.key(), it.value());
    }
    return names;
}

QUrl QmlInProcessApplicationInterface::icon() const
{
    if (m_runtime && m_runtime->application())
        return m_runtime->application()->packageInfo()->icon();
    return QUrl();
}

QString QmlInProcessApplicationInterface::version() const
{
    if (m_runtime && m_runtime->application())
        return m_runtime->application()->version();
    return QString();
}

QVariantMap QmlInProcessApplicationInterface::systemProperties() const
{
    if (m_runtime)
        return m_runtime->systemProperties();
    return QVariantMap();
}

QVariantMap QmlInProcessApplicationInterface::applicationProperties() const
{
    if (m_runtime && m_runtime->application()) {
        return m_runtime->application()->info()->allAppProperties();
    }
    return QVariantMap();
}

void QmlInProcessApplicationInterface::acknowledgeQuit()
{
    emit quitAcknowledged();
}

void QmlInProcessApplicationInterface::finishedInitialization()
{
}

Notification *QmlInProcessApplicationInterface::createNotification()
{
    QmlInProcessNotification *n = new QmlInProcessNotification(this, Notification::Dynamic);
    n->m_appId = applicationId();
    return n;
}


QVector<QPointer<QmlInProcessNotification> > QmlInProcessNotification::s_allNotifications;

QmlInProcessNotification::QmlInProcessNotification(QObject *parent, ConstructionMode mode)
    : Notification(parent, mode)
    , m_mode(mode)
{
    initialize();
}

void QmlInProcessNotification::componentComplete()
{
    Notification::componentComplete();

    if (m_mode == Declarative) {
        QQmlContext *ctxt = QQmlEngine::contextForObject(this);
        if (ctxt) {
            QQmlExpression expr(ctxt, nullptr, qSL("ApplicationInterface.applicationId"), nullptr);
            QVariant v = expr.evaluate();
            if (!v.isNull())
                m_appId = v.toString();
        }
    }
}

void QmlInProcessNotification::initialize()
{
    static bool once = false;

    if (once)
        return;
    once = true;

    auto nm = NotificationManager::instance();

    connect(nm, &NotificationManager::ActionInvoked,
            nm, [](uint notificationId, const QString &actionId) {
        qDebug("Notification action triggered signal: %u %s", notificationId, qPrintable(actionId));
        for (const QPointer<QmlInProcessNotification> &n : s_allNotifications) {
            if (n->notificationId() == notificationId) {
                n->libnotifyActionInvoked(actionId);
                break;
            }
        }
    });

    connect(nm, &NotificationManager::NotificationClosed,
            nm, [](uint notificationId, uint reason) {
        qDebug("Notification was closed signal: %u", notificationId);
        // quick fix: in case apps have been closed items are null (see AUTOSUITE-14)
        s_allNotifications.removeAll(nullptr);
        for (const QPointer<QmlInProcessNotification> &n : s_allNotifications) {
            if (n->notificationId() == notificationId) {
                n->libnotifyNotificationClosed(reason);
                s_allNotifications.removeAll(n);
                break;
            }
        }
    });
}

void QmlInProcessNotification::libnotifyClose()
{
    NotificationManager::instance()->CloseNotification(notificationId());
}

uint QmlInProcessNotification::libnotifyShow()
{
    s_allNotifications << this;
    return NotificationManager::instance()->Notify(m_appId, notificationId(),
                                                   icon().toString(), summary(), body(),
                                                   libnotifyActionList(), libnotifyHints(),
                                                   timeout());
}

QT_END_NAMESPACE_AM

#include "moc_qmlinprocessapplicationinterface.cpp"
