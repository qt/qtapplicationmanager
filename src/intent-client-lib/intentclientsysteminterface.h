// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef INTENTCLIENTSYSTEMINTERFACE_H
#define INTENTCLIENTSYSTEMINTERFACE_H

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
    ~IntentClientSystemInterface() override = default;

    virtual void initialize(IntentClient *intentClient) noexcept(false);

    virtual QString currentApplicationId(QObject *hint) = 0;

    virtual void requestToSystem(const QPointer<IntentClientRequest> &icr) = 0;
    virtual void replyFromApplication(const QPointer<IntentClientRequest> &icr) = 0;

signals:
    void requestToSystemFinished(const QPointer<QtAM::IntentClientRequest> &icr,
                                 const QUuid &newRequestId, bool error, const QString &errorMessage);
    void replyFromSystem(const QUuid &requestId, bool error, const QVariantMap &result);

    void requestToApplication(const QUuid &requestId, const QString &intentId,
                              const QString &requestingApplicationId, const QString &applicationId,
                              const QVariantMap &parameters);

protected:
    IntentClient *m_ic = nullptr;
};

QT_END_NAMESPACE_AM

#endif // INTENTCLIENTSYSTEMINTERFACE_H
