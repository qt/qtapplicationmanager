/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QQmlParserStatus>
#include <QPointer>
#include <QHash>
#include <QVector>
#include <QDBusConnection>

#include "../../manager-lib/applicationinterface.h"
#include "notification.h"

QT_FORWARD_DECLARE_CLASS(QDBusInterface)

class QmlNotification : public Notification
{
public:
    QmlNotification(QObject *parent = 0, Notification::ConstructionMode mode = Notification::Declarative);

protected:
    uint libnotifyShow() override;
    void libnotifyClose() override;

    friend class QmlApplicationInterface;
};

class QmlApplicationInterface : public ApplicationInterface
{
    Q_OBJECT

public:
    explicit QmlApplicationInterface(const QVariantMap &additionalConfiguration, const QString &dbusConnectionName, QObject *parent = 0);
    bool initialize();

    QString applicationId() const override;
    QVariantMap additionalConfiguration() const override;
    Q_INVOKABLE Notification *createNotification();

private slots:
    void notificationClosed(uint notificationId, uint reason);
    void notificationActionTriggered(uint notificationId, const QString &actionId);
private:
    Q_SIGNAL void startApplication(const QString &qmlFile, const QString &document, const QVariantMap &runtimeParams);

    uint notificationShow(QmlNotification *n);
    void notificationClose(QmlNotification *n);

    mutable QString m_appId; // cached
    QDBusConnection m_connection;
    QDBusInterface *m_applicationIf;
    QDBusInterface *m_runtimeIf;
    QDBusInterface *m_notifyIf;
    QVariantMap m_additionalConfiguration;
    QVector<QPointer<QmlNotification> > m_allNotifications;

    static QmlApplicationInterface *s_instance;

    friend class QmlNotification;
    friend class Controller;
};


class QmlApplicationInterfaceExtensionPrivate;

class QmlApplicationInterfaceExtension : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)
    Q_PROPERTY(QObject *object READ object NOTIFY objectChanged)

public:
    static void initialize(const QDBusConnection &connection);

    explicit QmlApplicationInterfaceExtension(QObject *parent = nullptr);

    QString name() const;
    bool isReady() const;
    QObject *object() const;

protected:
    void classBegin() override;
    void componentComplete() override;

public slots:
    void setName(const QString &name);

signals:
    void readyChanged();
    void objectChanged();

private:
    static QmlApplicationInterfaceExtensionPrivate *d;
    QString m_name;
    QObject *m_object = nullptr;
    bool m_complete = false;
};
