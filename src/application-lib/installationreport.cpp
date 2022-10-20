/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QIODevice>
#include <QVariant>
#include <QDataStream>
#include <QDebug>
#include <QMessageAuthenticationCode>

#include <exception>

#include "global.h"
#include "qtyaml.h"
#include "packageinfo.h"
#include "utilities.h"
#include "exception.h"
#include "installationreport.h"

QT_BEGIN_NAMESPACE_AM

// you can generate a new set with
//   xxd -i <(dd if=/dev/urandom bs=64 count=1)
static const unsigned char privateHmacKeyData[64] = {
    0xd8, 0xde, 0x41, 0x25, 0xee, 0x24, 0xd0, 0x19, 0xa2, 0x43, 0x06, 0x22,
    0x30, 0xa4, 0x87, 0xf0, 0x12, 0x07, 0xe9, 0xd3, 0x1c, 0xd4, 0x6f, 0xd6,
    0x1c, 0xc5, 0x38, 0x22, 0x2d, 0x7a, 0xe9, 0x90, 0x1e, 0xdf, 0xc8, 0x85,
    0x86, 0x96, 0xc4, 0x64, 0xc5, 0x59, 0xee, 0xc4, 0x69, 0xb6, 0x0f, 0x94,
    0x5c, 0xb0, 0x2a, 0xf0, 0xf1, 0xc0, 0x8a, 0x7a, 0xf0, 0xf6, 0x3f, 0x17,
    0xe6, 0xab, 0x2e, 0xc7
};


InstallationReport::InstallationReport(const QString &packageId)
    : m_packageId(packageId)
{ }

QString InstallationReport::packageId() const
{
    return m_packageId;
}

void InstallationReport::setPackageId(const QString &packageId)
{
    m_packageId = packageId;
}

QVariantMap InstallationReport::extraMetaData() const
{
    return m_extraMetaData;
}

void InstallationReport::setExtraMetaData(const QVariantMap &extraMetaData)
{
    m_extraMetaData = extraMetaData;
}

QVariantMap InstallationReport::extraSignedMetaData() const
{
    return m_extraSignedMetaData;
}

void InstallationReport::setExtraSignedMetaData(const QVariantMap &extraSignedMetaData)
{
    m_extraSignedMetaData = extraSignedMetaData;
}

QByteArray InstallationReport::digest() const
{
    return m_digest;
}

void InstallationReport::setDigest(const QByteArray &digest)
{
    m_digest = digest;
}

quint64 InstallationReport::diskSpaceUsed() const
{
    return m_diskSpaceUsed;
}

void InstallationReport::setDiskSpaceUsed(quint64 diskSpaceUsed)
{
    m_diskSpaceUsed = diskSpaceUsed;
}

QByteArray InstallationReport::developerSignature() const
{
    return m_developerSignature;
}

void InstallationReport::setDeveloperSignature(const QByteArray &developerSignature)
{
    m_developerSignature = developerSignature;
}

QByteArray InstallationReport::storeSignature() const
{
    return m_storeSignature;
}

void InstallationReport::setStoreSignature(const QByteArray &storeSignature)
{
    m_storeSignature = storeSignature;
}

QStringList InstallationReport::files() const
{
    return m_files;
}

void InstallationReport::addFile(const QString &file)
{
    m_files << file.normalized(QString::NormalizationForm_C);
}

void InstallationReport::addFiles(const QStringList &files)
{
    m_files << files;
}

bool InstallationReport::isValid() const
{
    return PackageInfo::isValidApplicationId(m_packageId) && !m_digest.isEmpty() && !m_files.isEmpty();
}

