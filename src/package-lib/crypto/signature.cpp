// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:cryptography

#include <QSet>

#include "signature.h"
#include "signature_p.h"
#include "exception.h"
#include "logging.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

Signature::Signature(const QByteArray &hash)
    : d(new SignaturePrivate)
{
    // We have to use ASCII here since the default S/MIME content is text/plain.
    // This is what can be supported easily cross-platform without diving
    // deeply into low-level PKCS7 APIs.
    d->hash = hash.toBase64();
}

Signature::~Signature()
{
    delete d;
}

void Signature::requireKeyUsage(Certificate::KeyUsages keyUsages)
{
    d->requiredKeyUsages = keyUsages;
}

void Signature::requirePackageId(const QString &packageId)
{
    d->requirePackageId = packageId;
}

void Signature::requireApplicationIds(const QStringList &applicationIds)
{
    d->requiredApplicationIds = applicationIds;
}

void Signature::requireCapabilities(const QStringList &capabilities)
{
    d->requiredCapabilities = capabilities;
}

void Signature::requireCategories(const QStringList &categories)
{
    d->requiredCategories = categories;
}

void Signature::requireCertificateRoles(const QHash<QByteArray, CertificateRole> &trustedCertDataToRole)
{
    d->requiredRoles = trustedCertDataToRole;
}

void Signature::requireRevocationCheck(const QByteArrayList &crls)
{
    d->requiredCRLs = crls;
}

QByteArray Signature::create(const QByteArray &signingCertificatePkcs12,
                             const QByteArray &signingCertificatePassword)
{
    if (!d->requiredCRLs.isEmpty())
        qCWarning(LogCrypto) << "CRL checking is ignored as it does not work when creating signatures";
    if (!d->requiredRoles.isEmpty())
        qCWarning(LogCrypto) << "Certificate role requirements are ignored when creating signatures";

    // Although OpenSSL could, the macOS Security Framework (pre macOS 12) cannot
    // process empty detached data. So we better just not support it at all.
    if (d->hash.isEmpty())
        throw Exception("cannot sign an empty hash value");

    QByteArray sig = d->create(signingCertificatePkcs12, signingCertificatePassword,
                               [this](const Certificate &signer) { d->checkSignerCertificate(signer); });
    // very useful while debugging
    // static int counter = 0;
    // QFile f(QDir::home().absoluteFilePath(u"sig%1.der"_s.arg(++counter)));
    // if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    //     f.write(sig);
    return sig;
}

Signature::VerificationResult Signature::verify(const QByteArray &signaturePkcs7,
                                                const QByteArrayList &trustedCertsData) noexcept(false)
{
    VerificationResult result = d->verify(signaturePkcs7, trustedCertsData);
    d->checkSignerCertificate(result.signer());
    return result;
}

void SignaturePrivate::checkSignerCertificate(const Certificate &signer)
{
    if (!requirePackageId.isEmpty()) {
        if (!signer.matchPackageId(requirePackageId)) {
            throw Exception("Package ID mismatch: the certificate only allows '%1'; cannot sign '%2'")
                .arg(signer.packageIds())
                .arg(requirePackageId);
        }
    }

    if (!requiredApplicationIds.isEmpty()) {
        if (!signer.matchApplicationIds(requiredApplicationIds)) {
            throw Exception("Application ID mismatch: the certificate only allows '%1'; cannot sign '%2'")
                .arg(signer.applicationIds())
                .arg(requiredApplicationIds);
        }
    }

    if (!requiredCapabilities.isEmpty()) {
        if (!signer.matchCapabilities(requiredCapabilities)) {
            throw Exception("Capabilities mismatch: the certificate only allows '%1'; cannot sign '%2'")
                .arg(signer.capabilities())
                .arg(requiredCapabilities);
        }
    }

    if (!requiredCategories.isEmpty()) {
        if (!signer.matchCategories(requiredCategories)) {
            throw Exception("Categories mismatch: the certificate only allows '%1'; cannot sign '%2'")
                .arg(signer.categories())
                .arg(requiredCategories);
        }
    }

    if constexpr (!QT_CONFIG(am_legacy_certificates)) {
        if (requiredKeyUsages) {
            if (signer.keyUsages() != requiredKeyUsages) {
                throw Exception("Key usage mismatch on certificate: expected 0x%1, but got 0x%2")
                    .arg(requiredKeyUsages.toInt(), 3, 16, u'0')
                    .arg(signer.keyUsages().toInt(), 3, 16, u'0');
            }
        }
    }
}

void SignaturePrivate::setDNByOid(QVariantMap &map, const QString &oid, const QString &name)
{
    if (name.isEmpty())
        throw Exception("Found an empty distinguished name for %1").arg(oid);
    if (oid.isEmpty())
        throw Exception("Empty OID in distinguished name");

    // from https://oidref.com/2.5.4 reduced to the most common
    static const QHash<int, const char *> oid254Map = {
        {  2, "knowledgeInformation" },
        {  3, "commonName" },
        {  4, "surname" },
        {  5, "serialNumber" },
        {  6, "countryName" },
        {  7, "localityName" },
        {  8, "stateOrProvinceName" },
        {  9, "streetAddress" },
        { 10, "organizationName" },
        { 11, "organizationUnitName" },
        { 12, "title" },
        { 13, "description" },
        { 20, "telephoneNumber" },
        { 26, "registeredAddress" },
        { 29, "presentationAddress" },
        { 31, "member" },
        { 32, "owner" },
        { 41, "name" },
        { 42, "givenName" },
        { 45, "uniqueIdentifier" },
        { 46, "dnQualifier" },
        { 49, "distinguishedName" },
        { 73, "delegationPath" },
        { 97, "organizationIdentifier" }
    };

    QString oidName = oid;
    if (oid.startsWith(u"2.5.4.")) {
        bool ok;
        if (int num = QStringView(oid).slice(6).toInt(&ok); ok) {
            if (const char *name = oid254Map.value(num, nullptr))
                oidName = QString::fromLatin1(name);
        }
    }
    static const QSet<QString> onlyOneOid {
        u"commonName"_s,
        u"countryName"_s,
        u"localityName"_s,
        u"stateOrProvinceName"_s
    };

    if (auto it = map.find(oidName); it != map.end()) {
        if (onlyOneOid.contains(oidName))
            throw Exception("Found more than one distinguished name for %1").arg(oidName);
        *it = QString(it->toString() + u", " + name);
    } else {
        map.insert(oidName, name);
    }
}

void SignaturePrivate::verifyCertificateRole(const QString &subject, int position, int chainSize,
                                             Signature::CertificateRole role) noexcept(false)
{
    switch (role) {
    case Signature::CertificateRole::Any:
        break;
    case Signature::CertificateRole::Issuer:
        if (position != 1) {
            throw Exception("Certificate '%1' has role 'Issuer' but is not the second certificate in the chain")
                    .arg(subject);
        }
        break;
    case Signature::CertificateRole::Intermediate:
        if ((position <= 2) || (position == (chainSize - 1))) {
            throw Exception("Certificate '%1' has role 'Intermediate' but appears at position %2 in the verification chain")
                    .arg(subject).arg(position);
        }
        break;
    case Signature::CertificateRole::Root:
        if (position != (chainSize - 1)) {
            throw Exception("Certificate '%1' has role 'Root' but is not the last certificate in the chain")
                    .arg(subject);
        }
        break;
    }
}

QT_END_NAMESPACE_AM
