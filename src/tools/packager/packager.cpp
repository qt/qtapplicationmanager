/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
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
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QRegExp>
#include <QDirIterator>

#include <stdio.h>
#include <stdlib.h>

#include "yamlapplicationscanner.h"
#include "application.h"
#include "installationreport.h"
#include "digestfilter.h"
#include "packageextractor.h"
#include "packagecreator.h"
#include "signature.h"
#include "exception.h"
#include "utilities.h"
#include "packager.h"


// this corresponds to the -b parameter for mkfs.ext2 in sudo.cpp
static const int Ext2BlockSize = 1024;


Packager *Packager::create(const QString &destinationName, const QString &sourceDir)
{
    Packager *p = new Packager();
    p->m_mode = Create;
    p->m_destinationName = destinationName;
    p->m_sourceDir = sourceDir;
    return p;
}

Packager *Packager::developerSign(const QString &sourceName, const QString &destinationName, const QString &certificateFile, const QString &passPhrase)
{
    Packager *p = new Packager();
    p->m_mode = DeveloperSign;
    p->m_sourceName = sourceName;
    p->m_destinationName = destinationName;
    p->m_passphrase = passPhrase;
    p->m_certificateFiles = QStringList { certificateFile };
    return p;
}

Packager *Packager::developerVerify(const QString &sourceName, const QStringList &certificateFiles)
{
    Packager *p = new Packager();
    p->m_mode = DeveloperVerify;
    p->m_sourceName = sourceName;
    p->m_certificateFiles = certificateFiles;
    return p;
}

Packager *Packager::storeSign(const QString &sourceName, const QString &destinationName, const QString &certificateFile, const QString &passPhrase, const QString &hardwareId)
{
    Packager *p = new Packager();
    p->m_mode = StoreSign;
    p->m_sourceName = sourceName;
    p->m_destinationName = destinationName;
    p->m_passphrase = passPhrase;
    p->m_certificateFiles = QStringList { certificateFile };
    p->m_hardwareId = hardwareId;
    return p;
}

Packager *Packager::storeVerify(const QString &sourceName, const QStringList &certificateFiles, const QString &hardwareId)
{
    Packager *p = new Packager();
    p->m_mode = StoreVerify;
    p->m_sourceName = sourceName;
    p->m_certificateFiles = certificateFiles;
    p->m_hardwareId = hardwareId;
    return p;
}

QByteArray Packager::packageDigest() const
{
    return m_digest;
}

QString Packager::output() const
{
    return m_output;
}

int Packager::resultCode() const
{
    return m_resultCode;
}

Packager::Packager()
{ }

void Packager::execute() throw(Exception)
{
    switch (m_mode) {
    case Create: {
        if (m_destinationName.isEmpty())
            throw Exception(Error::Package, "no destination package name given");

        QFile destination(m_destinationName);
        if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate))
            throw Exception(destination, "could not create package file");

        QDir source(m_sourceDir);
        if (!source.exists())
            throw Exception(Error::Package, "source %1 is not a directory").arg(m_sourceDir);

        // check metadata
        YamlApplicationScanner yas;
        QString infoName = yas.metaDataFileName();
        Application *app = yas.scan(source.absoluteFilePath(infoName));

        // build report
        InstallationReport report(app->id());
        report.addFile(infoName);

        if (app->type() == Application::Gui) {
            if (!QFile::exists(source.absoluteFilePath(app->icon())))
                throw Exception(Error::Package, "missing the 'icon.png' file");
            report.addFile(qSL("icon.png"));
        }

        // check executable
        if (!QFile::exists(source.absoluteFilePath(app->codeFilePath())))
            throw Exception(Error::Package, "missing the file referenced by the 'code' field");

        quint64 estimatedImageSize = 0;
        QString canonicalSourcePath = source.canonicalPath();
        QDirIterator it(source.absolutePath(), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

        while (it.hasNext()) {
            it.next();
            QFileInfo entryInfo = it.fileInfo();
            QString entryPath = entryInfo.canonicalFilePath();

            if (!entryPath.startsWith(canonicalSourcePath))
                throw Exception(Error::Package, "file %1 is not inside the source directory %2").arg(entryPath).arg(canonicalSourcePath);

            // QDirIterator::filePath() returns absolute paths, although the naming suggests otherwise
            entryPath = entryPath.mid(canonicalSourcePath.size() + 1);

            if (entryInfo.fileName().startsWith(qL1S("--PACKAGE-")))
                throw Exception(Error::Package, "file names starting with --PACKAGE- are reserved by the packager (found: %1)").arg(entryPath);

            estimatedImageSize += (entryInfo.size() + Ext2BlockSize - 1) / Ext2BlockSize;

            if (entryPath != infoName && entryPath != qL1S("icon.png"))
                report.addFile(entryPath);
        }

        // we have the estimatedImageSize for the raw content now, but we need to add the inode
        // overhead still. This algorithm comes from buildroot:
        // http://git.buildroot.net/buildroot/tree/package/mke2img/mke2img
        estimatedImageSize = (500 + (estimatedImageSize + report.files().count() + 400 / 8) * 11 / 10) * Ext2BlockSize;
        report.setDiskSpaceUsed(estimatedImageSize);

        // finally create the package
        PackageCreator creator(source, &destination, report);
        if (!creator.create())
            throw Exception(Error::Package, "could not create package %1: %2").arg(app->id()).arg(creator.errorString());

        m_digest = creator.createdDigest();
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
        foreach (const QString &cert, m_certificateFiles) {
            QFile cf(cert);
            if (!cf.open(QIODevice::ReadOnly))
                throw Exception(cf, "could not open certificate file");
            certificates << cf.readAll();
        }

        // create temporary dir for extraction
        TemporaryDir tmp;
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
                QByteArray digestPlusId = HMACFilter::hmac(HMACFilter::Sha256, m_hardwareId.toUtf8(), report.digest());
                Signature sig(digestPlusId);
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
            QByteArray digestPlusId = HMACFilter::hmac(HMACFilter::Sha256, m_hardwareId.toUtf8(), report.digest());
            Signature sig(digestPlusId);
            QByteArray signature = sig.create(certificates.first(), m_passphrase.toUtf8());

            if (signature.isEmpty())
                throw Exception(Error::Package, "could not create signature: %1").arg(sig.errorString());
            report.setStoreSignature(signature);
        }

        if (!creator.create())
            throw Exception(Error::Package, "could not create package %1: %2").arg(m_destinationName).arg(creator.errorString());

        m_digest = creator.createdDigest();
        break;
    }
    default:
        throw Exception(Error::System, "invalid mode");
    }
}
