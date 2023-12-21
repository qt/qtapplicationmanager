// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QUrl>
#include <QFileSystemWatcher>
#include <QDirIterator>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QtAppManCommon/exception.h>
#include <QtAppManCommon/architecture.h>
#include <QtAppManCommon/unixsignalhandler.h>
#include <QtAppManPackage/packagecreator.h>
#include <QtAppManPackage/packageextractor.h>
#include <QtAppManApplication/yamlpackagescanner.h>
#include <QtAppManApplication/packageinfo.h>
#include <QtAppManApplication/installationreport.h>
#include <QtAppManCrypto/signature.h>

#include "pspackages.h"
#include "pspackages_p.h"
#include "psconfiguration.h"
#include "package-server.h"

#if defined(Q_OS_UNIX)
#  include <csignal>
#  define AM_PS_SIGNALS  { SIGTERM, SIGINT, SIGFPE, SIGSEGV, SIGPIPE, SIGABRT, SIGQUIT }
#else
#  include <windows.h>
#  define AM_PS_SIGNALS  { SIGTERM, SIGINT }
#endif


QT_USE_NAMESPACE_AM
using namespace Qt::StringLiterals;

static const QString UploadDirName   = u"upload"_s;
static const QString RemoveDirName   = u"remove"_s;
static const QString PackagesDirName = u".packages"_s;
static const QString LockFileName    = u".lock"_s;


PSPackages::PSPackages(PSConfiguration *cfg, QObject *parent)
    : QObject(parent)
    , d(new PSPackagesPrivate)
{
    d->q = this;
    d->cfg = cfg;
}


PSPackages::~PSPackages()
{ }

void PSPackages::initialize()
{
    QDir &dd = d->cfg->dataDirectory;

    // create a lock file to avoid data corruption
    d->lockFile = std::make_unique<QLockFile>(dd.absoluteFilePath(LockFileName));
    d->lockFile->setStaleLockTime(0); // this is a long-lived lock
    if (!d->lockFile->tryLock(1000)) {
        QString msg;
        if (d->lockFile->error() == QLockFile::LockFailedError) {
            msg = u" - there is an existing .lock file"_s;
            qint64 pid = 0;
            if (d->lockFile->getLockInfo(&pid, nullptr, nullptr))
                msg = msg + u" (belonging to pid %1)"_s.arg(pid);
        }
        throw Exception("could not lock the data directory %1%2")
            .arg(dd.absolutePath()).arg(msg);
    }

    // make sure to always clean up the lock file, even if we crash
    d->lockFilePath = d->lockFile->fileName().toLocal8Bit();


    UnixSignalHandler::instance()->install(UnixSignalHandler::RawSignalHandler, AM_PS_SIGNALS,
                                           [this](int sig) {
        UnixSignalHandler::instance()->resetToDefault(sig);
#if defined(Q_OS_WINDOWS)
        if (!d->lockFilePath.isEmpty())
            ::DeleteFileW((LPCWSTR) d->lockFile->fileName().utf16());
        ::raise(sig);
#else
        if (!d->lockFilePath.isEmpty())
            ::unlink(d->lockFilePath.constData());
        ::kill(0, sig);
#endif
    });

    if (!dd.mkpath(PackagesDirName)) {
        throw Exception("could not create a '%1' directory inside the data directory %2")
            .arg(PackagesDirName).arg(dd.absolutePath());
    }
    if (!dd.mkpath(UploadDirName)) {
        throw Exception("could not create an '%1' directory inside the data directory %2")
            .arg(UploadDirName).arg(dd.absolutePath());
    }
    if (!dd.mkpath(RemoveDirName)) {
        throw Exception("could not create an '%1' directory inside the data directory %2")
            .arg(RemoveDirName).arg(dd.absolutePath());
    }

    d->scanPackages();
    d->scanUploads();
    d->scanRemoves();

    auto dirWatcher = new QFileSystemWatcher({ dd.absoluteFilePath(UploadDirName),
                                              dd.absoluteFilePath(RemoveDirName) }, this);
    QObject::connect(dirWatcher, &QFileSystemWatcher::directoryChanged,
                     this, [&](const QString &dir) {
        const auto dirName = QDir(dir).dirName();
        if (dirName == RemoveDirName)
            d->scanRemoves();
        else if (dirName == UploadDirName)
            d->scanUploads();
    });
}

