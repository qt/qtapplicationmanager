// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QList>
#include <QMap>
#include <QPair>
#include <QElapsedTimer>

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class IntentHandler;
class IntentClientRequest;
class IntentClientSystemInterface;

/* internal interface used by IntentRequest and IntentHandler */

class IntentClient : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/IntentClient 2.1 SINGLETON")
    Q_PROPERTY(QString systemUiId READ systemUiId CONSTANT REVISION 1)

public:
    ~IntentClient() override;
    static IntentClient *createInstance(IntentClientSystemInterface *systemInterface);
    static IntentClient *instance();

    QString systemUiId() const;

    int replyFromSystemTimeout() const;
    void setReplyFromSystemTimeout(int timeout);
    void setReplyFromApplicationTimeout(int timeout);

    IntentClientRequest *requestToSystem(const QString &requestingApplicationId, const QString &intentId,
                                         const QString &applicationId, const QVariantMap &parameters);
    void replyFromApplication(IntentClientRequest *icr, const QVariantMap &result);
    void errorReplyFromApplication(IntentClientRequest *icr, const QString &errorMessage);

    void registerHandler(IntentHandler *handler);
    void unregisterHandler(IntentHandler *handler);

    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(IntentClientRequest) *sendIntentRequest(const QString &intentId,
                                                                                const QVariantMap &parameters);
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(IntentClientRequest) *sendIntentRequest(const QString &intentId,
                                                                                const QString &applicationId,
                                                                                const QVariantMap &parameters);

    Q_REVISION(1) Q_INVOKABLE bool broadcastIntentRequest(const QString &intentId,
                                                             const QVariantMap &parameters);

private:
    void requestToSystemFinished(IntentClientRequest *icr, const QUuid &newRequestId,
                                 bool error, const QString &errorMessage);
    void replyFromSystem(const QUuid &requestId, bool error, const QVariantMap &result);

    void requestToApplication(const QUuid &requestId, const QString &intentId, const QString &requestingApplicationId,
                              const QString &applicationId, const QVariantMap &parameters);

private:
    IntentClient(IntentClientSystemInterface *systemInterface, QObject *parent = nullptr);
    Q_DISABLE_COPY(IntentClient)
    static IntentClient *s_instance;

    QList<QPointer<IntentClientRequest>> m_waiting;
    QElapsedTimer m_lastWaitingCleanup;
    QMap<QPair<QString, QString>, IntentHandler *> m_handlers; // intentId + appId -> handler

    // no timeouts by default -- these have to be set at runtime
    int m_replyFromSystemTimeout = 0;
    int m_replyFromApplicationTimeout = 0;

    IntentClientSystemInterface *m_systemInterface;
    friend class IntentClientSystemInterface;
};

QT_END_NAMESPACE_AM
