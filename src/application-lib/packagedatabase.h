/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
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

#pragma once

#include <QtAppManCommon/global.h>
#include <QVector>
#include <QString>

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
    ~PackageDatabase();

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
    Q_DISABLE_COPY(PackageDatabase)

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

