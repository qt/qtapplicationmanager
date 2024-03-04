// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PACKAGEDATABASE_H
#define PACKAGEDATABASE_H

#include <QtAppManCommon/global.h>
#include <QtCore/QVector>
#include <QtCore/QString>

#include <QtAppManApplication/packageinfo.h>

QT_BEGIN_NAMESPACE_AM

class PackageInfo;
class FileSystemMountWatcher;


class PackageDatabase : public QObject
{
    Q_OBJECT

public:
    PackageDatabase(const QStringList &builtInPackagesDirs, const QString &installedPackagesDir = QString(),
                    const QString &installedPackagesMountPoint = QString());
    PackageDatabase(const QString &singlePackagePath);
    ~PackageDatabase() override;

    enum PackageLocation { None = 0x0, Builtin = 0x01, Installed = 0x02, All = 0x03 };
    Q_DECLARE_FLAGS(PackageLocations, PackageLocation)

    QString installedPackagesDir() const;

    void enableLoadFromCache();
    void enableSaveToCache();

    void parse(PackageLocations packageLocations = All);

    PackageLocations parsedPackageLocations() const;

    QVector<PackageInfo *> builtInPackages() const;
    QVector<PackageInfo *> installedPackages() const;

    // runtime installations
    void addPackageInfo(PackageInfo *package);
    void removePackageInfo(PackageInfo *package);

signals:
    void installedPackagesParsed();

private:
    Q_DISABLE_COPY_MOVE(PackageDatabase)

    bool builtInHasRemovableUpdate(PackageInfo *packageInfo) const;
    QStringList findManifestsInDir(const QDir &manifestDir, bool scanningBuiltInApps);
    void parseInstalled();

    bool m_loadFromCache = false;
    bool m_saveToCache = false;
    bool m_parsed = false;
    QStringList m_builtInPackagesDirs;
    QString m_installedPackagesDir;
    QString m_installedPackagesMountPoint;
    QString m_singlePackagePath;
    PackageLocations m_parsedPackageLocations = None;
    FileSystemMountWatcher *m_installedPackagesMountWatcher = nullptr;

    QVector<PackageInfo *> m_builtInPackages;
    QVector<PackageInfo *> m_installedPackages;

};

Q_DECLARE_OPERATORS_FOR_FLAGS(PackageDatabase::PackageLocations)

QT_END_NAMESPACE_AM


#endif // PACKAGEDATABASE_H
