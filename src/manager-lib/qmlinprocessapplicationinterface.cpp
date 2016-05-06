/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QQmlEngine>
#include <QQmlExpression>

#include "qmlinprocessapplicationinterface.h"
#include "qmlinprocessruntime.h"
#include "application.h"
#include "applicationmanager.h"
#include "notificationmanager.h"
#include "applicationipcmanager.h"
#include "applicationipcinterface.h"


QmlInProcessApplicationInterface::QmlInProcessApplicationInterface(QmlInProcessRuntime *runtime)
    : ApplicationInterface(runtime)
    , m_runtime(runtime)
{
    connect(ApplicationManager::instance(), &ApplicationManager::memoryLowWarning,
            this, &ApplicationInterface::memoryLowWarning);
    connect(runtime, &QmlInProcessRuntime::aboutToStop,
            this, &ApplicationInterface::quit);
}

QString QmlInProcessApplicationInterface::applicationId() const
{
    if (m_runtime && m_runtime->application())
        return m_runtime->application()->id();
    return QString();
}

QVariantMap QmlInProcessApplicationInterface::additionalConfiguration() const
{
    if (m_runtime)
        return m_runtime->additionalConfiguration();
    return QVariantMap();
}

Notification *QmlInProcessApplicationInterface::createNotification()
{
    QmlInProcessNotification *n = new QmlInProcessNotification(this, Notification::Dynamic);
    n->m_appId = applicationId();
    return n;
}


//void QmlInProcessApplicationInterface::notificationClosed(uint notificationId, uint reason)
//{
//    qDebug("Notification was closed signal: %u", notificationId);
//    foreach (const QPointer<QmlInProcessNotification> &n, m_allNotifications) {
//        if (n->notificationId() == notificationId) {
//            n->libnotifyClosed(reason);
//            m_allNotifications.removeAll(n);
//            break;
//        }
//    }
//}

//void QmlInProcessApplicationInterface::notificationActivated(uint notificationId, const QString &actionId)
//{
//    qDebug("Notification action triggered signal: %u %s", notificationId, qPrintable(actionId));
//    foreach (const QPointer<QmlInProcessNotification> &n, m_allNotifications) {
//        if (n->notificationId() == notificationId) {
//            n->libnotifyActivated(actionId);
//            break;
//        }
//    }
//}


QmlInProcessNotification::QmlInProcessNotification(QObject *parent, ConstructionMode mode)
    : Notification(parent, mode)
    , m_mode(mode)
{
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

void QmlInProcessNotification::libnotifyClose()
{
    NotificationManager::instance()->CloseNotification(notificationId());
}

uint QmlInProcessNotification::libnotifyShow()
{
    return NotificationManager::instance()->Notify(m_appId, notificationId(),
                                                   icon().toString(), summary(), body(),
                                                   libnotifyActionList(), libnotifyHints(),
                                                   timeout());
}



QmlInProcessApplicationInterfaceExtension::QmlInProcessApplicationInterfaceExtension(QObject *parent)
    : QObject(parent)
{ }

QString QmlInProcessApplicationInterfaceExtension::name() const
{
    return m_name;
}

bool QmlInProcessApplicationInterfaceExtension::isReady() const
{
    return m_object;
}

QObject *QmlInProcessApplicationInterfaceExtension::object() const
{
    return m_object;
}

void QmlInProcessApplicationInterfaceExtension::classBegin()
{ }

void QmlInProcessApplicationInterfaceExtension::componentComplete()
{
    m_complete = true;

    if (m_name.isEmpty()) {
        qCWarning(LogQmlIpc) << "ApplicationInterfaceExension.name is not set.";
        return;
    }

    // find out our application id
    const Application *app = nullptr;

    if (QQmlContext *ctxt = QQmlEngine::contextForObject(this)) {
        QQmlExpression expr(ctxt, nullptr, qSL("ApplicationInterface.applicationId"), nullptr);
        QVariant v = expr.evaluate();
        if (!v.isNull())
            app = ApplicationManager::instance()->fromId(v.toString());
    }

    if (app) {
        for (ApplicationIPCInterface *iface : ApplicationIPCManager::instance()->interfaces()) {
            if ((iface->interfaceName() == m_name) && iface->isValidForApplication(app)) {
                m_object = iface;
                emit objectChanged();
                emit readyChanged();
                break;
            }
        }
    }
}

void QmlInProcessApplicationInterfaceExtension::setName(const QString &name)
{
    if (!m_complete)
        m_name = name;
    else
        qWarning("Cannot change the name property of an ApplicationInterfaceExtension after creation.");
}

