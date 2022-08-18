// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "intentserversysteminterface.h"
#include "intentserver.h"

QT_BEGIN_NAMESPACE_AM

void IntentServerSystemInterface::initialize(IntentServer *intentServer)
{
    m_is = intentServer;

    connect(this, &IntentServerSystemInterface::replyFromApplication,
            m_is, &IntentServer::replyFromApplication);

    // we need to explicitly decouple here via QueuedConnection. Otherwise the request queue
    // handling in IntentServer can get out of sync
    connect(this, &IntentServerSystemInterface::applicationWasStarted,
            m_is, &IntentServer::applicationWasStarted, Qt::QueuedConnection);
}

IntentServer *IntentServerSystemInterface::intentServer() const
{
    return m_is;
}

IntentServerRequest *IntentServerSystemInterface::requestToSystem(const QString &requestingApplicationId,
                                                                  const QString &intentId,
                                                                  const QString &applicationId,
                                                                  const QVariantMap &parameters)
{
    // not possible to do via signal, due to return value
    return m_is->requestToSystem(requestingApplicationId, intentId, applicationId, parameters);
}

QT_END_NAMESPACE_AM

#include "moc_intentserversysteminterface.cpp"
