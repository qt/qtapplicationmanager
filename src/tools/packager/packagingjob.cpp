// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDirIterator>
#include <QMessageAuthenticationCode>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <stdio.h>
#include <stdlib.h>

#include "exception.h"
#include "signature.h"
#include "qtyaml.h"
#include "packageinfo.h"
#include "applicationinfo.h"
#include "intentinfo.h"
#include "installationreport.h"
#include "packageextractor.h"
#include "packagecreator.h"

#include "packagingjob.h"

QT_USE_NAMESPACE_AM

// this corresponds to the -b parameter for mkfs.ext2 in sudo.cpp
static const int Ext2BlockSize = 1024;


PackagingJob *PackagingJob::create(const QString &destinationName, const QString &sourceDir,
                                   const QVariantMap &extraMetaData,
                                   const QVariantMap &extraSignedMetaData, bool asJson)
{
    PackagingJob *p = new PackagingJob();
    p->m_mode = Create;
    p->m_asJson = asJson;
    p->m_destinationName = destinationName;
    p->m_sourceDir = sourceDir;
    p->m_extraMetaData = extraMetaData;
    p->m_extraSignedMetaData = extraSignedMetaData;
    return p;
}

PackagingJob *PackagingJob::developerSign(const QString &sourceName, const QString &destinationName,
                                  const QString &certificateFile, const QString &passPhrase,
                                  bool asJson)
{
    PackagingJob *p = new PackagingJob();
    p->m_mode = DeveloperSign;
    p->m_asJson = asJson;
    p->m_sourceName = sourceName;
    p->m_destinationName = destinationName;
    p->m_passphrase = passPhrase;
    p->m_certificateFiles = QStringList { certificateFile };
    return p;
}

PackagingJob *PackagingJob::developerVerify(const QString &sourceName, const QStringList &certificateFiles)
{
    PackagingJob *p = new PackagingJob();
    p->m_mode = DeveloperVerify;
    p->m_sourceName = sourceName;
    p->m_certificateFiles = certificateFiles;
    return p;
}

PackagingJob *PackagingJob::storeSign(const QString &sourceName, const QString &destinationName,
                              const QString &certificateFile, const QString &passPhrase,
                              const QString &hardwareId, bool asJson)
{
    PackagingJob *p = new PackagingJob();
    p->m_mode = StoreSign;
    p->m_asJson = asJson;
    p->m_sourceName = sourceName;
    p->m_destinationName = destinationName;
    p->m_passphrase = passPhrase;
    p->m_certificateFiles = QStringList { certificateFile };
    p->m_hardwareId = hardwareId;
    return p;
}

PackagingJob *PackagingJob::storeVerify(const QString &sourceName, const QStringList &certificateFiles, const QString &hardwareId)
{
    PackagingJob *p = new PackagingJob();
    p->m_mode = StoreVerify;
    p->m_sourceName = sourceName;
    p->m_certificateFiles = certificateFiles;
    p->m_hardwareId = hardwareId;
    return p;
}

QString PackagingJob::output() const
{
    return m_output;
}

int PackagingJob::resultCode() const
{
    return m_resultCode;
}

PackagingJob::PackagingJob()
{ }

