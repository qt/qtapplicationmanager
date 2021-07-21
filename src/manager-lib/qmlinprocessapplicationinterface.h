/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
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
****************************************************************************/

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
