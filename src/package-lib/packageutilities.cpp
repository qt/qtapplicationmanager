// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only


#include <QFileInfo>
#include <QDataStream>
#include <QCryptographicHash>
#include <QByteArray>
#include <QString>

#include <archive.h>

#include "packageutilities.h"
#include "packageutilities_p.h"
#include "global.h"
#include "logging.h"

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM

//TODO: remove in 6.9
bool PackageUtilities::ensureCorrectLocale() { return true; }
bool PackageUtilities::checkCorrectLocale() { return true; }

ArchiveException::ArchiveException(struct ::archive *ar, const char *errorString)
    : Exception(Error::Archive, u"[libarchive] "_s + QString::fromLatin1(errorString) + u": "_s + QString::fromLocal8Bit(::archive_error_string(ar)))
{ }


QVariantMap PackageUtilities::headerDataForDigest = QVariantMap {
    { u"extraSigned"_s, QVariantMap() }
};

void PackageUtilities::addFileMetadataToDigest(const QString &entryFilePath, const QFileInfo &fi, QCryptographicHash &digest)
{
    // (using QDataStream would be more readable, but it would make the algorithm Qt dependent)
    QByteArray addToDigest = ((fi.isDir()) ? "D/" : "F/")
            + QByteArray::number(fi.isDir() ? 0 : fi.size())
            + '/' + entryFilePath.toUtf8();
    digest.addData(addToDigest);
}

void PackageUtilities::addHeaderDataToDigest(const QVariantMap &header, QCryptographicHash &digest) Q_DECL_NOEXCEPT_EXPR(false)
{
    for (auto it = headerDataForDigest.constBegin(); it != headerDataForDigest.constEnd(); ++it) {
        if (header.contains(it.key())) {
            QByteArray ba;
            QDataStream ds(&ba, QDataStream::WriteOnly);

            QVariant v = header.value(it.key());
            if (!v.convert(it.value().metaType()))
                throw Exception(Error::Package, "metadata field %1 has invalid type for digest calculation (cannot convert %2 to %3)")
                    .arg(it.key()).arg(header.value(it.key()).metaType().name()).arg(it.value().metaType().name());
            ds << v;

            digest.addData(ba);
        }
    }
}

QT_END_NAMESPACE_AM