void PackagingJob::execute() Q_DECL_NOEXCEPT_EXPR(false)
{
    switch (m_mode) {
    case Create: {
        if (m_destinationName.isEmpty())
            throw Exception(Error::Package, "no destination package name given");

        QFile destination(m_destinationName);
        if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate))
            throw Exception(destination, "could not create package file");

        QString canonicalDestination = QFileInfo(destination).canonicalFilePath();

        QDir source(m_sourceDir);
        if (!source.exists())
            throw Exception(Error::Package, "source %1 is not a directory").arg(m_sourceDir);

        // check metadata
        QString infoName = qSL("info.yaml");
        QScopedPointer<PackageInfo> package(PackageInfo::fromManifest(source.absoluteFilePath(infoName)));

        // warn the user that old-style manifests are going to be deprecated in the future
        try {
            QFile f(package->manifestPath());
            f.open(QFile::ReadOnly);
            YamlParser p(f.readAll());

            auto header = p.parseHeader();
            if (header.first == qL1S("am-application") && header.second == 1) {
                fprintf(stderr, "WARNING: 'info.yaml' is still using the old format (type '%s', version '%d').\n"
                                "         This is going to be deprecated in a future release.\n",
                        qPrintable(header.first), header.second);
            }
        } catch (...) {
        }

        // build report
        InstallationReport report(package->id());
        report.addFile(infoName);

        // check icon
        if (!QFile::exists(source.absoluteFilePath(package->icon())))
            throw Exception(Error::Package, "missing the file referenced by the 'icon' field");
        report.addFile(package->icon());

        // check intent icons
        auto intents = package->intents();
        for (const auto intent : intents) {
            if (!QFile::exists(source.absoluteFilePath(intent->icon()))) {
                throw Exception(Error::Package, "missing the file referenced by the 'icon' field for intent '%1'")
                    .arg(intent->id());
            }
        }

        // check executables
        auto applications = package->applications();
        if (applications.isEmpty())
            throw Exception(Error::Package, "no applications defined in package");
        for (const auto application : applications) {
            if (!QFile::exists(source.absoluteFilePath(application->codeFilePath()))) {
                throw Exception(Error::Package, "missing the file referenced by the 'code' field for application '%1'")
                    .arg(application->id());
            }
        }

        quint64 estimatedImageSize = 0;
        QString canonicalSourcePath = source.canonicalPath();
        QDirIterator it(source.absolutePath(), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

        while (it.hasNext()) {
            it.next();
            QFileInfo entryInfo = it.fileInfo();
            QString entryPath = entryInfo.canonicalFilePath();

            // do not package the package itself, in case someone builds the package within the source dir
            if (canonicalDestination == entryPath)
                continue;

            // prevent the packaging of symlinks
            if (entryInfo.isSymLink()) {
                fprintf(stderr, "WARNING: sym-links are not supported (found: %s -> %s)\n",
                        qPrintable(entryInfo.filePath()), qPrintable(entryInfo.symLinkTarget()));
                continue;
            }

            // we sorted out sym-links, but just to be safe, we check the canonical path
            if (!entryPath.startsWith(canonicalSourcePath)) {
                throw Exception(Error::Package, "file %1 is not inside the source directory %2")
                    .arg(entryInfo.filePath()).arg(canonicalSourcePath);
            }

            // QDirIterator::filePath() returns absolute paths, although the naming suggests otherwise
            entryPath = entryPath.mid(canonicalSourcePath.size() + 1);

            if (entryInfo.fileName().startsWith(qL1S("--PACKAGE-")))
                throw Exception(Error::Package, "file names starting with --PACKAGE- are reserved by the packager (found: %1)").arg(entryPath);

            estimatedImageSize += (quint64(entryInfo.size()) + Ext2BlockSize - 1) / Ext2BlockSize;

            if (entryPath != infoName && entryPath != package->icon())
                report.addFile(entryPath);
        }

        // we have the estimatedImageSize for the raw content now, but we need to add the inode
        // overhead still. This algorithm comes from buildroot:
        // http://git.buildroot.net/buildroot/tree/package/mke2img/mke2img
        estimatedImageSize = (500 + (estimatedImageSize + quint64(report.files().count()) + 400 / 8) * 11 / 10) * Ext2BlockSize;
        report.setDiskSpaceUsed(estimatedImageSize);

        // set extra metadata
        report.setExtraMetaData(m_extraMetaData);
        report.setExtraSignedMetaData(m_extraSignedMetaData);

        // finally create the package
        PackageCreator creator(source, &destination, report);
        if (!creator.create())
            throw Exception(Error::Package, "could not create package %1: %2").arg(package->id()).arg(creator.errorString());

        QVariantMap md = creator.metaData();
        m_output = QString::fromUtf8(m_asJson ? QJsonDocument::fromVariant(md).toJson()
                                              : QtYaml::yamlFromVariantDocuments({ md }));
        break;
    }
    case DeveloperSign:
    case DeveloperVerify:
    case StoreSign:
    case StoreVerify: {
        if (!QFile::exists(m_sourceName))
            throw Exception(Error::Package, "package file %1 does not exist").arg(m_sourceName);

        // read certificates
        QList<QByteArray> certificates;
        for (const QString &cert : qAsConst(m_certificateFiles)) {
            QFile cf(cert);
            if (!cf.open(QIODevice::ReadOnly))
                throw Exception(cf, "could not open certificate file");
            certificates << cf.readAll();
        }

        // create temporary dir for extraction
        QTemporaryDir tmp;
        if (!tmp.isValid())
            throw Exception(Error::Package, "could not create temporary directory %1").arg(tmp.path());

        // extract source
        PackageExtractor extractor(QUrl::fromLocalFile(m_sourceName), tmp.path());
        if (!extractor.extract())
            throw Exception(Error::Package, "could not extract package %1: %2").arg(m_sourceName).arg(extractor.errorString());

        InstallationReport report = extractor.installationReport();

        // check signatures
        if (m_mode == DeveloperVerify) {
            if (report.developerSignature().isEmpty()) {
                m_output = qSL("no developer signature");
                m_resultCode = 1;
            } else {
                Signature sig(report.digest());
                if (!sig.verify(report.developerSignature(), certificates)) {
                    m_output = qSL("invalid developer signature (") + sig.errorString() + qSL(")");
                    m_resultCode = 2;
                } else {
                    m_output = qSL("valid developer signature");
                }
            }
            break; // done with DeveloperVerify

        } else if (m_mode == StoreVerify) {
            if (report.storeSignature().isEmpty()) {
                m_output = qSL("no store signature");
                m_resultCode = 1;
            } else {
                QByteArray sigDigest = report.digest();
                if (!m_hardwareId.isEmpty())
                    sigDigest = QMessageAuthenticationCode::hash(sigDigest, m_hardwareId.toUtf8(), QCryptographicHash::Sha256);

                Signature sig(sigDigest);
                if (!sig.verify(report.storeSignature(), certificates)) {
                    m_output = qSL("invalid store signature (") + sig.errorString() + qSL(")");
                    m_resultCode = 2;
                } else {
                    m_output = qSL("valid store signature");
                }

            }
            break; // done with StoreVerify
        }

        // create a signed package
        if (m_destinationName.isEmpty())
            throw Exception(Error::Package, "no destination package name given");

        QFile destination(m_destinationName);
        if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate))
            throw Exception(destination, "could not create package file");

        PackageCreator creator(tmp.path(), &destination, report);

        if (certificates.size() != 1)
            throw Exception(Error::Package, "cannot sign packages with more than one certificate");

        if (m_mode == DeveloperSign) {
            Signature sig(report.digest());
            QByteArray signature = sig.create(certificates.first(), m_passphrase.toUtf8());

            if (signature.isEmpty())
                throw Exception(Error::Package, "could not create signature: %1").arg(sig.errorString());
            report.setDeveloperSignature(signature);
        } else if (m_mode == StoreSign) {
            QByteArray sigDigest = report.digest();
            if (!m_hardwareId.isEmpty())
                sigDigest = QMessageAuthenticationCode::hash(sigDigest, m_hardwareId.toUtf8(), QCryptographicHash::Sha256);

            Signature sig(sigDigest);
            QByteArray signature = sig.create(certificates.first(), m_passphrase.toUtf8());

            if (signature.isEmpty())
                throw Exception(Error::Package, "could not create signature: %1").arg(sig.errorString());
            report.setStoreSignature(signature);
        }

        if (!creator.create())
            throw Exception(Error::Package, "could not create package %1: %2").arg(m_destinationName).arg(creator.errorString());

        QVariantMap md = creator.metaData();
        m_output = QString::fromUtf8(m_asJson ? QJsonDocument::fromVariant(md).toJson()
                                              : QtYaml::yamlFromVariantDocuments({ md }));
        break;
    }
    default:
        throw Exception("invalid mode");
    }
}
