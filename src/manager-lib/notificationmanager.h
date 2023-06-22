// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QAbstractListModel>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QJSEngine)

QT_BEGIN_NAMESPACE_AM

class NotificationManagerPrivate;

class NotificationManager : public QAbstractListModel
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
    Q_CLASSINFO("AM-QmlType", "QtApplicationManager.SystemUI/NotificationManager 2.0 SINGLETON")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    ~NotificationManager() override;
    static NotificationManager *createInstance();
    static NotificationManager *instance();
    static QObject *instanceForQml(QQmlEngine *qmlEngine, QJSEngine *);

    // the item model part
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QVariantMap notification(uint id) const;
    Q_INVOKABLE int indexOfNotification(uint id) const;

    Q_INVOKABLE void acknowledgeNotification(uint id);
    Q_INVOKABLE void triggerNotificationAction(uint id, const QString &actionId);
    Q_INVOKABLE void dismissNotification(uint id);

    // vv libnotify DBus interface
    Q_SCRIPTABLE QString GetServerInformation(QString &vendor, QString &version, QString &spec_version);
    Q_SCRIPTABLE QStringList GetCapabilities();
    Q_SCRIPTABLE uint Notify(const QString &app_name, uint replaces_id, const QString &app_icon, const QString &summary, const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout);
    Q_SCRIPTABLE void CloseNotification(uint id);

signals:
    Q_SCRIPTABLE void NotificationClosed(uint id, uint reason);
    Q_SCRIPTABLE void ActionInvoked(uint id, const QString &action_key);
    // ^^ libnotify DBus interface

signals:
    void countChanged();
    void notificationAdded(uint id);
    void notificationAboutToBeRemoved(uint id);
    void notificationChanged(uint id, const QStringList &rolesChanged);

private:
    uint notifyHelper(const QString &app_name, uint id, bool replaces, const QString &app_icon, const QString &summary, const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout);

private:
    NotificationManager(QObject *parent = nullptr);
    NotificationManager(const NotificationManager &);
    NotificationManager &operator=(const NotificationManager &);
    static NotificationManager *s_instance;

    NotificationManagerPrivate *d;
    friend class NotificationManagerPrivate;
};

QT_END_NAMESPACE_AM
