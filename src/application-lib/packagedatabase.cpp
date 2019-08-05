/****************************************************************************
**
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

#include "packagedatabase.h"
#include "packageinfo.h"
#include "yamlpackagescanner.h"
#include "applicationinfo.h"
#include "installationreport.h"
#include "exception.h"
#include "logging.h"

QT_BEGIN_NAMESPACE_AM

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

bool PackageDatabase::loadFromCache()
{
    return false;
    //TODO: read cache file
}

void PackageDatabase::saveToCache()
{
    //TODO: write cache file
}

bool PackageDatabase::canBeRevertedToBuiltIn(PackageInfo *pi)
{
    if (!pi || pi->isBuiltIn() || !m_installedPackages.contains(pi))
        return false;
    for (auto it = m_builtInPackages.cbegin(); it != m_builtInPackages.cend(); ++it) {
        if (it.key()->id() == pi->id())
            return true;
    }
    return false;
}


QMap<PackageInfo *, QString> PackageDatabase::loadManifestsFromDir(YamlPackageScanner *yps, const QString &manifestDir, bool scanningBuiltInApps)
{
    QMap<PackageInfo *, QString> result;

    auto flags = scanningBuiltInApps ? QDir::Dirs | QDir::NoDotAndDotDot
                                     : QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;
    const QDir baseDir(manifestDir);
    const QStringList pkgDirNames = baseDir.entryList(flags);

    for (const QString &pkgDirName : pkgDirNames) {
        try {
            // ignore left-overs from the installer
            if (pkgDirName.endsWith('+') || pkgDirName.endsWith('-'))
                continue;

            // ignore filesystem problems
            QDir pkgDir = baseDir.absoluteFilePath(pkgDirName);
            if (!pkgDir.exists())
                continue;

            // ignore directory names with weird/forbidden characters
            QString pkgIdError;
            if (!PackageInfo::isValidApplicationId(pkgDirName, &pkgIdError))
                throw Exception("directory name is not a valid package-id: %1").arg(pkgIdError);

            if (!pkgDir.exists(qSL("info.yaml")))
                throw Exception("couldn't find an info.yaml manifest");
            if (!scanningBuiltInApps && !pkgDir.exists(qSL(".installation-report.yaml")))
                throw Exception("found a non-built-in package without an installation report");

            QString manifestPath = pkgDir.absoluteFilePath(yps->metaDataFileName());
            QScopedPointer<PackageInfo> pkg(loadManifest(yps, manifestPath));

            if (pkg->id() != pkgDir.dirName()) {
                throw Exception("an info.yaml must be in a directory that has"
                                " the same name as the package's id: found '%1'").arg(pkg->id());
            }
            if (scanningBuiltInApps) {
                pkg->setBuiltIn(true);
            } else { // 3rd-party apps
                QFile f(pkgDir.absoluteFilePath(qSL(".installation-report.yaml")));
                if (!f.open(QFile::ReadOnly))
                    throw Exception(f, "failed to open the installation report");

                QScopedPointer<InstallationReport> report(new InstallationReport(pkg->id()));
                if (!report->deserialize(&f))
                    throw Exception(f, "failed to deserialize the installation report");

                pkg->setInstallationReport(report.take());
                pkg->setBaseDir(QDir(m_installedPackagesDir).filePath(pkg->id()));
            }
            result.insert(pkg.take(), manifestPath);
        } catch (const Exception &e) {
            qCDebug(LogSystem) << "Ignoring package" << pkgDirName << ":" << e.what();
        }
    }
    return result;
}

PackageInfo *PackageDatabase::loadManifest(YamlPackageScanner *yps, const QString &manifestPath)
{
    QScopedPointer<PackageInfo> pkg(yps->scan(manifestPath));
    Q_ASSERT(pkg);

    if (pkg->applications().isEmpty())
        throw Exception("package contains no applications");

    return pkg.take();
}

void PackageDatabase::parse()
{
    if (m_parsed)
        throw Exception("PackageDatabase::parse() has been called multiple times");
    m_parsed = true;

    if (m_loadFromCache) {
        if (loadFromCache())
            return;
    }

    YamlPackageScanner yps;

    if (!m_singlePackagePath.isEmpty()) {
        try {
            m_builtInPackages.insert(loadManifest(&yps, m_singlePackagePath), m_singlePackagePath);
        } catch (const Exception &e) {
            throw Exception("Failed to load manifest for package: %1").arg(e.errorString());
        }
    } else {
        for (const QString &dir : m_builtInPackagesDirs)
            m_builtInPackages.unite(loadManifestsFromDir(&yps, dir, true));

        if (!m_installedPackagesDir.isEmpty())
            m_installedPackages = loadManifestsFromDir(&yps, m_installedPackagesDir, false);
    }

    if (m_saveToCache)
        saveToCache();
}

QVector<PackageInfo *> PackageDatabase::installedPackages() const
{
    return m_installedPackages.keys().toVector();
}

QVector<PackageInfo *> PackageDatabase::builtInPackages() const
{
    return m_builtInPackages.keys().toVector();
}

QT_END_NAMESPACE_AM
