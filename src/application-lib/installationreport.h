// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QByteArray>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QIODevice)

QT_BEGIN_NAMESPACE_AM

class InstallationReport
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

    quint64 diskSpaceUsed() const;
    void setDiskSpaceUsed(quint64 diskSpaceUsed);

    QByteArray developerSignature() const;
    void setDeveloperSignature(const QByteArray &developerSignature);

    QByteArray storeSignature() const;
    void setStoreSignature(const QByteArray &storeSignature);

    QStringList files() const;
    void addFile(const QString &file);
    void addFiles(const QStringList &files);

    bool isValid() const;

    void deserialize(QIODevice *from);
    bool serialize(QIODevice *to) const;

private:
    QString m_packageId;
    QByteArray m_digest;
    quint64 m_diskSpaceUsed = 0;
    QStringList m_files;
    QByteArray m_developerSignature;
    QByteArray m_storeSignature;
    QVariantMap m_extraMetaData;
    QVariantMap m_extraSignedMetaData;
};

QT_END_NAMESPACE_AM
