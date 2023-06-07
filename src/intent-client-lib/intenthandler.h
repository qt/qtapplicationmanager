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

class AbstractIntentHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager/AbstractIntentHandler 2.0 UNCREATABLE")

public:
    AbstractIntentHandler(QObject *parent = nullptr);
    ~AbstractIntentHandler() override;

    QStringList intentIds() const;
    void setIntentIds(const QStringList &intentId);

signals:
    void intentIdsChanged();

    void requestReceived(QT_PREPEND_NAMESPACE_AM(IntentClientRequest) *request);

protected:
    void classBegin() override;
    void componentComplete() override;
    bool isComponentCompleted() const;

private:
    Q_DISABLE_COPY(AbstractIntentHandler)

    QStringList m_intentIds;
    bool m_completed = false;
};

class IntentHandler : public AbstractIntentHandler
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.Application/IntentHandler 2.0")
    Q_PROPERTY(QStringList intentIds READ intentIds WRITE setIntentIds NOTIFY intentIdsChanged)

public:
    IntentHandler(QObject *parent = nullptr);

private:
    Q_DISABLE_COPY(IntentHandler)
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QT_PREPEND_NAMESPACE_AM(IntentHandler) *)
