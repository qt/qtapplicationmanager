// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef INTENTCLIENTDBUSIMPLEMENTATION_H
#define INTENTCLIENTDBUSIMPLEMENTATION_H

#include <QtCore/QPointer>
#include <QtAppManIntentClient/intentclientsysteminterface.h>

class IoQtApplicationManagerIntentInterfaceInterface;

QT_BEGIN_NAMESPACE_AM

class IntentClientDBusImplementation : public IntentClientSystemInterface
{
    Q_OBJECT
public:
    IntentClientDBusImplementation(const QString &dbusName, QObject *parent = nullptr);

    void initialize(IntentClient *intentClient) noexcept(false) override;
    bool isSystemUI() const override;
    QString currentApplicationId(QObject *hint) override;

    void requestToSystem(const QPointer<IntentClientRequest> &icr) override;
    void replyFromApplication(const QPointer<IntentClientRequest> &icr) override;

private:
    IoQtApplicationManagerIntentInterfaceInterface *m_dbusInterface = nullptr;
    QString m_dbusName;
};

QT_END_NAMESPACE_AM

#endif // INTENTCLIENTDBUSIMPLEMENTATION_H
