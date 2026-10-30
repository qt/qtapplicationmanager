// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PACKAGEUTILITIES_P_H
#define PACKAGEUTILITIES_P_H

#include <QtAppManPackage/qtappmanpackageglobal.h>
#include <QtAppManCommon/exception.h>
#include <QVariantMap>
#include <QCryptographicHash>

#include <map>

struct archive;

QT_BEGIN_NAMESPACE_AM

// Calculates the package digest. The package's header formatVersion selects the algorithm:
// 1 and 2 use the legacy algorithm (not injective, kept to verify existing packages), 3 uses the
// injective one. Usage per package: addHeader(), then for every entry beginEntry(), addXattr()*,
// addContent()*, endEntry(), finally result(). Version 3 doesn't depend on the entry order, the
// entries are sorted by path in result(); duplicate paths and xattr names throw.
class Q_APPMANPACKAGE_EXPORT PackageDigest
{
public:
    explicit PackageDigest(int formatVersion);

    int formatVersion() const;
    bool isLegacy() const;

    void addHeader(const QVariantMap &header) noexcept(false);

    void beginEntry(const QString &path, bool isDir, qint64 size) noexcept(false);
    void addXattr(QByteArrayView name, QByteArrayView value) noexcept(false);
    void addContent(QByteArrayView data);
    void endEntry(qint64 actualSize) noexcept(false);

    QByteArray result() noexcept(false);

private:
    struct Record
    {
        bool isDir;
        QString path;
        quint64 size;
        QByteArray xattrDigest;
        QByteArray contentDigest;
    };

    QCryptographicHash m_hash { QCryptographicHash::Sha256 };
    int m_formatVersion;

    bool m_inEntry = false;
    bool m_entryIsDir = false;
    QString m_entryPath;
    qint64 m_entrySize = 0;
    qint64 m_contentSize = 0;

    // v3 only
    std::map<QByteArray, QByteArray> m_entryXattrs;
    QCryptographicHash m_entryContent { QCryptographicHash::Sha256 };
    std::map<QByteArray, Record> m_records;   // key: UTF-8 path
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
