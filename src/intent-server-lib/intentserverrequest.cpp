// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "intentserverrequest.h"

QT_BEGIN_NAMESPACE_AM

IntentServerRequest::IntentServerRequest(const QString &requestingApplicationId, const QString &intentId,
                                         const QVector<Intent *> &potentialIntents,
                                         const QVariantMap &parameters, bool broadcast)
    : m_id(QUuid::createUuid())
    , m_state(State::ReceivedRequest)
    , m_broadcast(broadcast)
    , m_intentId(intentId)
    , m_requestingApplicationId(requestingApplicationId)
    , m_parameters(parameters)
{
    Q_ASSERT(!potentialIntents.isEmpty());
    Q_ASSERT(!broadcast || (potentialIntents.size() == 1));

    // Intents may die during handling (app removal), so we create shadow copies for QML
    m_potentialIntents.reserve(potentialIntents.size());
    for (auto *intent : potentialIntents) {
        Q_ASSERT(intent);
        auto copy = intent->copy();
        copy->setParent(this); // for automatic deletion and ownership
        m_potentialIntents << copy;
    }

    if (potentialIntents.size() == 1)
        m_selectedIntent = m_potentialIntents.constFirst();
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

Intent *IntentServerRequest::selectedIntent() const
{
    return m_selectedIntent;
}

QList<Intent *> IntentServerRequest::potentialIntents() const
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

bool IntentServerRequest::isBroadcast() const
{
    return m_broadcast;
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

void IntentServerRequest::setSelectedIntent(Intent *intent)
{
    if (!intent || m_potentialIntents.contains(intent))
        m_selectedIntent = intent;
}

QT_END_NAMESPACE_AM

#include "moc_intentserverrequest.cpp"
