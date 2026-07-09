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
/*! \qmlproperty string Certificate::fingerprint

    This property holds the SHA-256 fingerprint of the certificate as hex-encoded string.
*/
/*! \qmlproperty list<string> Certificate::subjectAlternativeNames

    The subject alternative names (SANs) of the certificate. This is the "raw" value from the X509
    certificate. The application manager post-processes these entries to extract bound package-ids.

    \sa packageIds
*/

static QStringList sanValues(const QStringList &sans, QStringView host)
{
    QStringList result;
    for (const auto &san : sans) {
        const QUrl sanUrl(san);
        if (!sanUrl.isValid() || sanUrl.scheme() != u"qtam" || sanUrl.host() != host)
            continue;
        result << sanUrl.path().mid(1); // skip leading '/'
    }
    return result;
}

static bool sanValuesContainAll(const QStringList &sans, QStringView host, const QStringList &required)
{
    const QStringList certValues = sanValues(sans, host);
    for (const auto &value : required) {
        bool found = false;
        for (const auto &certValue : certValues) {
            if (certValue.contains(u'*')) // wildcard match
                found = QRegularExpression::fromWildcard(certValue).match(value).hasMatch();
            else // exact match
                found = (certValue == value);

            if (found)
                break;
        }
        if (!found)
            return false;
    }
    return true;
}

/*!
    \internal
    Returns the certificate "version", i.e. the AM version that introduced the set of restrictions
    this certificate carries. The version is derived from the certificate's contents, all of which
    are issued by the CA and cannot be forged by the holder:

      \list
      \li exactly one parseable \c{qtam://version/<x.y>} SAN  ->  that version (6.12 and later)
      \li no version SAN, but a valid AM key-usage            ->  6.11
      \li no (or an incorrect) AM key-usage                   ->  6.10 (legacy)
      \endlist

    A missing/incorrect key-usage is the reliable discriminator for pre-6.11 (legacy) certificates.

    A null version is returned for a malformed version SAN: more than one version SAN, or an
    unparseable one. No legitimate certificate carries such a SAN, so callers must reject it.
*/
QVersionNumber Certificate::version() const
{
    const QStringList versions = sanValues(m_subjectAlternativeNames, u"version");
    if (!versions.isEmpty()) {
        // a legitimate certificate carries exactly one version SAN
        if (versions.size() > 1)
            return QVersionNumber();
        return QVersionNumber::fromString(versions.constFirst()); // null if unparseable
    }
    if ((m_keyUsages == KeyUsages(KeyUsage::Store)) || (m_keyUsages == KeyUsages(KeyUsage::Developer)))
        return QVersionNumber(6, 11);
    return QVersionNumber(6, 10);
}

/*!
    \internal
    Returns the latest certificate policy version this build understands. Bump this \e only when
    the set of certificate restrictions actually changes - not on every AM release. It is the
    default value for the installer's minimum certificate version floor.
*/
QVersionNumber Certificate::currentVersion()
{
    return QVersionNumber(6, 12);
}

/*! \qmlmethod list<string> Certificate::packageIds()

    This function returns the list of package-ids the certificate is bound to.
*/
QStringList Certificate::packageIds() const
{
    return sanValues(m_subjectAlternativeNames, u"packageid");
}

/*!
    \qmlmethod bool Certificate::matchPackageId(string packageId)

    Returns \c true if the given \a packageId matches any of the package-ids the certificate is
    bound to or \c false otherwise.

    \note Package-id entries containing \c{*} are matched as shell-style wildcards.
*/
bool Certificate::matchPackageId(const QString &packageId) const
{
    if (isValid() && (version() < QVersionNumber(6, 11))) // legacy certs carry no restrictions at all
        return true;
    return sanValuesContainAll(m_subjectAlternativeNames, u"packageid", { packageId });
}

/*! \qmlmethod list<string> Certificate::applicationIds()

    This function returns the list of application-ids the certificate is bound to.
*/
QStringList Certificate::applicationIds() const
{
    return sanValues(m_subjectAlternativeNames, u"applicationid");
}