void InstallationReport::deserialize(QIODevice *from)
{
    if (!from || !from->isReadable() || (from->size() > 2*1024*1024))
        throw Exception("Installation report is invalid");

    m_digest.clear();
    m_files.clear();

    auto docs = YamlParser::parseAllDocuments(from->readAll());
    checkYamlFormat(docs, 3 /*number of expected docs*/, { { qSL("am-installation-report"), 3 } });

    const QVariantMap &root = docs.at(1).toMap();

    try {
        if (m_packageId.isEmpty()) {
            m_packageId = root[qSL("packageId")].toString();
            if (m_packageId.isEmpty())
                throw Exception("packageId is empty");
        } else if (root[qSL("packageId")].toString() != m_packageId) {
            throw Exception("packageId does not match: expected '%1', but got '%2'")
                    .arg(m_packageId).arg(root[qSL("packageId")].toString());
        }

        m_diskSpaceUsed = root[qSL("diskSpaceUsed")].toULongLong();
        m_digest = QByteArray::fromHex(root[qSL("digest")].toString().toLatin1());
        if (m_digest.isEmpty())
            throw Exception("digest is empty");

        auto devSig = root.find(qSL("developerSignature"));
        if (devSig != root.end()) {
            m_developerSignature = QByteArray::fromBase64(devSig.value().toString().toLatin1());
            if (m_developerSignature.isEmpty())
                throw Exception("developerSignature is empty");
        }
        auto storeSig = root.find(qSL("storeSignature"));
        if (storeSig != root.end()) {
            m_storeSignature = QByteArray::fromBase64(storeSig.value().toString().toLatin1());
            if (m_storeSignature.isEmpty())
                throw Exception("storeSignature is empty");
        }
        auto extra = root.find(qSL("extra"));
        if (extra != root.end()) {
            m_extraMetaData = extra.value().toMap();
            if (m_extraMetaData.isEmpty())
                throw Exception("extra metadata is empty");
        }
        auto extraSigned = root.find(qSL("extraSigned"));
        if (extraSigned != root.end()) {
            m_extraSignedMetaData = extraSigned.value().toMap();
            if (m_extraSignedMetaData.isEmpty())
                throw Exception("extraSigned metadata is empty");
        }
        m_files = root[qSL("files")].toStringList();
        if (m_files.isEmpty())
            throw Exception("No files");

        // see if the file has been tampered with by checking the hmac
        QByteArray hmacFile = QByteArray::fromHex(docs[2].toMap().value(qSL("hmac")).toString().toLatin1());
        QByteArray hmacKey = QByteArray::fromRawData(reinterpret_cast<const char *>(privateHmacKeyData),
                                                     sizeof(privateHmacKeyData));

        QByteArray out = QtYaml::yamlFromVariantDocuments({ docs[0], docs[1] }, QtYaml::BlockStyle);
        QByteArray hmacCalc= QMessageAuthenticationCode::hash(out, hmacKey, QCryptographicHash::Sha256);

        if (hmacFile != hmacCalc) {
            throw Exception("HMAC does not match: expected '%1', but got '%2'")
                .arg(hmacCalc.toHex()).arg(hmacFile.toHex());
        }
    } catch (const Exception &) {
        m_digest.clear();
        m_diskSpaceUsed = 0;
        m_files.clear();

        throw;
    }
}

bool InstallationReport::serialize(QIODevice *to) const
{
    if (!isValid() || !to || !to->isWritable())
        return false;

    QVariantMap header {
        { qSL("formatVersion"), 3 },
        { qSL("formatType"), qSL("am-installation-report") }
    };
    QVariantMap root {
        { qSL("packageId"), packageId() },
        { qSL("diskSpaceUsed"), diskSpaceUsed() },
        { qSL("digest"), QLatin1String(digest().toHex()) }
    };
    if (!m_developerSignature.isEmpty())
        root[qSL("developerSignature")] = QLatin1String(m_developerSignature.toBase64());
    if (!m_storeSignature.isEmpty())
        root[qSL("storeSignature")] = QLatin1String(m_storeSignature.toBase64());
    if (!m_extraMetaData.isEmpty())
        root[qSL("extra")] = m_extraMetaData;
    if (!m_extraSignedMetaData.isEmpty())
        root[qSL("extraSigned")] = m_extraSignedMetaData;

    root[qSL("files")] = files();

    QVector<QVariant> docs;
    docs << header;
    docs << root;

    // generate hmac to prevent tampering
    QByteArray hmacKey = QByteArray::fromRawData(reinterpret_cast<const char *>(privateHmacKeyData),
                                                 sizeof(privateHmacKeyData));
    QByteArray out = QtYaml::yamlFromVariantDocuments({ docs[0], docs[1] }, QtYaml::BlockStyle);
    QByteArray hmacCalc= QMessageAuthenticationCode::hash(out, hmacKey, QCryptographicHash::Sha256);

    // add another YAML document with a single key/value (way faster than using QtYaml)
    out += "---\nhmac: '";
    out += hmacCalc.toHex();
    out += "'\n";

    return (to->write(out) == out.size());
}

QT_END_NAMESPACE_AM
