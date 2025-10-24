// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SIGNATURE_P_H
#define SIGNATURE_P_H

#include <QStringList>
#include <QtAppManCommon/exception.h>
#include <QtAppManPackage/signature.h>

QT_BEGIN_NAMESPACE_AM

class SignaturePrivate
{
public:
    QByteArray hash;
    Certificate::KeyUsages requiredKeyUsages;
    QString requirePackageId;
    QStringList requiredIssuerFingerprints;
    QByteArrayList requiredCRLs;

    QByteArray create(const QByteArray &signingCertificatePkcs12,
                      const QByteArray &signingCertificatePassword,
                      const std::function<void(const Certificate &)> &checkCertificate) noexcept(false);

    Signature::VerificationResult verify(const QByteArray &signaturePkcs7,
                                         const QByteArrayList &chainOfTrust) noexcept(false);

    void checkCertificate(const Certificate &signer, const Certificate &issuer) noexcept(false);

    static void setDNByOid(QVariantMap &map, const QString &oid, const QString &name);

    template<typename PARSER, typename CERT>
    Signature::VerificationResult createSignatureVerificationResult(PARSER parser, CERT signerCert,
                                                                    CERT issuerCert)
    {
        try {
            auto signer = parser(signerCert);
            try {
                auto issuer = parser(issuerCert);
                return { signer, { issuer } };
            } catch (const Exception &e) {
                throw Exception("Could not parse the direct issuer certificate: %1").arg(e.errorString());
            }
        } catch (const Exception &e) {
            throw Exception("Could not parse the signer certificate: %1").arg(e.errorString());
        }
    }
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // SIGNATURE_P_H