/*!
    \qmlmethod bool Certificate::matchApplicationIds(list<string> applicationIds)

    Returns \c true if all the given \a applicationIds match the application-ids the certificate is
    bound to or \c false otherwise.

    \note Application-id entries containing \c{*} are matched as shell-style wildcards.
*/
bool Certificate::matchApplicationIds(const QStringList &applicationIds) const
{
    if (isValid() && (version() < QVersionNumber(6, 12))) // application-id restrictions were introduced in 6.12
        return true;
    return sanValuesContainAll(m_subjectAlternativeNames, u"applicationid", applicationIds);
}

/*! \qmlmethod list<string> Certificate::capabilities()

    This function returns the list of capabilities the certificate is bound to.
*/
QStringList Certificate::capabilities() const
{
    return sanValues(m_subjectAlternativeNames, u"capability");
}

/*!
    \qmlmethod bool Certificate::matchCapabilities(list<string> capabilities)

    Returns \c true if all the given \a capabilities match the capabilities the certificate is
    bound to or \c false otherwise.

    \note Capability entries containing \c{*} are matched as shell-style wildcards.
*/
bool Certificate::matchCapabilities(const QStringList &capabilities) const
{
    if (isValid() && (version() < QVersionNumber(6, 12))) // capability restrictions were introduced in 6.12
        return true;
    return sanValuesContainAll(m_subjectAlternativeNames, u"capability", capabilities);
}

/*! \qmlmethod list<string> Certificate::categories()

    This function returns the list of categories the certificate is bound to.
*/
QStringList Certificate::categories() const
{
    return sanValues(m_subjectAlternativeNames, u"category");
}

/*!
    \qmlmethod bool Certificate::matchCategories(list<string> categories)

    Returns \c true if all the given \a categories match the categories the certificate is
    bound to or \c false otherwise.

    \note Category entries containing \c{*} are matched as shell-style wildcards.
*/
bool Certificate::matchCategories(const QStringList &categories) const
{
    if (isValid() && (version() < QVersionNumber(6, 12))) // category restrictions were introduced in 6.12
        return true;
    return sanValuesContainAll(m_subjectAlternativeNames, u"category", categories);
}

/*! \qmlmethod list<string> Certificate::runtimes()

    This function returns the list of runtimes the certificate is bound to.
*/
QStringList Certificate::runtimes() const
{
    return sanValues(m_subjectAlternativeNames, u"runtime");
}

/*!
    \qmlmethod bool Certificate::matchRuntimes(list<string> runtimes)

    Returns \c true if all the given \a runtimes match the runtimes the certificate is
    bound to or \c false otherwise.

    \note Runtime entries containing \c{*} are matched as shell-style wildcards.
*/
bool Certificate::matchRuntimes(const QStringList &runtimes) const
{
    if (isValid() && (version() < QVersionNumber(6, 12))) // runtime restrictions were introduced in 6.12
        return true;
    return sanValuesContainAll(m_subjectAlternativeNames, u"runtime", runtimes);
}

QString Certificate::subjectAsString() const
{
    QString subject;
    for (const auto &s : m_subject.asKeyValueRange()) {
        if (!subject.isEmpty())
            subject += u", ";
        subject += u"%1=%2"_s.arg(s.first, s.second.toString());
    }
    return subject;
}

bool Certificate::operator==(const Certificate &other) const
{
    return (m_subject == other.m_subject)
        && (m_serialNumber == other.m_serialNumber)
        && (m_keyUsages == other.m_keyUsages)
        && (m_validityNotBefore == other.m_validityNotBefore)
        && (m_validityNotAfter == other.m_validityNotAfter)
        && (m_fingerprint == other.m_fingerprint)
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
        { u"fingerprint"_s, fingerprintAsString() },
        { u"subjectAlternativeNames"_s, m_subjectAlternativeNames }
    };
}

QT_END_NAMESPACE_AM
