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

#pragma once

#include <QObject>
#include <QPointer>
#include <QUuid>
#include <QVariantMap>
#include <QtAppManCommon/global.h>


QT_BEGIN_NAMESPACE_AM

class IntentClient;
class IntentClientRequest;

class IntentClientSystemInterface : public QObject
{
    Q_OBJECT

public:
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
