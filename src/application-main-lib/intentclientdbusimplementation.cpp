// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QQmlEngine>
#include <qqml.h>

#include "applicationmain.h"
#include "dbus-utilities.h"
#include "intentclient.h"
#include "intentclientrequest.h"
#include "intentclientdbusimplementation.h"
#include "intenthandler.h"

#include "intentinterface_interface.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

IntentClientDBusImplementation::IntentClientDBusImplementation(const QString &dbusName, QObject *parent)
    : IntentClientSystemInterface(parent)
    , m_dbusName(dbusName)
{ }

QString IntentClientDBusImplementation::currentApplicationId(QObject *hint)
{
    Q_UNUSED(hint)
    return ApplicationMain::instance()->applicationId();
}

void IntentClientDBusImplementation::initialize(IntentClient *intentClient) noexcept(false)
{
    IntentClientSystemInterface::initialize(intentClient);

    m_dbusInterface = new IoQtApplicationManagerIntentInterfaceInterface(
                QString(), u"/IntentServer"_s, QDBusConnection(m_dbusName), intentClient);

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
        // Broadcasts were introduced after the DBus API was finalized. Instead of adding a complete
        // "version 2" DBus interface, we simply append the string "@broadcast" to the requestId
        // for broadcasts.

        bool isBroadcast = requestId.endsWith(u"@broadcast"_s);
        QUuid requestIdUuid = QUuid::fromString(isBroadcast ? requestId.chopped(10) : requestId);
        QString requestingApplicationId = isBroadcast ? u":broadcast:"_s : QString();

        emit requestToApplication(requestIdUuid, id, requestingApplicationId,
                                  applicationId, convertFromDBusVariant(parameters).toMap());
    });
}

void IntentClientDBusImplementation::requestToSystem(const QPointer<IntentClientRequest> &icr)
{
    QDBusPendingReply<QString> reply =
            m_dbusInterface->requestToSystem(icr->intentId(), icr->applicationId(),
                                             convertToDBusVariant(icr->parameters()).toMap());

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, icr);
    connect(watcher, &QDBusPendingCallWatcher::finished, icr, [this, watcher, icr]() {
        watcher->deleteLater();

        QDBusPendingReply<QString> replyCopy = *watcher;
        emit requestToSystemFinished(icr, QUuid::fromString(replyCopy.argumentAt<0>()),
                                     replyCopy.isError(), replyCopy.error().message());
    });
}

void IntentClientDBusImplementation::replyFromApplication(const QPointer<IntentClientRequest> &icr)
{
    if (icr) {
        m_dbusInterface->replyFromApplication(icr->requestId().toString(), !icr->succeeded(),
                                              convertToDBusVariant(icr->result()).toMap());
        // Checking for errors here is futile. Even if we would detect an error or timeout, we
        // couldn't do anything about it, as that would mean that the AM process is dead or frozen.
    }
}

QT_END_NAMESPACE_AM

#include "moc_intentclientdbusimplementation.cpp"
