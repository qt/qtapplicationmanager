// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only


#include <QDataStream>
#include <QCryptographicHash>
#include <QByteArray>
#include <QString>

#include <archive.h>

#include "packageutilities_p.h"
#include "installationreport.h"
#include "logging.h"

using namespace Qt::StringLiterals;


QT_BEGIN_NAMESPACE_AM

ArchiveException::ArchiveException(struct ::archive *ar, const char *errorString)
    : Exception(Error::Archive, u"[libarchive] "_s + QString::fromLatin1(errorString) + u": "_s + QString::fromLocal8Bit(::archive_error_string(ar)))
{ }


namespace {

// The legacy digest (formatVersion 1 and 2) is not injective (e.g. an absent extraSigned adds
// nothing, and the file descriptors and xattrs are not length-prefixed). It is only kept to be
// able to verify existing packages.


// The now current digest (formatVersion 3) is injective. Everything that goes into the hash is
// either a fixed-width field or has its length in front, so the byte stream can be parsed
// unambiguously from left to right. All fields are written with QDataStream (version Qt_6_7).
//  Layout:
//   stream  := TAG header entry*
//   TAG     := "AM\0PKG3\0"
//   header  := 'H' QVariant(QVariantMap { formatVersion, packageId, extendedAttributes, extraSigned })
//   entry   := 'E' type QString(path) quint64(size) ('X' bytes(name) bytes(value))* [ 'C' <size bytes> ]
//   type    := 'F' | 'D'        (the 'C' section is only present for files)
//   bytes   := quint32(length) <length bytes>       (QDataStream::writeBytes)
// 'H', 'E', 'X', 'C' and type are single quint8 values.

constexpr char v3Tag[8] = { 'A', 'M', '\0', 'P', 'K', 'G', '3', '\0' };

// Calls f with a QDataStream and adds whatever it wrote to the hash
template <typename F> void streamToHash(QCryptographicHash &hash, F &&f) noexcept(false)
{
    QByteArray ba;
    QDataStream ds(&ba, QDataStream::WriteOnly);
    ds.setVersion(QDataStream::Qt_6_7);
    f(ds);
    if (ds.status() != QDataStream::Ok)
        throw Exception("could not serialize data for the digest calculation");
    hash.addData(ba);
}

void writeBytes(QDataStream &ds, QByteArrayView data)
{
    ds.writeBytes(data.isEmpty() ? "" : data.data(), data.size());
}

} // namespace


PackageDigest::PackageDigest(int formatVersion)
    : m_formatVersion(formatVersion)
{
    Q_ASSERT((formatVersion >= 1) && (formatVersion <= InstallationReport::LatestPackageFormatVersion));

    if (!isLegacy())
        m_hash.addData(QByteArrayView(v3Tag, sizeof(v3Tag)));
}

int PackageDigest::formatVersion() const
{
    return m_formatVersion;
}

bool PackageDigest::isLegacy() const
{
    return m_formatVersion < 3;
}

void PackageDigest::addHeader(const QVariantMap &header) noexcept(false)
{
    if (isLegacy()) {
        static const QString extraSignedKey = u"extraSigned"_s;
        if (header.contains(extraSignedKey)) {
            QByteArray ba;
            QDataStream ds(&ba, QDataStream::WriteOnly);
            ds.setVersion(QDataStream::Qt_6_7);
            QVariant v = header.value(extraSignedKey);
            if (!v.convert(QMetaType::fromType<QVariantMap>())) {
                throw Exception("metadata field %1 has invalid type for digest calculation "
                                "(cannot convert %2 to %3)")
                    .arg(extraSignedKey)
                    .arg(header.value(extraSignedKey).metaType().name())
                    .arg(QMetaType::fromType<QVariantMap>().name());
            }
            ds << v;
            m_hash.addData(ba);
        }
        return;
    }

    // an absent extraSigned is the same as an empty one
    QVariant extraSigned = header.value(u"extraSigned"_s);
    if (!extraSigned.isValid() || (extraSigned.metaType() == QMetaType::fromType<std::nullptr_t>()))
        extraSigned = QVariantMap();
    else if (extraSigned.metaType() != QMetaType::fromType<QVariantMap>())
        throw Exception("metadata field extraSigned has invalid type for digest calculation (expected a map, got %1)")
            .arg(QLatin1StringView(extraSigned.metaType().name()));

    streamToHash(m_hash, [&](QDataStream &ds) {
        ds << quint8('H') << QVariant(QVariantMap {
            { u"formatVersion"_s, m_formatVersion },
            { u"packageId"_s, header.value(u"packageId"_s).toString() },
            { u"extendedAttributes"_s, header.value(u"extendedAttributes"_s).toBool() },
            { u"extraSigned"_s, extraSigned }
        });
    });
}

void PackageDigest::beginEntry(const QString &path, bool isDir, qint64 size) noexcept(false)
{
    Q_ASSERT(!m_inEntry);
    if (size < 0)
        throw Exception("invalid size for entry '%1'").arg(path);

    m_inEntry = true;
    m_contentStarted = false;
    m_entryIsDir = isDir;
    m_entryPath = path;
    m_entrySize = isDir ? 0 : size;
    m_contentSize = 0;

    if (!isLegacy()) {
        streamToHash(m_hash, [&](QDataStream &ds) {
            ds << quint8('E') << quint8(isDir ? 'D' : 'F') << path << quint64(m_entrySize);
        });
    }
}

void PackageDigest::addXattr(QByteArrayView name, QByteArrayView value)
{
    Q_ASSERT(m_inEntry && !m_contentStarted);

    if (isLegacy()) {
        m_hash.addData("XATTR/"_ba);
        m_hash.addData(name);
        m_hash.addData("/"_ba);
        m_hash.addData(value);
    } else {
        streamToHash(m_hash, [&](QDataStream &ds) {
            ds << quint8('X');
            writeBytes(ds, name);
            writeBytes(ds, value);
        });
    }
}

void PackageDigest::startContent()
{
    if (!m_contentStarted) {
        m_contentStarted = true;
        if (!isLegacy() && !m_entryIsDir)
            streamToHash(m_hash, [](QDataStream &ds) { ds << quint8('C'); });
    }
}

void PackageDigest::addContent(QByteArrayView data)
{
    Q_ASSERT(m_inEntry && !m_entryIsDir);

    startContent();
    m_contentSize += data.size();
    m_hash.addData(data);
}

void PackageDigest::endEntry(qint64 actualSize) noexcept(false)
{
    Q_ASSERT(m_inEntry);
    m_inEntry = false;

    if (isLegacy()) {
        // (using QDataStream would be more readable, but it would make the algorithm Qt dependent)
        const QByteArray descriptor = ((m_entryIsDir) ? "D/" : "F/")
                + QByteArray::number(m_entryIsDir ? 0 : actualSize)
                + '/' + m_entryPath.toUtf8();
        m_hash.addData(descriptor);
        return;
    }

    startContent();
    if (m_entryIsDir)
        actualSize = 0;
    if ((m_contentSize != m_entrySize) || (actualSize != m_entrySize)) {
        throw Exception("size mismatch for '%1': expected %2, got %3 (hashed %4)")
            .arg(m_entryPath).arg(m_entrySize).arg(actualSize).arg(m_contentSize);
    }
}

QByteArray PackageDigest::result()
{
    Q_ASSERT(!m_inEntry);
    return m_hash.result();
}

QT_END_NAMESPACE_AM
