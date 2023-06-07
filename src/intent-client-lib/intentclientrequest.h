// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QDir>
#include <QtCore/QUuid>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class IntentClient;

class IntentClientRequest : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/IntentRequest 2.1 UNCREATABLE")

    Q_PROPERTY(QUuid requestId READ requestId NOTIFY requestIdChanged)
    Q_PROPERTY(Direction direction READ direction CONSTANT)
    Q_PROPERTY(QString intentId READ intentId CONSTANT)
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT)
    Q_PROPERTY(QString requestingApplicationId READ requestingApplicationId CONSTANT)
    Q_PROPERTY(QVariantMap parameters READ parameters CONSTANT)
    Q_PROPERTY(bool succeeded READ succeeded NOTIFY replyReceived)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY replyReceived)
    Q_PROPERTY(QVariantMap result READ result NOTIFY replyReceived)
    Q_PROPERTY(bool broadcast READ isBroadcast CONSTANT REVISION(2, 1))

public:
    enum class Direction { ToSystem, ToApplication };
    Q_ENUM(Direction)

    ~IntentClientRequest() override;

    QUuid requestId() const;
    Direction direction() const;
    QString intentId() const;
    QString applicationId() const;
    QString requestingApplicationId() const;
    QVariantMap parameters() const;
    bool isBroadcast() const;

    const QVariantMap result() const;
    bool succeeded() const;
    QString errorMessage() const;

    Q_INVOKABLE void sendReply(const QVariantMap &result);
    Q_INVOKABLE void sendErrorReply(const QString &errorMessage);

    void startTimeout(int timeout);

signals:
    void requestIdChanged();
    void replyReceived();

protected:
    void connectNotify(const QMetaMethod &signal) override;

private:
    IntentClientRequest(Direction direction, const QString &requestingApplicationId, const QUuid &id,
                        const QString &intentId, const QString &applicationId, const QVariantMap &parameters,
                        bool broadcast);

    void setRequestId(const QUuid &requestId);
    void setResult(const QVariantMap &result);
    void setErrorMessage(const QString &errorMessage);
    void doFinish();

    Direction m_direction;
    QUuid m_id;
    QString m_intentId;
    QString m_requestingApplicationId;
    QString m_applicationId;
    QVariantMap m_parameters;
    QVariantMap m_result;
    QString m_errorMessage;
    bool m_succeeded = false;
    // The client side of this request is finished - the object will stay in this state until
    // garbage collected by the JS engine. The state is entered, depending on the direction:
    //  ToSystem: we have received the final result or errorMessage via replyReceived()
    //  ToApplication: the request was handled and send(Error)Reply was called
    bool m_finished = false;
    bool m_broadcast = false;

    Q_DISABLE_COPY(IntentClientRequest)

    friend class IntentClient;
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(IntentClientRequest) *)
