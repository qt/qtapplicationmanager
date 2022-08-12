/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QDir>
#include <QFile>
#include <QScopedPointer>
#include <QDataStream>

#include "packagedatabase.h"
#include "packageinfo.h"
#include "yamlpackagescanner.h"
#include "applicationinfo.h"
#include "installationreport.h"
#include "exception.h"
#include "logging.h"
#include "configcache.h"
#include "filesystemmountwatcher.h"

#include <cstdlib>


QT_BEGIN_NAMESPACE_AM

// the templated adaptor class needed to instantiate ConfigCache<PackageInfo> in parse() below
template<> class ConfigCacheAdaptor<PackageInfo>
{
public:
    PackageInfo *loadFromSource(QIODevice *source, const QString &fileName)
    {
        return YamlPackageScanner().scan(source, fileName);
    }
    PackageInfo *loadFromCache(QDataStream &ds)
    {
        return PackageInfo::readFromDataStream(ds);
    }
    void saveToCache(QDataStream &ds, const PackageInfo *pi)
    {
        pi->writeToDataStream(ds);
    }

    void preProcessSourceContent(QByteArray &, const QString &) { }
    void merge(PackageInfo *, const PackageInfo *) { }
};


PackageDatabase::PackageDatabase(const QStringList &builtInPackagesDirs,
                                 const QString &installedPackagesDir, const QString &installedPackagesMountPoint)
    : m_builtInPackagesDirs(builtInPackagesDirs)
    , m_installedPackagesDir(installedPackagesDir)
    , m_installedPackagesMountPoint(installedPackagesMountPoint)
{ }

PackageDatabase::PackageDatabase(const QString &singlePackagePath)
    : m_singlePackagePath(singlePackagePath)
{
    Q_ASSERT(!singlePackagePath.isEmpty());
}

PackageDatabase::~PackageDatabase()
{
    qDeleteAll(m_builtInPackages);
    qDeleteAll(m_installedPackages);
}

QString PackageDatabase::installedPackagesDir() const
{
    return m_installedPackagesDir;
}

void PackageDatabase::enableLoadFromCache()
{
    if (m_parsed)
        qCWarning(LogSystem) << "PackageDatabase cannot change the caching mode after the initial load";
    m_loadFromCache = true;
}

void PackageDatabase::enableSaveToCache()
{
    if (m_parsed)
        qCWarning(LogSystem) << "PackageDatabase cannot change the caching mode after the initial load";
    m_saveToCache = true;
}

bool PackageDatabase::builtInHasRemovableUpdate(PackageInfo *packageInfo) const
{
    if (!packageInfo || packageInfo->isBuiltIn() || !m_installedPackages.contains(packageInfo))
        return false;
    for (const auto *pi : m_builtInPackages) {
        if (pi->id() == packageInfo->id())
            return true;
    }
    return false;
}

QStringList PackageDatabase::findManifestsInDir(const QDir &manifestDir, bool scanningBuiltInApps)
{
    QStringList files;

    auto flags = scanningBuiltInApps ? QDir::Dirs | QDir::NoDotAndDotDot
                                     : QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;
    const QDir baseDir(manifestDir);
    const QStringList pkgDirNames = baseDir.entryList(flags);

    for (const QString &pkgDirName : pkgDirNames) {
        try {
            // ignore left-overs from the installer
            if (pkgDirName.endsWith(qL1C('+')) || pkgDirName.endsWith(qL1C('-')))
                continue;

            // ignore filesystem problems
            QDir pkgDir = baseDir.absoluteFilePath(pkgDirName);
            if (!pkgDir.exists())
                continue;

            // ignore directory names with weird/forbidden characters
            QString pkgIdError;
            if (!PackageInfo::isValidApplicationId(pkgDirName, &pkgIdError))
                throw Exception("not a valid package-id: %1").arg(pkgIdError);

            if (!pkgDir.exists(qSL("info.yaml")))
                throw Exception("couldn't find an info.yaml manifest");
            if (!scanningBuiltInApps && !pkgDir.exists(qSL(".installation-report.yaml")))
                throw Exception("found a non-built-in package without an installation report");

            QString manifestPath = pkgDir.absoluteFilePath(qSL("info.yaml"));
            files << manifestPath;

        } catch (const Exception &e) {
            qCDebug(LogSystem) << "Ignoring package" << pkgDirName << ":" << e.what();
        }
    }
    return files;
}

