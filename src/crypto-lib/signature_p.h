// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCrypto/signature.h>

QT_BEGIN_NAMESPACE_AM

class SignaturePrivate
{
public:
    QByteArray hash;
    QString error;

    QByteArray create(const QByteArray &signingCertificatePkcs12,
                      const QByteArray &signingCertificatePassword) noexcept(false);
    bool verify(const QByteArray &signaturePkcs7,
                const QByteArrayList &chainOfTrust) noexcept(false);
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