PSPackage *PSPackages::scan(const QString &filePath)
{
    YamlPackageScanner yps;
    QFileInfo fi(filePath);

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        throw Exception("could not create a temporary directory");

    QFile f(fi.absoluteFilePath());
    if (!f.open(QIODevice::ReadOnly))
        throw Exception(f, "could not open package file");
    if (f.size() > 100 * 1024 * 1024) // 100MiB
        throw Exception(f, "package file is too large");

    QByteArray magic = f.peek(2);
    if (magic != "\x1f\x8b") // .gz
        throw Exception(f, "wrong file type (%1)").arg(magic.toHex());

    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&f))
        throw Exception(f, "could not read the file");
    const auto sha1 = hash.result();

    f.close();

    QString architecture;

    PackageExtractor pe(QUrl::fromLocalFile(fi.absoluteFilePath()), tempDir.path());
    pe.setFileExtractedCallback([&architecture, &pe](const QString &fileName) {
        QString fileArch = Architecture::identify(pe.destinationDirectory().filePath(fileName));

        if (!fileArch.isEmpty()) {
            if (architecture.isEmpty())
                architecture = fileArch;
            else if (architecture != fileArch)
                throw Exception("mixed architectures: %1 and %2").arg(architecture, fileArch);
        }
    });
    if (!pe.extract())
        throw Exception("could not extract package: %1").arg(pe.errorString());

    if (!d->cfg->developerVerificationCaCertificates.isEmpty()) {
        const InstallationReport &report = pe.installationReport();

        // check signatures
        if (report.developerSignature().isEmpty()) {
            throw Exception("no developer signature");
        } else {
            Signature sig(report.digest());
            if (!sig.verify(report.developerSignature(), d->cfg->developerVerificationCaCertificates))
                throw Exception("invalid developer signature (\"%1\")").arg(sig.errorString());
        }
    }

    std::unique_ptr<PackageInfo> pi { yps.scan(tempDir.filePath(u"info.yaml"_s)) };

    auto sp = new PSPackage;
    sp->id = pi->id();
    sp->sha1 = sha1;
    sp->filePath = filePath;
    sp->architecture = architecture;
    sp->packageInfo.reset(pi.release());
    QFile icon(tempDir.filePath(u"icon.png"_s));
    if ((icon.size() < 1 * 1024 * 1024) && icon.open(QIODevice::ReadOnly))
        sp->iconData = icon.readAll();

    return sp;
}

QStringList PSPackages::categories() const
{
    QStringList categories;

    for (const auto &pkg : std::as_const(d->packages)) {
        for (const PSPackage *sp : pkg)
            categories += sp->packageInfo->categories();
    }

    categories.removeDuplicates();
    categories.sort();
    return categories;
}

QList<PSPackage *> PSPackages::byArchitecture(const QString &architecture) const
{
    QList<PSPackage *> packages;

    for (const auto &pkg : std::as_const(d->packages)) {
        PSPackage *sp = pkg.value(architecture, nullptr);
        if (!sp && !architecture.isEmpty())
            sp = pkg.value({ }, nullptr);
        if (sp)
            packages.append(sp);
    }

    return packages;
}

PSPackage *PSPackages::byIdAndArchitecture(const QString &id, const QString &architecture) const
{
    auto *sp = d->packages.value(id).value(architecture, nullptr);
    if (!sp && !architecture.isEmpty())
        sp = d->packages.value(id).value({ }, nullptr);
    return sp;
}

