/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
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

QVariantMap QmlInProcessApplicationInterface::additionalConfiguration() const
{
    return systemProperties();
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
        return m_runtime->application()->allAppProperties();
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
        foreach (const QPointer<QmlInProcessNotification> &n, s_allNotifications) {
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
        foreach (const QPointer<QmlInProcessNotification> &n, s_allNotifications) {
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

/*!
    \qmltype ApplicationInterfaceExtension
    \inqmlmodule QtApplicationManager
    \brief Client side access to IPC interface extensions in the System-UI.

    This is the client side type used to access IPC interfaces, registered via the ApplicationIPCManager
    on the System-UI side.

    \qml
    ApplicationInterfaceExtension {
        id: testInterface
        name: "io.qt.test.interface"

        onReadyChanged: {
            if (ready)
                var v = object.testFunction(42, "string")
        }
    }
    \endqml
*/

/*!
    \qmlproperty string ApplicationInterfaceExtension::name

    The name of the interface, as it was registered by ApplicationIPCManager::registerInterface.
*/

/*!
    \qmlproperty bool ApplicationInterfaceExtension::ready
    \readonly

    This property will change to \c true, as soon as the connection to the remote interface has
    succeeded. In single-process setups, this will always be \c true, whereas in multi-process
    setups, it takes a few milli-seconds to setup the D-Bus connection first.
*/

/*!
    \qmlproperty QtObject ApplicationInterfaceExtension::object
    \readonly

    The actual IPC object, which has all the signals, slots and properties exported from the server
    side. Will be null, until ready becomes \c true.
*/

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

    connect(ApplicationIPCManager::instance(), &ApplicationIPCManager::interfaceCreated,
            this, &QmlInProcessApplicationInterfaceExtension::resolveObject);
}

void QmlInProcessApplicationInterfaceExtension::resolveObject()
{
    const auto ifaces = ApplicationIPCManager::instance()->interfaces();
    for (ApplicationIPCInterface *iface : ifaces) {
        if ((iface->interfaceName() == m_name)) {
            m_object = iface->serviceObject();
            emit objectChanged();
            emit readyChanged();
            break;
        }
    }
}

void QmlInProcessApplicationInterfaceExtension::setName(const QString &name)
{
    if (!m_complete) {
        m_name = name;
        resolveObject();
    } else {
        qWarning("Cannot change the name property of an ApplicationInterfaceExtension after creation.");
    }
}

QT_END_NAMESPACE_AM
