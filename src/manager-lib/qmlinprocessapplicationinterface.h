// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QVector>
#include <QPointer>
#include <QQmlParserStatus>

#include <QtAppManApplication/applicationinterface.h>
#include <QtAppManNotification/notification.h>

QT_BEGIN_NAMESPACE_AM

class QmlInProcessRuntime;
class IntentClientRequest;

class QmlInProcessNotification : public Notification // clazy:exclude=missing-qobject-macro
{
public:
    QmlInProcessNotification(QObject *parent = nullptr, ConstructionMode mode = Declarative);

    void componentComplete() override;

    static void initialize();

protected:
    uint libnotifyShow() override;
    void libnotifyClose() override;

private:
    ConstructionMode m_mode;
    QString m_appId;

    static QVector<QPointer<QmlInProcessNotification> > s_allNotifications;

    friend class QmlInProcessApplicationInterface;
};


class QmlInProcessApplicationInterface : public ApplicationInterface
{
    Q_OBJECT

public:
    explicit QmlInProcessApplicationInterface(QmlInProcessRuntime *runtime = nullptr);

    QString applicationId() const override;
    QVariantMap name() const override;
    QUrl icon() const override;
    QString version() const override;
    QVariantMap systemProperties() const override;
    QVariantMap applicationProperties() const override;

    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Notification *) createNotification();
    Q_INVOKABLE void acknowledgeQuit();

    void finishedInitialization() override;

signals:
    void quitAcknowledged();
private:
    QmlInProcessRuntime *m_runtime;
    friend class QmlInProcessRuntime;
};

QT_END_NAMESPACE_AM