int PSPackages::removeIf(const std::function<bool(PSPackage *)> &pred)
{
    int count = 0;

    for (auto iit = d->packages.begin(); iit != d->packages.end(); ) {
        for (auto ait = iit->begin(); ait != iit->end(); ) {
            PSPackage *sp = *ait;

            if (pred && pred(sp)) {
                colorOut() << ColorPrint::red << " - removing " << ColorPrint::bcyan << sp->id
                           << ColorPrint::reset << " [" << sp->architectureOrAll() << "]";
                QFile::remove(sp->filePath);
                delete sp;
                ait = iit->erase(ait); // clazy:exclude=strict-iterators
                ++count;
            } else {
                ++ait;
            }
        }
        if (iit->isEmpty())
            iit = d->packages.erase(iit); // clazy:exclude=strict-iterators
        else
            ++iit;
    }
    return count;
}


std::pair<PSPackages::UploadResult, PSPackage *> PSPackages::upload(const QString &filePath)
{
    std::pair<UploadResult, PSPackage *> result { UploadResult::NoChanges, nullptr };

    try {
        std::unique_ptr<PSPackage> scannedSp { scan(filePath) };

        QString id = scannedSp->id;
        QString architecture = scannedSp->architecture;

        const QString finalPath = d->cfg->dataDirectory.absoluteFilePath(PackagesDirName) + u'/'
                                  + id + u'_' + scannedSp->architectureOrAll() + u".ampkg"_s;

        auto existingSp = d->packages.value(id).value(architecture, nullptr);

        if (existingSp) {
            Q_ASSERT(existingSp->filePath == finalPath);

            if (existingSp->sha1 == scannedSp->sha1) {
                QFile::remove(filePath);
                result.second = existingSp;
                result.first = UploadResult::NoChanges;
            } else {
                QFile::remove(finalPath);
                if (!QFile::rename(filePath, finalPath))
                    throw Exception("could not rename from %1 to %2").arg(filePath, finalPath);

                scannedSp->filePath = finalPath;
                d->packages[id][architecture] = result.second = scannedSp.release();
                result.first = UploadResult::Updated;
                delete existingSp;
            }
        } else {
            if (!QFile::rename(filePath, finalPath))
                throw Exception("could not rename from %1 to %2").arg(filePath, finalPath);

            scannedSp->filePath = finalPath;
            d->packages[id][architecture] = result.second = scannedSp.release();
            result.first = UploadResult::Added;
        }

        const char *action = nullptr;
        auto color = ColorPrint::reset;
        const char *msg = "";

        switch (result.first) {
        case UploadResult::NoChanges:
            action = " = skipping ";
            color = ColorPrint::blue;
            msg = " (no changes)";
            break;
        case UploadResult::Updated:
            action = " ! updating ";
            color = ColorPrint::yellow;
            break;
        case UploadResult::Added:
            action = " + adding   ";
            color = ColorPrint::green;
            break;
        }
        colorOut() << color << action << ColorPrint::bcyan << result.second->id
                   << ColorPrint::reset << " [" << result.second->architectureOrAll() << "]"
                   << msg;

        return result;

    } catch (const Exception &e) {
        colorOut() << ColorPrint::red << " x failed   " << ColorPrint::bcyan << filePath
                   << ColorPrint::reset << " ("  << e.errorString() << ")";
        QFile::remove(filePath);
        throw;
    }
}

