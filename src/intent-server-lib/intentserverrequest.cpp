// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "intentserverrequest.h"

QT_BEGIN_NAMESPACE_AM

IntentServerRequest::IntentServerRequest(const QString &requestingApplicationId, const QString &intentId,
                                         const QVector<Intent *> &potentialIntents,
                                         const QVariantMap &parameters)
    : m_id(QUuid::createUuid())
    , m_state(State::ReceivedRequest)
    , m_intentId(intentId)
    , m_requestingApplicationId(requestingApplicationId)
    , m_potentialIntents(potentialIntents)
    , m_parameters(parameters)
{
    Q_ASSERT(!potentialIntents.isEmpty());

    if (potentialIntents.size() == 1)
        setHandlingApplicationId(potentialIntents.first()->applicationId());
}

IntentServerRequest::State IntentServerRequest::state() const
{
    return m_state;
}

QString IntentServerRequest::intentId() const
{
    return m_intentId;
}

QUuid IntentServerRequest::requestId() const
{
    return m_id;
}

QString IntentServerRequest::requestingApplicationId() const
{
    return m_requestingApplicationId;
}

QString IntentServerRequest::handlingApplicationId() const
{
    return m_handlingApplicationId;
}

QVector<Intent *> IntentServerRequest::potentialIntents() const
{
    return m_potentialIntents;
}

QVariantMap IntentServerRequest::parameters() const
{
    return m_parameters;
}

bool IntentServerRequest::succeeded() const
{
    return m_succeeded;
}

QVariantMap IntentServerRequest::result() const
{
    return m_result;
}

void IntentServerRequest::setRequestFailed(const QString &errorMessage)
{
    m_succeeded = false;
    m_result.clear();
    m_result[qSL("errorMessage")] = errorMessage;
    m_state = State::ReceivedReplyFromApplication;
}

void IntentServerRequest::setRequestSucceeded(const QVariantMap &result)
{
    m_succeeded = true;
    m_result = result;
    m_state = State::ReceivedReplyFromApplication;
}

void IntentServerRequest::setState(IntentServerRequest::State newState)
{
    m_state = newState;
}

void IntentServerRequest::setHandlingApplicationId(const QString &applicationId)
{
    m_handlingApplicationId = applicationId;
}

QT_END_NAMESPACE_AM

#include "moc_intentserverrequest.cpp"
