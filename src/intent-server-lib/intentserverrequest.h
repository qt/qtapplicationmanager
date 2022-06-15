// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QUuid>
#include <QVector>
#include <QtAppManCommon/global.h>
#include <QtAppManIntentServer/intent.h>

QT_BEGIN_NAMESPACE_AM

class IntentServer;

class IntentServerRequest
{
    Q_GADGET

public:
    IntentServerRequest(const QString &requestingApplicationId, const QString &intentId,
                        const QVector<Intent *> &potentialIntents, const QVariantMap &parameters);

    enum class State {
        ReceivedRequest,
        WaitingForDisambiguation,
        Disambiguated,
        WaitingForApplicationStart,
        StartedApplication,
        WaitingForReplyFromApplication,
        ReceivedReplyFromApplication,
    };

    Q_ENUM(State)

    State state() const;
    QUuid requestId() const;
    QString intentId() const;
    QString requestingApplicationId() const;
    QString handlingApplicationId() const;
    QVector<Intent *> potentialIntents() const;
    QVariantMap parameters() const;
    bool succeeded() const;
    QVariantMap result() const;

    void setState(State newState);
    void setHandlingApplicationId(const QString &applicationId);

    void setRequestFailed(const QString &errorMessage);
    void setRequestSucceeded(const QVariantMap &result);

private:
    QUuid m_id;
    State m_state;
    bool m_succeeded = false;
    QString m_intentId;
    QString m_requestingApplicationId;
    QString m_handlingApplicationId;
    QVector<Intent *> m_potentialIntents;
    QVariantMap m_parameters;
    QVariantMap m_result;
};

QT_END_NAMESPACE_AM
