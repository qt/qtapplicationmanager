// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtQml/QQmlParserStatus>
#include <QtCore/QUuid>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class IntentClientRequest;

// This base class is used in 2 derived classes, each in a different QML namespace.
// Having the common signals here does work, but messes up the code model in Creator.
class AbstractIntentHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

public:
    AbstractIntentHandler(QObject *parent = nullptr);
    ~AbstractIntentHandler() override;

    QStringList intentIds() const;

    virtual void internalRequestReceived(IntentClientRequest *req) = 0;

protected:
    QStringList m_intentIds;

    Q_DISABLE_COPY_MOVE(AbstractIntentHandler)
};

class IntentHandler : public AbstractIntentHandler
{
    Q_OBJECT
    Q_PROPERTY(QStringList intentIds READ intentIds WRITE setIntentIds NOTIFY intentIdsChanged FINAL)

public:
    IntentHandler(QObject *parent = nullptr);

    void setIntentIds(const QStringList &intentId);

signals:
    void intentIdsChanged();
    void requestReceived(QtAM::IntentClientRequest *request);

protected:
    void classBegin() override;
    void componentComplete() override;
    void internalRequestReceived(IntentClientRequest *request) override;

private:
    bool m_completed = false;

    Q_DISABLE_COPY_MOVE(IntentHandler)
};

QT_END_NAMESPACE_AM
