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
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class IntentServer;
class IntentServerRequest;

class IntentServerSystemInterface : public QObject
{
    Q_OBJECT

public:
    virtual ~IntentServerSystemInterface() = default;

    virtual void initialize(IntentServer *intentServer);
    IntentServer *intentServer() const;

    class IpcConnection;
    virtual IpcConnection *findClientIpc(const QString &appId) = 0;

    virtual void startApplication(const QString &appId) = 0;


    virtual bool checkApplicationCapabilities(const QString &applicationId,
                                              const QStringList &requiredCapabilities) = 0;

    IntentServerRequest *requestToSystem(const QString &requestingApplicationId, const QString &intentId,
                                         const QString &applicationId, const QVariantMap &parameters);
    virtual void replyFromSystem(IpcConnection *clientIPC, IntentServerRequest *irs) = 0;

    virtual void requestToApplication(IpcConnection *clientIPC, IntentServerRequest *irs) = 0;

signals:
    void applicationWasStarted(const QString &appId);
    void replyFromApplication(const QString &replyingApplicationId, const QUuid &requestId,
                              bool error, const QVariantMap &result);

private:
    IntentServer *m_is = nullptr;
};

QT_END_NAMESPACE_AM