void PSPackages::storeSign(PSPackage *sp, const QString &hardwareId, QIODevice *destination)
{
    // extract to temp dir, store-sign and re-create at destination

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        throw Exception("could not create a temporary directory");

    PackageExtractor pe(QUrl::fromLocalFile(sp->filePath), tempDir.path());
    if (!pe.extract())
        throw Exception("could not extract package: %1").arg(pe.errorString());

    InstallationReport report = pe.installationReport();

    QByteArray sigDigest = report.digest();
    if (!hardwareId.isEmpty()) {
        sigDigest = QMessageAuthenticationCode::hash(sigDigest, hardwareId.toUtf8(),
                                                     QCryptographicHash::Sha256);
    }

    Signature sig(sigDigest);
    QByteArray signature = sig.create(d->cfg->storeSignCertificate, d->cfg->storeSignPassword.toUtf8());

    if (signature.isEmpty())
        throw Exception("could not create store signature: %1").arg(sig.errorString());
    report.setStoreSignature(signature);

    PackageCreator pc(tempDir.path(), destination, report);
    if (!pc.create())
        throw Exception("could not re-create package: %1").arg(pc.errorString());
}


///////////////////////////////////////////////////////////////////////////////////////////////////


void PSPackagesPrivate::scanRemoves()
{
    int fileCount = 0;
    QDirIterator dit(cfg->dataDirectory.absoluteFilePath(RemoveDirName), QDir::Files);
    while (dit.hasNext()) {
        if (++fileCount == 1)
            colorOut() << "> Scanning " << RemoveDirName;

        dit.next();
        QString matchStr = dit.fileName();
        if (matchStr.endsWith(u".ampkg"_s))
            matchStr.chop(6);

        int removeCount = q->removeIf([matchStr](PSPackage *sp) {
            return (sp->id == matchStr)
                   || (QString(sp->id + u'_' + sp->architectureOrAll()) == matchStr);
        });

        if (!removeCount) {
            colorOut() << ColorPrint::blue << " = skipping " << ColorPrint::bcyan << matchStr
                       << ColorPrint::reset << " (no match)";
        }

        QFile::remove(dit.filePath());
    }
}

void PSPackagesPrivate::scanUploads()
{
    int fileCount = 0;
    QDirIterator dit(cfg->dataDirectory.absoluteFilePath(UploadDirName), QDir::Files);
    while (dit.hasNext()) {
        if (++fileCount == 1)
            colorOut() << "> Scanning " << UploadDirName;

        try {
            q->upload(dit.next());
        } catch (const Exception &) { /* ignore */ }
    }
}

void PSPackagesPrivate::scanPackages()
{
    Q_ASSERT(packages.isEmpty());
    colorOut() << "> Scanning " << PackagesDirName;

    int fileCount = 0;
    QDirIterator dit(cfg->dataDirectory.absoluteFilePath(PackagesDirName), QDir::Files);
    while (dit.hasNext()) {
        ++fileCount;

        const QString filePath = dit.next();

        try {
            std::unique_ptr<PSPackage> scannedSp { q->scan(filePath) };

            QString id = scannedSp->id;
            QString architecture = scannedSp->architecture;
            QString architectureOrAll = scannedSp->architectureOrAll();

            const QString expectedFileName = id + u'_' + architectureOrAll + u".ampkg"_s;

            if (dit.fileName() != expectedFileName) {
                throw Exception("invalid package name: expected %1, found %2").arg(expectedFileName)
                    .arg(dit.fileName());
            }

            if (packages.value(id).value(architecture, nullptr)) {
                throw Exception("internal error: duplicate package, id: %1, architecture: %2")
                    .arg(id).arg(architecture);
            }

            packages[id][architecture] = scannedSp.release();

            colorOut() << ColorPrint::green << " + adding   " << ColorPrint::bcyan << id
                       << ColorPrint::reset << " [" << architectureOrAll << "]";

        } catch (const Exception &e) {
            colorOut() << ColorPrint::blue << " = skipping " << ColorPrint::bcyan << dit.fileName()
                       << ColorPrint::reset << " (" << e.errorString() << ")";
        }
    }

    if (!fileCount)
        colorOut() << " (" << ColorPrint::yellow << "none found" << ColorPrint::reset << ")";
}


///////////////////////////////////////////////////////////////////////////////////////////////////


QString PSPackage::architectureOrAll() const
{
    return architecture.isEmpty() ? u"all"_s  : architecture;
}

#include "moc_pspackages.cpp"
