// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:data-parser

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QBuffer>
#include <QStandardPaths>
#include <QtConcurrentMap>

#include "packagedatabase.h"
#include "packageinfo.h"
#include "yamlpackagescanner.h"
#include "installationreport.h"
#include "exception.h"
#include "logging.h"
#include "filesystemmountwatcher.h"
#include "sudo.h"

#include <memory>
#include <optional>
#include <vector>
#include <cstdlib>

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

// Parse a list of info.yaml manifests directly (no caching). Broken manifests are skipped and
// reported as a nullptr in the result, which stays index-aligned with manifestFiles. If
// expectedDigests is set, each file's raw content is verified against the recorded digest and a
// mismatch is treated like a broken manifest.
static std::vector<std::unique_ptr<PackageInfo>> parseManifests(const QStringList &manifestFiles,
                                                                const std::optional<QHash<QString, QByteArray>> &expectedDigests = std::nullopt)
{
    auto parseOne = [expectedDigests](const QString &manifestFile) -> PackageInfo * {
        try {
            QFile file(manifestFile);
            if (!file.open(QIODevice::ReadOnly))
                throw Exception("Failed to open file '%1' for reading").arg(manifestFile);
            if (file.size() > 1024*1024)
                throw Exception("File '%1' is too big (> 1MB)").arg(manifestFile);

            QByteArray rawContent = file.readAll();

            if (expectedDigests) {
                const QByteArray checksum = QCryptographicHash::hash(rawContent, QCryptographicHash::Sha256);
                const auto it = expectedDigests->constFind(manifestFile);
                if ((it == expectedDigests->cend()) || (checksum != it.value())) {
                    qCWarning(LogInstaller) << "Source content digest mismatch for" << manifestFile
                                            << "- the file will be ignored";
                    return nullptr;
                }
            }

            QBuffer buffer(&rawContent);
            buffer.open(QIODevice::ReadOnly);
            return YamlPackageScanner().scan(&buffer, manifestFile);
        } catch (const Exception &e) {
            qCWarning(LogSystem, "Could not parse file '%s': %s (file will be ignored)",
                      qPrintable(manifestFile), qPrintable(e.errorString()));
            return nullptr;
        }
    };

    // parse in parallel if there is more than one file
    QVector<PackageInfo *> parsed;
    if (manifestFiles.size() > 1)
        parsed = QtConcurrent::blockingMapped<QVector<PackageInfo *>>(manifestFiles, parseOne);
    else if (!manifestFiles.isEmpty())
        parsed << parseOne(manifestFiles.first());

    // adopt the parsed manifests into owning pointers for the caller
    std::vector<std::unique_ptr<PackageInfo>> result;
    result.reserve(parsed.size());
    for (PackageInfo *pi : std::as_const(parsed))
        result.emplace_back(pi);
    return result;
}