void PackageDatabase::parse(PackageLocations packageLocations)
{
    if (m_parsed)
        throw Exception("PackageDatabase::parse() has been called multiple times");
    m_parsed = true;

    if (!m_singlePackagePath.isEmpty()) {
        try {
            m_builtInPackages.append(PackageInfo::fromManifest(m_singlePackagePath));
        } catch (const Exception &e) {
            throw Exception("Failed to load manifest for package: %1").arg(e.errorString());
        }
        m_parsedPackageLocations = Builtin | Installed;
    } else {
        AbstractConfigCache::Options cacheOptions = AbstractConfigCache::IgnoreBroken;
        if (!m_loadFromCache)
            cacheOptions |= AbstractConfigCache::ClearCache;
        if (!m_loadFromCache && !m_saveToCache)
            cacheOptions |= AbstractConfigCache::NoCache;

        if ((packageLocations & Builtin) && !(m_parsedPackageLocations & Builtin)) {
            QStringList manifestFiles;
            for (const QString &dir : m_builtInPackagesDirs)
                manifestFiles << findManifestsInDir(dir, true);

            ConfigCache<PackageInfo> cache(manifestFiles, qSL("appdb-builtin"), "PKGB",
                                           PackageInfo::DataStreamVersion, cacheOptions);
            cache.parse();

            for (int i = 0; i < manifestFiles.size(); ++i) {
                QString manifestFile = manifestFiles.at(i);
                QDir pkgDir = QFileInfo(manifestFile).dir();
                QScopedPointer<PackageInfo> pkg(cache.takeResult(i));

                if (pkg.isNull()) { // the YAML file was not parseable and we ignore broken manifests
                    qCWarning(LogSystem) << "The file" << manifestFile << "is not a valid manifest YAML"
                                            " file and will be ignored.";
                    continue;
                }

                if (pkg->id() != pkgDir.dirName()) {
                    throw Exception("an info.yaml for packages must be in a directory that has"
                                    " the same name as the package's id: found '%1'").arg(pkg->id());
                }
                pkg->setBuiltIn(true);
                m_builtInPackages.append(pkg.take());
            }
            m_parsedPackageLocations |= Builtin;
        }
        if ((packageLocations & Installed) && !(m_parsedPackageLocations & Installed)) {
            if (m_installedPackagesDir.isEmpty()) {
                m_parsedPackageLocations |= Installed;
            } else {
                if (!m_installedPackagesMountPoint.isEmpty()) {
                    if (!m_installedPackagesMountWatcher) {
                        m_installedPackagesMountWatcher = new FileSystemMountWatcher(this);
                        connect(m_installedPackagesMountWatcher, &FileSystemMountWatcher::mountChanged,
                                this, [this](const QString &mountPoint, const QString &device) {
                            if (mountPoint == m_installedPackagesMountPoint && !device.isEmpty()) {
                                if (!(m_parsedPackageLocations & Installed)) {
                                    // we are not in main() anymore: we can't just throw

                                    try {
                                        parseInstalled();
                                    } catch (const Exception &e) {
                                        qCCritical(LogInstaller) << "Failed to parse the package meta-data after the device"
                                                                 << device << "was mounted onto" << mountPoint << ":"
                                                                 << e.what();
                                        std::abort(); // there is no qCFatal()
                                    }
                                    emit installedPackagesParsed();
                                }
                                m_installedPackagesMountWatcher->deleteLater();
                                m_installedPackagesMountWatcher = nullptr;
                            }
                        });
                        m_installedPackagesMountWatcher->addMountPoint(m_installedPackagesMountPoint);
                        if (m_installedPackagesMountWatcher->currentMountPoints().contains(m_installedPackagesMountPoint)) {
                            // we don't need the watcher, but we had to set it up to avoid a race condition
                            delete m_installedPackagesMountWatcher;
                            m_installedPackagesMountWatcher = nullptr;
                        }
                    }
                }

                // scan immediately, if we don't have to wait for the mountpoint
                if (!m_installedPackagesMountWatcher)
                    parseInstalled();
            }
        }
    }
}

PackageDatabase::PackageLocations PackageDatabase::parsedPackageLocations() const
{
    return m_parsedPackageLocations;
}

void PackageDatabase::parseInstalled()
{
    Q_ASSERT(m_parsed && !(m_parsedPackageLocations & Installed));

    QStringList manifestFiles = findManifestsInDir(m_installedPackagesDir, false);

    AbstractConfigCache::Options cacheOptions = AbstractConfigCache::IgnoreBroken;
    if (!m_loadFromCache)
        cacheOptions |= AbstractConfigCache::ClearCache;
    if (!m_loadFromCache && !m_saveToCache)
        cacheOptions |= AbstractConfigCache::NoCache;

    ConfigCache<PackageInfo> cache(manifestFiles, qSL("appdb-installed"), "PKGI",
                                   PackageInfo::DataStreamVersion, cacheOptions);
    cache.parse();

    for (int i = 0; i < manifestFiles.size(); ++i) {
        QString manifestFile = manifestFiles.at(i);
        QDir pkgDir = QFileInfo(manifestFile).dir();

        try {
            QScopedPointer<PackageInfo> pkg(cache.takeResult(i));

            if (pkg.isNull()) { // the YAML file was not parseable and we ignore broken manifests
                qCWarning(LogSystem) << "The file" << manifestFile << "is not a valid manifest YAML"
                                        " file and will be ignored.";
                continue;
            }

            if (pkg->id() != pkgDir.dirName()) {
                throw Exception("an info.yaml for packages must be in a directory that has"
                                " the same name as the package's id: found '%1'").arg(pkg->id());
            }

            QFile f(pkgDir.absoluteFilePath(qSL(".installation-report.yaml")));
            if (!f.open(QFile::ReadOnly))
                throw Exception(f, "failed to open the installation report");

            QScopedPointer<InstallationReport> report(new InstallationReport(pkg->id()));
            try {
                report->deserialize(&f);
            } catch (const Exception &e) {
                throw Exception("Failed to deserialize the installation report %1: %2")
                        .arg(f.fileName()).arg(e.errorString());
            }

            pkg->setInstallationReport(report.take());
            pkg->setBaseDir(pkgDir.path());
            m_installedPackages.append(pkg.take());

        } catch (const Exception &e) {
            qCWarning(LogInstaller) << "Ignoring broken package at" << pkgDir.absolutePath()
                                    << ":" << e.what();
        }
    }
    m_parsedPackageLocations |= Installed;
}

void PackageDatabase::addPackageInfo(PackageInfo *package)
{
    m_installedPackages.append(package);
}

void PackageDatabase::removePackageInfo(PackageInfo *package)
{
    if (m_installedPackages.removeAll(package))
        delete package;
}

QVector<PackageInfo *> PackageDatabase::installedPackages() const
{
    return m_installedPackages;
}

QVector<PackageInfo *> PackageDatabase::builtInPackages() const
{
    return m_builtInPackages;
}

QT_END_NAMESPACE_AM
