// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QRegularExpression>
#include <QUrl>

#include "certificate.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

/*!
    \qmlvaluetype Certificate
    \inqmlmodule QtApplicationManager.SystemUI
    \ingroup system-ui-non-instantiable
    \since 6.11
    \brief This class represents a digital certificate used for package signing.

    The Certificate class encapsulates information about a digital certificate, including its
    subject, serial number, key usages, validity period, fingerprints, and bound package-ids.

    While it does allow you to read-only access to the most important X509 certificate information,
    it does however \b not encapsulate the actual DER/ASN.1 encoded certificate blob.

    For more information about X509 certificates, see \c{RFC 5280}.

    \sa {Package Installation}
*/
/*! \qmlproperty bool Certificate::valid

    This property holds whether the certificate is valid.
*/
/*! \qmlproperty variant Certificate::subject

    Returns the subject of the certificate as a map of attribute names and values.
    The attribute names follow the standard OID naming conventions, e.g. \c {commonName} instead
    of \c{CN}.
*/
/*! \qmlproperty string Certificate::serialNumber

    The serial number of the certificate as hex-encoded string.
*/
/*! \qmlproperty Certificate.KeyUsages Certificate::keyUsages

    This property is an integer value representing the key usage of the certificate. The possible
    key usage flags are defined as enum and they follow the bit values given in RFC 5280:
    \list
        \li Certificate.DigitalSignature
        \li Certificate.NonRepudiation
        \li Certificate.KeyEncipherment
        \li Certificate.DataEncipherment
        \li Certificate.KeyAgreement
        \li Certificate.KeyCertSign
        \li Certificate.CRLSign
        \li Certificate.EncipherOnly
        \li Certificate.DecipherOnly
    \endlist

    There are also two predefined combinations for common use cases within the application manager:
    \list
        \li Certificate.Store: Intended for store certificates used to sign packages for
            distribution via an application store. It combines DigitalSignature, NonRepudiation,
            KeyEncipherment, and EncipherOnly.
        \li Certificate.Developer: Intended for developer certificates used to sign packages
            during development and testing. It combines DigitalSignature, NonRepudiation,
            KeyEncipherment, and DecipherOnly.
    \endlist

    You can use bitwise operations to check for specific key usage in the returned value.
*/
/*! \qmlproperty date Certificate::validityNotBefore

    The validity start date for this certificate.
*/
/*! \qmlproperty date Certificate::validityNotAfter

    The validity end date for this certificate.
*/
/*! \qmlproperty variant Certificate::fingerprints

    This property holds the fingerprints of the certificate as a map of hash algorithm names
    and their corresponding fingerprint values as hex-encoded strings:
    Currently, the supported hash algorithms names are:
    \list
        \li \c{SHA-1}
        \li \c{SHA-256}
    \endlist
*/
/*! \qmlproperty list<string> Certificate::subjectAlternativeNames

    The subject alternative names (SANs) of the certificate. This is the "raw" value from the X509
    certificate. The application manager post-processes these entries to extract bound package-ids.

    \sa packageIds
*/

/*! \qmlmethod list<string> Certificate::packageIds()

    This function returns the list of package-ids the certificate is bound to.
*/
QStringList Certificate::packageIds() const
{
    QStringList result;
    for (const auto &san : m_subjectAlternativeNames) {
        auto sanUrl = QUrl(san);
        if (!sanUrl.isValid() || (sanUrl.scheme() != u"qtam") || (sanUrl.host() != u"packageid"))
            continue;
        result << sanUrl.path().mid(1); // skip leading '/'
    }
    return result;
}

/*!
    \qmlmethod bool Certificate::matchPackageId(string packageId)

    Returns \c true if the given \a packageId matches any of the package-ids the certificate is
    bound to or \c false otherwise.
*/
bool Certificate::matchPackageId(const QString &packageId) const
{
    if constexpr (QT_CONFIG(am_legacy_certificates)) {
        if (m_subjectAlternativeNames.isEmpty())
            return true;
    }

    bool foundMatch = false;
    const auto certPackageIds = packageIds();

    for (const auto &certPackageId : certPackageIds) {
        if (certPackageId.contains(u'*')) { // wildcard match
            const auto re = QRegularExpression::fromWildcard(certPackageId);
            foundMatch = re.match(packageId).hasMatch();
        } else { // exact match
            foundMatch = (certPackageId == packageId);
        }
        if (foundMatch)
            break;
    }
    return foundMatch;
}

bool Certificate::operator==(const Certificate &other) const
{
    return (m_subject == other.m_subject)
        && (m_serialNumber == other.m_serialNumber)
        && (m_keyUsages == other.m_keyUsages)
        && (m_validityNotBefore == other.m_validityNotBefore)
        && (m_validityNotAfter == other.m_validityNotAfter)
        && (m_fingerprints == other.m_fingerprints)
        && (m_subjectAlternativeNames == other.m_subjectAlternativeNames);
}

bool Certificate::operator!=(const Certificate &other) const
{
    return !((*this) == other);
}

/*!
    \qmlmethod object Certificate::toVariant()

    Returns a QVariantMap representation of this Certificate object.
*/
QVariant Certificate::toVariant() const
{
    if (!isValid())
        return QVariant::fromValue(nullptr);

    return QVariantMap {
        { u"subject"_s, m_subject },
        { u"serialNumber"_s, m_serialNumber },
        { u"keyUsages"_s, m_keyUsages.toInt() },
        { u"validityNotBefore"_s, m_validityNotBefore },
        { u"validityNotAfter"_s, m_validityNotAfter },
        { u"fingerprints"_s, m_fingerprints },
        { u"subjectAlternativeNames"_s, m_subjectAlternativeNames }
    };
}

QT_END_NAMESPACE_AM
