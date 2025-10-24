// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
#ifndef SIGNATURE_H
#define SIGNATURE_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtAppManCrypto/qtappmancryptoglobal.h>

#include "certificate.h"

QT_BEGIN_NAMESPACE_AM

class SignaturePrivate;

class Q_APPMANCRYPTO_EXPORT Signature
{
public:
    explicit Signature(const QByteArray &hash);
    ~Signature();

    QByteArray create(const QByteArray &signingCertificatePkcs12,
                      const QByteArray &signingCertificatePassword) noexcept(false);

    struct VerificationResult
    {
        //TODO: Even though the issuers are a list, I have not yet found a way to return the
        // complete chain of issuers for all the backends. All do support the direct issuer, though.

        Certificate signer;
        QList<Certificate> issuers;
        bool isValid() const { return signer.isValid() && !issuers.isEmpty() && issuers.constFirst().isValid(); }
    };

    VerificationResult verify(const QByteArray &signaturePkcs7,
                              const QByteArrayList &chainOfTrust) noexcept(false);

    enum class FingerprintHash {
        Sha256 = 1
    };

    void requireKeyUsage(Certificate::KeyUsages keyUsages);
    void requirePackageId(const QString &packageId);
    void requireIssuerFingerprint(FingerprintHash hash, const QStringList &fingerprints); // hex-encoded "01:23:34:..."
    void requireRevocationCheck(const QByteArrayList &crls);

private:
    SignaturePrivate *d;
    Q_DISABLE_COPY_MOVE(Signature)
};

QT_END_NAMESPACE_AM

#endif // SIGNATURE_H
