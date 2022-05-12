/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

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
