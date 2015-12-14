/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QQmlParserStatus>
#include <QPointer>
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
    explicit QmlApplicationInterface(const QString &dbusConnectionName, QObject *parent = 0);
    bool initialize();

    QString applicationId() const override;
    Q_INVOKABLE Notification *createNotification();

private slots:
    void notificationClosed(uint notificationId, uint reason);
    void notificationActivated(uint notificationId, const QString &actionId);

private:
    Q_SIGNAL void startApplication(const QString &qmlFile, const QString &document, const QVariantMap &runtimeParams);

    uint notificationShow(QmlNotification *n);
    void notificationClose(QmlNotification *n);

    mutable QString m_appId; // cached
    QDBusConnection m_connection;
    QDBusInterface *m_applicationIf;
    QDBusInterface *m_runtimeIf;
    QDBusInterface *m_notifyIf;
    QList<QPointer<QmlNotification> > m_allNotifications;

    static QmlApplicationInterface *s_instance;

    friend class QmlNotification;
    friend class Controller;
};
