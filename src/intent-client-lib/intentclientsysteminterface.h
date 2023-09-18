// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QUuid>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>


QT_BEGIN_NAMESPACE_AM

class IntentClient;
class IntentClientRequest;

class IntentClientSystemInterface : public QObject
{
    Q_OBJECT

public:
    IntentClientSystemInterface(QObject *parent = nullptr);
    virtual ~IntentClientSystemInterface() = default;

    virtual void initialize(IntentClient *intentClient) Q_DECL_NOEXCEPT_EXPR(false);

    virtual QString currentApplicationId(QObject *hint) = 0;

    virtual void requestToSystem(QPointer<IntentClientRequest> icr) = 0;
    virtual void replyFromApplication(QPointer<IntentClientRequest> icr) = 0;

signals:
    void requestToSystemFinished(QPointer<QT_PREPEND_NAMESPACE_AM(IntentClientRequest)> icr,
                                 const QUuid &newRequestId, bool error, const QString &errorMessage);
    void replyFromSystem(const QUuid &requestId, bool error, const QVariantMap &result);

    void requestToApplication(const QUuid &requestId, const QString &intentId,
                              const QString &requestingApplicationId, const QString &applicationId,
                              const QVariantMap &parameters);

protected:
    IntentClient *m_ic = nullptr;
};

QT_END_NAMESPACE_AM
