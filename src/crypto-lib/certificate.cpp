// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QRegularExpression>
#include <QUrl>

#include "certificate.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

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
