// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SIGNATURE_P_H
#define SIGNATURE_P_H

#include <QStringList>
#include <QHash>
#include <QVariantMap>
#include <QtAppManCommon/exception.h>
#include <QtAppManPackage/signature.h>

QT_BEGIN_NAMESPACE_AM

class SignaturePrivate
{
public:
    QByteArray hash;
    Certificate::KeyUsages requiredKeyUsages;
    QString requirePackageId;
    QStringList requiredApplicationIds;
    QStringList requiredCapabilities;
    QStringList requiredCategories;
    QHash<QByteArray, Signature::CertificateRole> requiredRoles;
    QByteArrayList requiredCRLs;

    QByteArray create(const QByteArray &signingCertificatePkcs12,
                      const QByteArray &signingCertificatePassword,
                      const std::function<void(const Certificate &)> &checkCertificate) noexcept(false);

    Signature::VerificationResult verify(const QByteArray &signaturePkcs7,
                                         const QByteArrayList &trustedCertsData) noexcept(false);

    void checkSignerCertificate(const Certificate &signer) noexcept(false);

    static void setDNByOid(QVariantMap &map, const QString &oid, const QString &name);

    static void verifyCertificateRole(const QString &subject, int position, int chainSize,
                                      Signature::CertificateRole role) noexcept(false);
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // SIGNATURE_P_H
