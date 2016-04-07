/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QIODevice>
#include <QVariant>
#include <QDataStream>
#include <QDebug>

#include <exception>

#include "global.h"
#include "qtyaml.h"
#include "utilities.h"
#include "digestfilter.h"
#include "installationreport.h"

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


InstallationReport::InstallationReport(const QString &applicationId)
    : m_applicationId(applicationId)
{ }

QString InstallationReport::applicationId() const
{
    return m_applicationId;
}

void InstallationReport::setApplicationId(const QString &applicationId)
{
    m_applicationId = applicationId;
}

QString InstallationReport::installationLocationId() const
{
    return m_installationLocationId;
}

void InstallationReport::setInstallationLocationId(const QString &installationLocationId)
{
    m_installationLocationId = installationLocationId;
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
    m_files << file;
}

void InstallationReport::addFiles(const QStringList &files)
{
    m_files << files;
}

bool InstallationReport::isValid() const
{
    return isValidDnsName(m_applicationId) && !m_digest.isEmpty() && !m_files.isEmpty();
}

bool InstallationReport::deserialize(QIODevice *from)
{
    if (!from || !from->isReadable() || (from->size() > 2*1024*1024))
        return false;

    m_digest.clear();
    m_files.clear();

    QtYaml::ParseError error;
    QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(from->readAll(), &error);

    if (error.error != QJsonParseError::NoError)
        return false;

    if ((docs.size() != 3)
            || (docs.first().toMap().value(qSL("formatType")).toString() != qL1S("am-installation-report"))
            || (docs.first().toMap().value(qSL("formatVersion")).toInt(0) != 1)) {
        return false;
    }

    const QVariantMap &root = docs.at(1).toMap();

    try {
        if (m_applicationId.isEmpty()) {
            m_applicationId = root[qSL("applicationId")].toString();
            if (m_applicationId.isEmpty())
                throw false;
        } else if (root[qSL("applicationId")].toString() != m_applicationId) {
            throw false;
        }

        m_installationLocationId = root[qSL("installationLocationId")].toString();
        m_diskSpaceUsed = root[qSL("diskSpaceUsed")].toULongLong();
        m_digest = QByteArray::fromHex(root[qSL("digest")].toString().toLatin1());
        if (m_digest.isEmpty())
            throw false;

        auto devSig = root.find(qSL("developerSignature"));
        if (devSig != root.end()) {
            m_developerSignature = QByteArray::fromBase64(devSig.value().toString().toLatin1());
            if (m_developerSignature.isEmpty())
                throw false;
        }
        auto storeSig = root.find(qSL("storeSignature"));
        if (storeSig != root.end()) {
            m_storeSignature = QByteArray::fromBase64(storeSig.value().toString().toLatin1());
            if (m_storeSignature.isEmpty())
                throw false;
        }
        m_files = root[qSL("files")].toStringList();
        if (m_files.isEmpty())
            throw false;

        // see if the file has been tampered with by checking the hmac
        QByteArray hmacFile = QByteArray::fromHex(docs[2].toMap().value(qSL("hmac")).toString().toLatin1());
        QByteArray hmacKey = QByteArray::fromRawData((const char *) privateHmacKeyData, sizeof(privateHmacKeyData));
        QByteArray hmacCalc= HMACFilter::hmac(HMACFilter::Sha256, hmacKey, QtYaml::yamlFromVariantDocuments({ docs[0], docs[1] }, QtYaml::BlockStyle));

        if (hmacFile != hmacCalc)
            throw false;

        return true;
    } catch (bool) {
        m_digest.clear();
        m_diskSpaceUsed = 0;
        m_files.clear();

        return false;
    }
}

bool InstallationReport::serialize(QIODevice *to) const
{
    if (!isValid() || !to || !to->isWritable())
        return false;

    QVariantMap header {
        { "formatVersion", 1 },
        { "formatType", "am-installation-report" }
    };
    QVariantMap root {
        { qSL("applicationId"), applicationId() },
        { qSL("installationLocationId"), installationLocationId() },
        { qSL("diskSpaceUsed"), diskSpaceUsed() },
        { qSL("digest"), QLatin1String(digest().toHex()) }
    };
    if (!m_developerSignature.isEmpty())
        root[qSL("developerSignature")] = QLatin1String(m_developerSignature.toBase64());
    if (!m_storeSignature.isEmpty())
        root[qSL("storeSignature")] = QLatin1String(m_storeSignature.toBase64());

    root[qSL("files")] = files();

    QVector<QVariant> docs;
    docs << header;
    docs << root;

    // generate hmac to prevent tampering
    QByteArray hmacKey = QByteArray::fromRawData((const char *) privateHmacKeyData, sizeof(privateHmacKeyData));
    QByteArray hmacCalc= HMACFilter::hmac(HMACFilter::Sha256, hmacKey, QtYaml::yamlFromVariantDocuments({ docs[0], docs[1] }, QtYaml::BlockStyle));

    QVariantMap footer { { qSL("hmac"), QString::fromLatin1(hmacCalc.toHex()) } };
    docs << footer;

    QByteArray out = QtYaml::yamlFromVariantDocuments(docs, QtYaml::BlockStyle);

    return (to->write(out) == out.size());
}
