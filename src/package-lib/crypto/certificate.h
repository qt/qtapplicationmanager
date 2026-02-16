// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
#ifndef CERTIFICATE_H
#define CERTIFICATE_H

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QDateTime>
#include <QtAppManPackage/qtappmanpackageglobal.h>

QT_BEGIN_NAMESPACE_AM

class CertificateParser;

// This class is a simple data holder for the most important X509 certificate information.
// It does NOT encapsulate the actual DER/ASN1 encoded certificate blob.
// For more information on the fields, see RFC 5280.

class Q_APPMANPACKAGE_EXPORT Certificate
{
    Q_GADGET
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(QVariantMap subject READ subject CONSTANT)
    Q_PROPERTY(QString serialNumber READ serialNumber CONSTANT)
    Q_PROPERTY(Certificate::KeyUsages keyUsages READ keyUsages CONSTANT)
    Q_PROPERTY(QDateTime validityNotBefore READ validityNotBefore CONSTANT)
    Q_PROPERTY(QDateTime validityNotAfter READ validityNotAfter CONSTANT)
    Q_PROPERTY(QString fingerprint READ fingerprintAsString CONSTANT)
    Q_PROPERTY(QStringList subjectAlternativeNames READ subjectAlternativeNames CONSTANT)

public:
    Certificate() = default;

    enum KeyUsage : uint {
        // X509
        DigitalSignature = 0x001,
        NonRepudiation   = 0x002,
        KeyEncipherment  = 0x004,
        DataEncipherment = 0x008,
        KeyAgreement     = 0x010,
        KeyCertSign      = 0x020,
        CRLSign          = 0x040,
        EncipherOnly     = 0x080,
        DecipherOnly     = 0x100,

        // AppMan installer use
        Store            = DigitalSignature | NonRepudiation | KeyEncipherment | EncipherOnly,
        Developer        = DigitalSignature | NonRepudiation | KeyEncipherment | DecipherOnly,
    };
    Q_DECLARE_FLAGS(KeyUsages, KeyUsage)
    Q_FLAG(KeyUsages)

    bool isValid() const { return !m_subject.isEmpty(); }
    QVariantMap subject() const { return m_subject; }
    QString subjectAsString() const;
    QString serialNumber() const { return m_serialNumber; }
    Certificate::KeyUsages keyUsages() const { return m_keyUsages; }
    QDateTime validityNotBefore() const { return m_validityNotBefore; }
    QDateTime validityNotAfter() const { return m_validityNotAfter; }
    QByteArray fingerprint() const { return m_fingerprint; }
    QString fingerprintAsString() const { return QString::fromLatin1(m_fingerprint.toHex(':')); }
    QStringList subjectAlternativeNames() const { return m_subjectAlternativeNames; }

    bool operator==(const Certificate &other) const;
    bool operator!=(const Certificate &other) const;

    Q_INVOKABLE QStringList packageIds() const;
    Q_INVOKABLE bool matchPackageId(const QString &name) const;
    Q_INVOKABLE QVariant toVariant() const;

private:
    QVariantMap m_subject;
    QString m_serialNumber;
    Certificate::KeyUsages m_keyUsages;
    QDateTime m_validityNotBefore;
    QDateTime m_validityNotAfter;
    QByteArray m_fingerprint;
    QStringList m_subjectAlternativeNames;

    friend class CertificateParser;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Certificate::KeyUsages)

QT_END_NAMESPACE_AM

#endif // CERTIFICATE_H
