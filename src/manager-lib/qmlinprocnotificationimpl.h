// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QMLINPROCNOTIFICATIONIMPL_H
#define QMLINPROCNOTIFICATIONIMPL_H

#include <QtCore/QVector>
#include <QtCore/QPointer>
#include <QtQml/QQmlParserStatus>

#include <QtAppManSharedMain/notificationimpl.h>


QT_BEGIN_NAMESPACE_AM

class QmlInProcNotificationImpl : public NotificationImpl
{
public:
    QmlInProcNotificationImpl(Notification *notification, const QString &applicationId);

    static void initialize();

    void componentComplete() override;
    uint show() override;
    void close() override;

private:
    QString m_applicationId;

    static QVector<QPointer<Notification>> s_allNotifications;

    Q_DISABLE_COPY_MOVE(QmlInProcNotificationImpl)
    friend class QmlInProcApplicationInterfaceImpl;
};

QT_END_NAMESPACE_AM

#endif // QMLINPROCNOTIFICATIONIMPL_H
