/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/


#include <QFileInfo>
#include <QDataStream>
#include <QCryptographicHash>

#include <archive.h>

#include "package_p.h"

QT_BEGIN_NAMESPACE_AM

ArchiveException::ArchiveException(struct ::archive *ar, const char *errorString)
    : Exception(Error::Archive, qSL("[libarchive] ") + qL1S(errorString) + qSL(": ") + QString::fromLocal8Bit(::archive_error_string(ar)))
{ }


QVariantMap PackageUtilities::headerDataForDigest = QVariantMap {
    { "extraSigned", QVariantMap() }
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
            QDataStream ds(&ba, QIODevice::WriteOnly);

            QVariant v = header.value(it.key());
            if (!v.convert(it.value().type()))
                throw Exception(Error::Package, "metadata field %1 has invalid type for digest calculation (cannot convert %2 to %3)")
                    .arg(it.key()).arg(header.value(it.key()).type()).arg(it.value().type());
            ds << v;

            digest.addData(ba);
        }
    }
}

QT_END_NAMESPACE_AM
