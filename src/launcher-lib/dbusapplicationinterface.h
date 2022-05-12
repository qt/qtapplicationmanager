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

#include <QPointer>
#include <QVector>
#include <QDBusConnection>

#include <QtAppManCommon/global.h>
#include <QtAppManApplication/applicationinterface.h>
#include <QtAppManNotification/notification.h>

QT_FORWARD_DECLARE_CLASS(QDBusInterface)

QT_BEGIN_NAMESPACE_AM

class DBusNotification;
class Notification;
class IntentClientRequest;
class Controller;


class DBusApplicationInterface : public ApplicationInterface
{
    Q_OBJECT
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.Application/ApplicationInterface 2.0 UNCREATABLE")

public:
    explicit DBusApplicationInterface(const QString &dbusConnectionName,
                                     const QString &dbusNotificationBusName, QObject *parent = nullptr);
    bool initialize(bool hasRuntime = false);

    QString applicationId() const override;
    QVariantMap name() const override;
    QUrl icon() const override;
    QString version() const override;
    QVariantMap systemProperties() const override;
    QVariantMap applicationProperties() const override;
    Q_INVOKABLE QT_PREPEND_NAMESPACE_AM(Notification *) createNotification();
    Q_INVOKABLE void acknowledgeQuit() const;
    Q_INVOKABLE void finishedInitialization() override;

private slots:
    void notificationClosed(uint notificationId, uint reason);
    void notificationActionTriggered(uint notificationId, const QString &actionId);
private:
    Q_SIGNAL void startApplication(const QString &baseDir, const QString &qmlFile, const QString &document,
                                   const QString &mimeType, const QVariantMap &runtimeParams,
                                   const QVariantMap systemProperties);

    uint notificationShow(DBusNotification *n);
    void notificationClose(DBusNotification *n);

    QDBusConnection m_connection;
    QDBusConnection m_notificationConnection;
    QDBusInterface *m_applicationIf = nullptr;
    QDBusInterface *m_runtimeIf = nullptr;
    QDBusInterface *m_notifyIf = nullptr;
    mutable QString m_appId; // cached
    QVariantMap m_name;
    QString m_icon;
    QString m_version;
    QVariantMap m_systemProperties;
    QVariantMap m_applicationProperties;
    QVector<QPointer<DBusNotification> > m_allNotifications;

    static DBusApplicationInterface *s_instance;

    friend class DBusNotification;
    friend class Controller;
};

QT_END_NAMESPACE_AM