PackageDatabase::PackageDatabase(const QStringList &builtInPackagesDirs,
                                 const QString &installedPackagesDir, const QString &installedPackagesMountPoint)
    : m_builtInPackagesDirs(builtInPackagesDirs)
    , m_installedPackagesDir(installedPackagesDir)
    , m_installedPackagesMountPoint(installedPackagesMountPoint)
{
    qCDebug(LogInstaller) << "Loading built-in apps from:" << m_builtInPackagesDirs;
    qCDebug(LogInstaller) << "Loading installed apps from:" << m_installedPackagesDir;
}

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
            if (pkgDirName.endsWith(u'+') || pkgDirName.endsWith(u'-') || pkgDirName.startsWith(u".tmp-"_s))
                continue;

            // ignore filesystem problems
            QDir pkgDir = baseDir.absoluteFilePath(pkgDirName);
            if (!pkgDir.exists())
                continue;

            // ignore directory names with weird/forbidden characters
            QString pkgIdError;
            if (!PackageInfo::isValidApplicationId(pkgDirName, &pkgIdError))
                throw Exception("not a valid package-id: %1").arg(pkgIdError);

            if (!pkgDir.exists(u"info.yaml"_s))
                throw Exception("couldn't find an info.yaml manifest");
            QString manifestPath = pkgDir.absoluteFilePath(u"info.yaml"_s);
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
        if ((packageLocations & Builtin) && !(m_parsedPackageLocations & Builtin)) {
            QStringList manifestFiles;
            for (const QString &dir : std::as_const(m_builtInPackagesDirs))
                manifestFiles << findManifestsInDir(dir, true);

            std::vector<std::unique_ptr<PackageInfo>> pkgs = parseManifests(manifestFiles);

            for (int i = 0; i < manifestFiles.size(); ++i) {
                QString manifestFile = manifestFiles.at(i);
                QDir pkgDir = QFileInfo(manifestFile).dir();
                std::unique_ptr<PackageInfo> pkg = std::move(pkgs[i]);

                if (!pkg) { // the YAML file was not parseable and we ignore broken manifests
                    qCWarning(LogSystem) << "The file" << manifestFile << "is not a valid manifest YAML"
                                            " file and will be ignored.";
                    continue;
                }

                if (pkg->id() != pkgDir.dirName()) {
                    throw Exception("an info.yaml for packages must be in a directory that has"
                                    " the same name as the package's id: found id '%1' in directory '%2'")
                        .arg(pkg->id(), pkgDir.path());
                }
                pkg->setBuiltIn(true);
                m_builtInPackages.append(pkg.release());
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

    // Pre-pass: load every installation-report, then hand the expected digests to the ConfigCache
    // old v3 reports (both 6.11 and pre-6.11) are migrated on the fly here by hashing the on-disk
    // info.yaml and persisting a new v4 report through the sudo helper
    QStringList validManifests;
    QHash<QString, QByteArray> expectedDigests;
    std::vector<std::unique_ptr<InstallationReport>> validReports;

    for (const QString &manifestFile : std::as_const(manifestFiles)) {
        const QDir pkgDir = QFileInfo(manifestFile).dir();
        const QString pkgId = pkgDir.dirName();
        const QString reportRelPath = u"installation-reports/"_s + pkgId + u".yaml"_s;
        const QString reportPath611 = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + u'/' + reportRelPath;
        const QString reportPath610 = pkgDir.absoluteFilePath(u".installation-report.yaml"_s);

        auto report = std::make_unique<InstallationReport>(pkgId);
        bool isV3 = false;

        auto deserializeFrom = [&](QIODevice *dev, const char *kind) -> bool {
            try {
                report->deserialize(dev);
                return true;
            } catch (const Exception &e) {
                qCWarning(LogInstaller) << "Ignoring package at" << pkgDir.absolutePath()
                                        << ":" << kind << "installation report invalid:" << e.what();
                return false;
            }
        };

        try {
            auto rf = SudoClient::instance()->openTrustedFile(QStandardPaths::StateLocation, reportRelPath);
            if (!deserializeFrom(rf.get(), "6.12+"))
                continue;
        } catch (const Exception &) {
            if (QFile f(reportPath611); f.open(QFile::ReadOnly)) {
                if (!deserializeFrom(&f, "6.11"))
                    continue;
                isV3 = true;
            } else if (QFile f(reportPath610); f.open(QFile::ReadOnly)) {
                if (!deserializeFrom(&f, "pre-6.11"))
                    continue;
                isV3 = true;
            } else {
                qCWarning(LogInstaller) << "Ignoring package at" << pkgDir.absolutePath()
                                        << ": cannot open installation report";
                continue;
            }
        }

        if (report->manifestDigest().isEmpty() && !isV3) {
            qCCritical(LogInstaller) << "Refusing package at" << pkgDir.absolutePath()
                                     << ": installation report lacks manifestDigest";
            continue;
        }

        if (isV3) {
            // Hash on-disk info.yaml to bootstrap the v4 digest.
            QFile mf(manifestFile);
            if (!mf.open(QFile::ReadOnly)) {
                qCWarning(LogInstaller) << "Ignoring package at" << pkgDir.absolutePath()
                                        << ": cannot read info.yaml for v4 migration";
                continue;
            }
            report->setManifestDigest(QCryptographicHash::hash(mf.readAll(), QCryptographicHash::Sha256));

            try {
                auto out = SudoClient::instance()->openTrustedSaveFile(QStandardPaths::StateLocation,
                                                                       reportRelPath);
                if (!report->serialize(out.get()))
                    throw Exception("serialize failed");
                out->commit();

                qCInfo(LogInstaller) << "Migrated v3 installation-report to v4 for" << pkgId;

                // best effort cleanup of legacy files
                QFile::remove(reportPath610);
                QFile::remove(reportPath611);
            } catch (const Exception &e) {
                qCWarning(LogInstaller) << "Failed to persist migrated report for" << pkgId
                                        << "-" << e.errorString() << "- will retry on next start";
            }
        }

        validManifests << manifestFile;
        expectedDigests.insert(manifestFile, report->manifestDigest());
        validReports.push_back(std::move(report));
    }

    std::vector<std::unique_ptr<PackageInfo>> pkgs = parseManifests(validManifests, expectedDigests);

    for (int i = 0; i < validManifests.size(); ++i) {
        QString manifestFile = validManifests.at(i);
        QDir pkgDir = QFileInfo(manifestFile).dir();

        try {
            std::unique_ptr<PackageInfo> pkg = std::move(pkgs[i]);

            if (!pkg) { // the YAML file was not parseable and we ignore broken manifests
                qCWarning(LogSystem) << "The file" << manifestFile << "is not a valid manifest YAML"
                                        " file and will be ignored.";
                continue;
            }

            if (pkg->id() != pkgDir.dirName()) {
                throw Exception("an info.yaml for packages must be in a directory that has"
                                " the same name as the package's id: found id '%1' in directory '%2'")
                    .arg(pkg->id(), pkgDir.path());
            }

            pkg->setInstallationReport(validReports.at(i).release());
            pkg->setBaseDir(pkgDir.path());
            m_installedPackages.append(pkg.release());

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

#include "moc_packagedatabase.cpp"
