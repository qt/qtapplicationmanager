// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef INSTALLATIONREPORT_H
#define INSTALLATIONREPORT_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QByteArray>
#include <QtCore/QVariantMap>
#include <QtAppManPackage/qtappmanpackageglobal.h>

QT_FORWARD_DECLARE_CLASS(QIODevice)

QT_BEGIN_NAMESPACE_AM

class Q_APPMANPACKAGE_EXPORT InstallationReport
{
public:
    InstallationReport(const QString &packageId = QString());

    QString packageId() const;
    void setPackageId(const QString &packageId);

    QVariantMap extraMetaData() const;
    void setExtraMetaData(const QVariantMap &extraMetaData);
    QVariantMap extraSignedMetaData() const;
    void setExtraSignedMetaData(const QVariantMap &extraSignedMetaData);

    QByteArray digest() const;
    void setDigest(const QByteArray &sha1);

    QByteArray manifestDigest() const;
    void setManifestDigest(const QByteArray &manifestDigest);

    quint64 diskSpaceUsed() const;
    void setDiskSpaceUsed(quint64 diskSpaceUsed);

    QByteArray developerSignature() const;
    void setDeveloperSignature(const QByteArray &developerSignature);

    QByteArray storeSignature() const;
    void setStoreSignature(const QByteArray &storeSignature);

    bool includeExtendedAttributes() const;
    void setIncludeExtendedAttributes(bool b);

    // the package header's formatVersion: this selects the digest algorithm; not serialized
    static constexpr int LatestPackageFormatVersion = 3;
#if QT_VERSION >= QT_VERSION_CHECK(6, 13, 0)
    static constexpr int DefaultPackageFormatVersion = LatestPackageFormatVersion;
    static constexpr int DefaultMinimumPackageFormatVersion = LatestPackageFormatVersion;
#else
    // The 6.12 LTS releases need to keep creating and accepting packages that older application
    // managers can handle, as the version 3 digest was introduced only in the 6.12.1 release.
    static constexpr int DefaultPackageFormatVersion = 2;
    static constexpr int DefaultMinimumPackageFormatVersion = 1;
#endif
    int packageFormatVersion() const;
    void setPackageFormatVersion(int version);

    QStringList files() const;
    void addFile(const QString &file);
    void addFiles(const QStringList &files);

    bool isValid() const;

    void deserialize(QIODevice *from);
    bool serialize(QIODevice *to) const;

private:
    QString m_packageId;
    QByteArray m_digest;
    QByteArray m_manifestDigest;
    quint64 m_diskSpaceUsed = 0;
    QStringList m_files;
    QByteArray m_developerSignature;
    QByteArray m_storeSignature;
    QVariantMap m_extraMetaData;
    QVariantMap m_extraSignedMetaData;
    bool m_includeExtendedAttributes = false;
    int m_packageFormatVersion = DefaultPackageFormatVersion;
};

QT_END_NAMESPACE_AM

#endif // INSTALLATIONREPORT_H
