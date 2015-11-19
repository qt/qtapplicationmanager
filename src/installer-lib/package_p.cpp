/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/


#include <QFileInfo>
#include <QDataStream>

#include <archive.h>

#include "package_p.h"
#include "digestfilter.h"


ArchiveException::ArchiveException(struct ::archive *ar, const QString &errorString)
    : Exception(Error::Archive, "[libarchive] " + errorString + QLatin1String(": ") + QString::fromLocal8Bit(::archive_error_string(ar)))
{ }


QVariantMap PackageUtilities::importantHeaderData = QVariantMap {
};

void PackageUtilities::addFileMetadataToDigest(const QString &entryFilePath, const QFileInfo &fi, DigestFilter *digest)
{
    // (using QDataStream would be more readable, but it would make the algorithm Qt dependent)
    QByteArray addToDigest = ((fi.isDir()) ? "D/" : "F/")
            + QByteArray::number(fi.isDir() ? 0 : fi.size())
            + '/' + entryFilePath.toUtf8();
    digest->processData(addToDigest);
}

void PackageUtilities::addImportantHeaderDataToDigest(const QVariantMap &header, DigestFilter *digest) throw (Exception)
{
    for (auto it = importantHeaderData.constBegin(); it != importantHeaderData.constEnd(); ++it) {
        if (header.contains(it.key())) {
            QByteArray ba;
            QDataStream ds(&ba, QIODevice::WriteOnly);

            QVariant v = header.value(it.key());
            if (!v.convert(it.value().type()))
                throw Exception(Error::Package, "metadata field %1 has invalid type for digest calculation (cannot convert %2 to %3)")
                    .arg(it.key()).arg(header.value(it.key()).type()).arg(it.value().type());
            ds << v;

            digest->processData(ba);
        }
    }
}
