// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
#pragma once

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class SignaturePrivate;

class Signature
{
public:
    explicit Signature(const QByteArray &hash);
    ~Signature();

    QByteArray create(const QByteArray &signingCertificatePkcs12, const QByteArray &signingCertificatePassword);
    bool verify(const QByteArray &signaturePkcs7, const QList<QByteArray> &chainOfTrust);

    QString errorString() const;

private:
    SignaturePrivate *d;
    Q_DISABLE_COPY_MOVE(Signature)
};

QT_END_NAMESPACE_AM
