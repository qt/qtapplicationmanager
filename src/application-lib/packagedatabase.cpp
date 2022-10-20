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
                                 const QString &installedPackagesDir)
    : m_builtInPackagesDirs(builtInPackagesDirs)
    , m_installedPackagesDir(installedPackagesDir)
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

void PackageDatabase::parse()
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
    } else {
        QStringList manifestFiles;

        // parallelize this
        for (const QString &dir : qAsConst(m_builtInPackagesDirs))
            manifestFiles << findManifestsInDir(dir, true);
        int installedOffset = manifestFiles.size();
        if (!m_installedPackagesDir.isEmpty())
            manifestFiles << findManifestsInDir(m_installedPackagesDir, false);

        AbstractConfigCache::Options cacheOptions = AbstractConfigCache::IgnoreBroken;
        if (!m_loadFromCache)
            cacheOptions |= AbstractConfigCache::ClearCache;
        if (!m_loadFromCache && !m_saveToCache)
            cacheOptions |= AbstractConfigCache::NoCache;

        ConfigCache<PackageInfo> cache(manifestFiles, qSL("appdb"), "MANI",
                                       PackageInfo::DataStreamVersion, cacheOptions);
        cache.parse();

        for (int i = 0; i < manifestFiles.size(); ++i) {
            bool isBuiltIn = (i < installedOffset);
            QString manifestFile = manifestFiles.at(i);
            QDir pkgDir = QFileInfo(manifestFile).dir();
            QScopedPointer<PackageInfo> pkg(cache.takeResult(i));

            if (pkg.isNull()) { // the YAML file was not parseable and we ignore broken manifests
                qCWarning(LogSystem) << "The file" << manifestFile << "is not a valid manifest YAML"
                                        " file and will be ignored.";
                continue;
            }

            if (pkg->id() != pkgDir.dirName()) {
                throw Exception("an info.yaml for built-in packages must be in a directory that has"
                                " the same name as the package's id: found '%1'").arg(pkg->id());
            }
            if (isBuiltIn) {
                pkg->setBuiltIn(true);
                m_builtInPackages.append(pkg.take());
            } else { // 3rd-party apps
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
            }
        }
    }
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
