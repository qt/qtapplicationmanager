// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
#ifndef SIGNATURE_H
#define SIGNATURE_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QVersionNumber>
#include <QtAppManPackage/qtappmanpackageglobal.h>
#include <QtAppManPackage/certificate.h>

QT_BEGIN_NAMESPACE_AM

class SignaturePrivate;

class Q_APPMANPACKAGE_EXPORT Signature
{
public:
    explicit Signature(const QByteArray &hash);
    ~Signature();

    QByteArray create(const QByteArray &signingCertificatePkcs12,
                      const QByteArray &signingCertificatePassword,
                      Certificate *signerCertificate = nullptr) noexcept(false);

    struct VerificationResult
    {
        // The issuers list contains the complete verification chain from the direct issuer
        // to the root certificate.
        // Chain order: issuers[0] = direct issuer, issuers[n-1] = root certificate

        QList<Certificate> chain;

        Certificate signer() const { return !chain.isEmpty() ? chain.constFirst() : Certificate(); };
        QList<Certificate> issuers() const { return chain.mid(1); }

        bool isValid() const
        {
            return (chain.size() >= 2) && std::all_of(chain.begin(), chain.end(),
                                                      [](const auto &cert) { return cert.isValid(); });
        }
    };

    VerificationResult verify(const QByteArray &signaturePkcs7,
                              const QByteArrayList &trustedCertsData) noexcept(false);

    enum class CertificateRole {
        Any,
        Issuer,
        Intermediate,
        Root,
    };

    void requireMinimumCertificateVersion(const QVersionNumber &version);
    void requireKeyUsage(Certificate::KeyUsages keyUsages);
    void requirePackageId(const QString &packageId);
    void requireApplicationIds(const QStringList &applicationIds);
    void requireCapabilities(const QStringList &capabilities);
    void requireCategories(const QStringList &categories);
    void requireRuntimes(const QStringList &runtimes);
    void requireCertificateRoles(const QHash<QByteArray, CertificateRole> &trustedCertDataToRole);
    void requireRevocationCheck(const QByteArrayList &crls);

private:
    SignaturePrivate *d;
    Q_DISABLE_COPY_MOVE(Signature)
};

QT_END_NAMESPACE_AM

#endif // SIGNATURE_H
