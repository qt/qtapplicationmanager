/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QQmlEngine>
#include <qqml.h>

#include "launchermain.h"
#include "dbus-utilities.h"
#include "intentclient.h"
#include "intentclientrequest.h"
#include "intentclientdbusimplementation.h"
#include "intenthandler.h"

#include "intentinterface_interface.h"

QT_BEGIN_NAMESPACE_AM

IntentClientDBusImplementation::IntentClientDBusImplementation(const QString &dbusName)
    : IntentClientSystemInterface()
    , m_dbusName(dbusName)
{ }

QString IntentClientDBusImplementation::currentApplicationId(QObject *hint)
{
    Q_UNUSED(hint)
    return LauncherMain::instance()->applicationId();
}

void IntentClientDBusImplementation::initialize(IntentClient *intentClient) Q_DECL_NOEXCEPT_EXPR(false)
{
    IntentClientSystemInterface::initialize(intentClient);

    qmlRegisterSingletonType<IntentClient>("QtApplicationManager", 2, 0, "IntentClient",
                                           [](QQmlEngine *, QJSEngine *) -> QObject * {
        QQmlEngine::setObjectOwnership(IntentClient::instance(), QQmlEngine::CppOwnership);
        return IntentClient::instance();
    });

    qmlRegisterUncreatableType<IntentClientRequest>("QtApplicationManager", 2, 0, "IntentRequest",
                                                    qSL("Cannot create objects of type IntentRequest"));
    qmlRegisterType<IntentHandler>("QtApplicationManager.Application", 2, 0, "IntentHandler");


    m_dbusInterface = new IoQtApplicationManagerIntentInterfaceInterface(
                QString(), qSL("/IntentServer"), QDBusConnection(m_dbusName), intentClient);

    if (!m_dbusInterface->isValid())
        throw std::logic_error("Could not connect to the /IntentServer object on the P2P D-Bus");

    connect(m_dbusInterface, &IoQtApplicationManagerIntentInterfaceInterface::replyFromSystem,
            intentClient, [this](const QString &requestId, bool error, const QVariantMap &result) {
        emit replyFromSystem(QUuid::fromString(requestId), error,
                             convertFromDBusVariant(result).toMap());
    });

    connect(m_dbusInterface, &IoQtApplicationManagerIntentInterfaceInterface::requestToApplication,
            intentClient, [this](const QString &requestId, const QString &id,
            const QString &applicationId, const QVariantMap &parameters) {
        emit requestToApplication(QUuid::fromString(requestId), id, QString(), applicationId,
                                  convertFromDBusVariant(parameters).toMap());
    });
}

void IntentClientDBusImplementation::requestToSystem(QPointer<IntentClientRequest> icr)
{
    QDBusPendingReply<QString> reply =
            m_dbusInterface->requestToSystem(icr->intentId(), icr->applicationId(),
                                             convertFromJSVariant(icr->parameters()).toMap());

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, icr);
    connect(watcher, &QDBusPendingCallWatcher::finished, icr, [this, watcher, icr]() {
        watcher->deleteLater();

        QDBusPendingReply<QString> reply = *watcher;
        emit requestToSystemFinished(icr, QUuid::fromString(reply.argumentAt<0>()),
                                     reply.isError(), reply.error().message());
    });
}

void IntentClientDBusImplementation::replyFromApplication(QPointer<IntentClientRequest> icr)
{
    if (icr) {
        m_dbusInterface->replyFromApplication(icr->requestId().toString(), !icr->succeeded(),
                                              convertFromJSVariant(icr->result()).toMap());
        //TODO: should we wait for the call completion - how/why would we report a possible failure?
    }
}

QT_END_NAMESPACE_AM
