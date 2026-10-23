// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
#ifndef DEVELOPMENTTOKEN_H
#define DEVELOPMENTTOKEN_H

#include <QtCore/QByteArray>
#include <QtAppManPackage/qtappmanpackageglobal.h>

QT_BEGIN_NAMESPACE_AM

// A development mode token is a signature over a fixed payload, created with a developer
// certificate.
namespace DevelopmentToken {

// the payload that gets signed
inline QByteArray payload() { return QByteArrayLiteral("developmentMode"); }

// Passed as the first (PKCS#12) argument of the setDeveloperCertificate D-Bus call to flag that the
// second argument is a token instead of the password. Real PKCS#12 data is DER and starts with 0x30.
inline QByteArray dbusMoniker() { return QByteArrayLiteral("am-dev-token-v1"); }

} // namespace DevelopmentToken

QT_END_NAMESPACE_AM

#endif // DEVELOPMENTTOKEN_H
