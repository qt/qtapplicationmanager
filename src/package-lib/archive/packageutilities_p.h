// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PACKAGEUTILITIES_P_H
#define PACKAGEUTILITIES_P_H

#include <QtAppManPackage/qtappmanpackageglobal.h>
#include <QtAppManCommon/exception.h>
#include <QVariantMap>

struct archive;
QT_FORWARD_DECLARE_CLASS(QFileInfo)
QT_FORWARD_DECLARE_CLASS(QCryptographicHash)

QT_BEGIN_NAMESPACE_AM

namespace PackageUtilities
{
void addFileMetadataToDigest(const QString &entryFilePath, const QFileInfo &fi, QCryptographicHash &digest);
void addExtendedAttributeToDigest(QByteArrayView name, QByteArrayView value, QCryptographicHash &digest);
void addHeaderDataToDigest(const QVariantMap &header, QCryptographicHash &digest) noexcept(false);

// key == field name, value == type to choose correct hashing algorithm
extern const QVariantMap headerDataForDigest;
};

enum PackageEntryType {
    PackageEntry_Header,
    PackageEntry_File,
    PackageEntry_Dir,
    PackageEntry_Footer
};

class Q_APPMANPACKAGE_EXPORT ArchiveException : public Exception
{
    Q_DISABLE_COPY(ArchiveException)
public:
    ArchiveException(struct ::archive *ar, const char *errorString);
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // PACKAGEUTILITIES_P_H
