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

#include "intentserversysteminterface.h"
#include "intentserver.h"

QT_BEGIN_NAMESPACE_AM

void IntentServerSystemInterface::initialize(IntentServer *intentServer)
{
    m_is = intentServer;

    connect(this, &IntentServerSystemInterface::replyFromApplication,
            m_is, &IntentServer::replyFromApplication);
    connect(this, &IntentServerSystemInterface::applicationWasStarted,
            m_is, &IntentServer::applicationWasStarted);
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
