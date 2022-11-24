/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
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
****************************************************************************/

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
