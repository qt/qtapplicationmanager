/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
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

#include <QStringList>
#include <QAtomicInt>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>
#include <qplatformdefs.h>

#include <archive.h>
#include <archive_entry.h>

#include "packageutilities_p.h"
#include "packagecreator.h"
#include "packagecreator_p.h"
#include "exception.h"
#include "error.h"
#include "installationreport.h"
#include "qtyaml.h"

// archive.h might #define this for Android
#ifdef open
#  undef open
#endif
// these are not defined on all platforms
#ifndef S_IREAD
#  define S_IREAD S_IRUSR
#endif
#ifndef S_IEXEC
#  define S_IEXEC S_IXUSR
#endif

QT_BEGIN_NAMESPACE_AM

/*! \internal
  This is a workaround for the stupid filename encoding handling in libarchive:
  the 'hdrcharset' option is not taken into consideration at all, plus the Windows
  version simply defaults to hard-coded (non-UTF8) encoding.
  This trick gets us consistent behavior on all platforms.
*/
static void fixed_archive_entry_set_pathname(archive_entry *entry, const QString &pathname)
{
    wchar_t *wchars = new wchar_t[size_t(pathname.length()) + 1];
    wchars[pathname.toWCharArray(wchars)] = 0;
    archive_entry_copy_pathname_w(entry, wchars);
    delete[] wchars;
}


PackageCreator::PackageCreator(const QDir &sourceDir, QIODevice *output, const InstallationReport &report, QObject *parent)
    : QObject(parent)
    , d(new PackageCreatorPrivate(this, output, report))
{
    setSourceDirectory(sourceDir);
}

PackageCreator::~PackageCreator()
{
    delete d;
}

QDir PackageCreator::sourceDirectory() const
{
    return QDir(d->m_sourcePath);
}

void PackageCreator::setSourceDirectory(const QDir &sourceDir)
{
    d->m_sourcePath = sourceDir.absolutePath() + QLatin1Char('/');
}

bool PackageCreator::create()
{
    if (!wasCanceled())
        d->create();

    return !wasCanceled() && !hasFailed();
}

QByteArray PackageCreator::createdDigest() const
{
    return d->m_digest;
}

QVariantMap PackageCreator::metaData() const
{
    return d->m_metaData;
}

bool PackageCreator::hasFailed() const
{
    return d->m_failed || wasCanceled();
}

bool PackageCreator::wasCanceled() const
{
    return d->m_canceled.load();
}

Error PackageCreator::errorCode() const
{
    return wasCanceled() ? Error::Canceled : (d->m_failed ? d->m_errorCode : Error::None);
}

QString PackageCreator::errorString() const
{
    return wasCanceled() ? qSL("canceled") : (d->m_failed ? d->m_errorString : QString());
}

/*! \internal
  This function can be called from another thread while create() is running
*/
void PackageCreator::cancel()
{
    d->m_canceled.fetchAndStoreOrdered(1);
}


/* * * * * * * * * * * * * * * * * *
 *  vvv PackageCreatorPrivate vvv  *
 * * * * * * * * * * * * * * * * * */

PackageCreatorPrivate::PackageCreatorPrivate(PackageCreator *creator, QIODevice *output,
                                             const InstallationReport &report)
    : q(creator)
    , m_output(output)
    , m_report(report)
{ }

bool PackageCreatorPrivate::create()
{
    struct archive *ar = nullptr;
    char buffer[64 * 1024];

    try {
        if (m_report.packageId().isNull())
            throw Exception("package identifier is null");

        QCryptographicHash digest(QCryptographicHash::Sha256);

        QVariantMap headerFormat {
            { qSL("formatType"), qSL("am-package-header") },
            { qSL("formatVersion"), 2 }
        };

        m_metaData = QVariantMap {
            { qSL("packageId"), m_report.packageId() },
            { qSL("diskSpaceUsed"), m_report.diskSpaceUsed() }
        };
        if (!m_report.extraMetaData().isEmpty())
            m_metaData[qSL("extra")] = m_report.extraMetaData();
        if (!m_report.extraSignedMetaData().isEmpty())
            m_metaData[qSL("extraSigned")] = m_report.extraSignedMetaData();

        PackageUtilities::addHeaderDataToDigest(m_metaData, digest);

        emit q->progress(0);

        ar = archive_write_new();
        if (!ar)
            throw Exception(Error::Archive, "[libarchive] could not create a new archive object");
        if (archive_write_set_format_ustar(ar) != ARCHIVE_OK)
            throw ArchiveException(ar, "could not set the archive format to USTAR");
        if (archive_write_set_options(ar, "hdrcharset=UTF-8") != ARCHIVE_OK)
            throw ArchiveException(ar, "could not set the HDRCHARSET option");
        if (archive_write_add_filter_gzip(ar) != ARCHIVE_OK)
            throw ArchiveException(ar, "could not enable GZIP compression");
// disabled for now -- see libarchive.pro
//        if (archive_write_add_filter_xz(ar) != ARCHIVE_OK)
//            throw ArchiveException(ar, "could not enable XZ compression");

        auto dummyCallback = [](archive *, void *){ return ARCHIVE_OK; };
        auto writeCallback = [](archive *, void *user, const void *buffer, size_t size) {
            // this could be simpler, if we had an event loop ... but we do not
            QIODevice *output = reinterpret_cast<QIODevice *>(user);
            qint64 written = output->write(static_cast<const char *>(buffer), size);
            output->waitForBytesWritten(-1);
            return static_cast<__LA_SSIZE_T>(written);
        };

        if (archive_write_open(ar, m_output, dummyCallback, writeCallback, dummyCallback) != ARCHIVE_OK)
            throw ArchiveException(ar, "could not open archive.");

        // Add the metadata header

        if (!addVirtualFile(ar, qSL("--PACKAGE-HEADER--"), QtYaml::yamlFromVariantDocuments(QVector<QVariant> { headerFormat, m_metaData })))
            throw ArchiveException(ar, "could not write '--PACKAGE-HEADER--' to archive");

        // Add all regular files

        QStringList allFiles = m_report.files();

        // Calculate the total size first, so we can report progress later on

        qint64 allFilesSize = 0;
        for (const QString &file : qAsConst(allFiles)) {
            QFileInfo fi(m_sourcePath + file);

            if (!fi.exists())
                throw Exception(Error::IO, "file not found: %1").arg(fi.absoluteFilePath());
            allFilesSize += fi.size();
        }

        qint64 packagedSize = 0;
        int lastProgress = 0;

        // Iterate over all files in the report

        for (const QString &file : qAsConst(allFiles)) {
            if (q->wasCanceled())
                throw Exception(Error::Canceled);

            // Name and mode for archive entry

            QString filePath = m_sourcePath + file;
            QFileInfo fi(filePath);

            PackageEntryType packageEntryType;
            mode_t mode;

            if (fi.isDir()) {
                packageEntryType = PackageEntry_Dir;
                mode = S_IFDIR | S_IREAD | S_IEXEC;
            } else if (fi.isFile() && !fi.isSymLink()) {
                packageEntryType = PackageEntry_File;
                mode = S_IFREG | S_IREAD | (fi.permission(QFile::ExeOwner) ? S_IEXEC : 0);
#if defined(Q_OS_WIN)
                // We do not have x-bits for stuff that has been cross-compiled on Windows,
                // so this crude hack sets the x-bit in the package for all ELF files.
                QFile f(fi.absoluteFilePath());
                if (f.open(QFile::ReadOnly)) {
                    if (f.read(4) == "\x7f""ELF")
                        mode |= S_IEXEC;
                }
#endif
            } else {
                throw Exception(Error::Package, "inode '%1' is neither a directory or a file").arg(fi.filePath());
            }

            // Add to archive

            archive_entry *entry = archive_entry_new();
            if (!entry)
                throw Exception(Error::Archive, "[libarchive] could not create a new archive_entry object");

            fixed_archive_entry_set_pathname(entry, file); // please note: this is a special function (see top of file)
            archive_entry_set_size(entry, static_cast<__LA_INT64_T>(fi.size()));
            archive_entry_set_mode(entry, mode);

            bool headerOk = (archive_write_header(ar, entry) == ARCHIVE_OK);

            archive_entry_free(entry);

            if (!headerOk)
                throw ArchiveException(ar, "could not write header");

            if (packageEntryType == PackageEntry_File) {
                QFile f(fi.absoluteFilePath());
                if (!f.open(QIODevice::ReadOnly))
                    throw Exception(f, "could not open for reading");

                qint64 fileSize = 0;
                while (!f.atEnd()) {
                    if (q->wasCanceled())
                        throw Exception(Error::Canceled);

                    qint64 bytesRead = f.read(buffer, sizeof(buffer));
                    if (bytesRead < 0)
                        throw Exception(f, "could not read from file");
                    fileSize += bytesRead;

                    if (archive_write_data(ar, buffer, static_cast<size_t>(bytesRead)) == -1)
                        throw ArchiveException(ar, "could not write to archive");

                    digest.addData(buffer, static_cast<int>(bytesRead));
                }

                if (fileSize != fi.size())
                    throw Exception(Error::Archive, "size mismatch for '%1' between stating (%2) and reading (%3)").arg(fi.filePath()).arg(fi.size()).arg(fileSize);

                packagedSize += fileSize;
            }

            // Just to be on the safe side, we also add the file's meta-data to the digest
            PackageUtilities::addFileMetadataToDigest(file, fi, digest);

            int progress = allFilesSize ? int(packagedSize * 100 / allFilesSize) : 0;
            if (progress != lastProgress ) {
                emit q->progress(qreal(progress) / 100);
                lastProgress = progress;
            }
        }

        m_digest = digest.result();
        if (!m_report.digest().isEmpty()) {
            if (m_digest != m_report.digest())
                throw Exception(Error::Package, "package digest mismatch (is %1, but should be %2)").arg(m_digest.toHex()).arg(m_report.digest().toHex());
        }

        // Add the metadata footer

        QVariantMap footerFormat {
            { qSL("formatType"), qSL("am-package-footer") },
            { qSL("formatVersion"), 2 }
        };

        QVariantMap footerData {
            { qSL("digest"), QLatin1String(m_digest.toHex()) },
        };

        if (!addVirtualFile(ar, qSL("--PACKAGE-FOOTER--"), QtYaml::yamlFromVariantDocuments(QVector<QVariant> { footerFormat, footerData })))
            throw ArchiveException(ar, "could not add '--PACKAGE-FOOTER--' to archive");

        m_metaData.unite(footerData);

        if (!m_report.developerSignature().isEmpty()) {
            QVariantMap footerDevSig {
                { qSL("developerSignature"), QLatin1String(m_report.developerSignature().toBase64()) }
            };
            if (!addVirtualFile(ar, qSL("--PACKAGE-FOOTER--"), QtYaml::yamlFromVariantDocuments(QVector<QVariant> { footerDevSig })))
                throw ArchiveException(ar, "could not add '--PACKAGE-FOOTER--' to archive");

            m_metaData.unite(footerDevSig);
        }
        if (!m_report.storeSignature().isEmpty()) {
            QVariantMap footerStoreSig {
                { qSL("storeSignature"), QLatin1String(m_report.storeSignature().toBase64()) }
            };
            if (!addVirtualFile(ar, qSL("--PACKAGE-FOOTER--"), QtYaml::yamlFromVariantDocuments(QVector<QVariant> { footerStoreSig })))
                throw ArchiveException(ar, "could not add '--PACKAGE-FOOTER--' to archive");

            m_metaData.unite(footerStoreSig);
        }

        if (archive_write_free(ar) != ARCHIVE_OK)
            throw ArchiveException(ar, "could not close archive");


        emit q->progress(1);

        return true;

    } catch (const Exception &e) {
        if (!q->wasCanceled())
            setError(e.errorCode(), e.errorString());
    }

    if (ar)
        archive_write_free(ar);

    return false;
}

bool PackageCreatorPrivate::addVirtualFile(struct archive *ar, const QString &file, const QByteArray &data)
{
    bool result = false;
    struct archive_entry *entry = archive_entry_new();

    if (entry) {
        fixed_archive_entry_set_pathname(entry, file);
        archive_entry_set_mode(entry, S_IFREG | S_IREAD);
        archive_entry_set_size(entry, data.size());
        archive_entry_set_mtime(entry, time(nullptr), 0);

        if (archive_write_header(ar, entry) == ARCHIVE_OK) {
            if (archive_write_data(ar, data.constData(), static_cast<size_t>(data.size())) == data.size())
                result = true;
        }
        archive_entry_free(entry);
    }
    return result;
}

void PackageCreatorPrivate::setError(Error errorCode, const QString &errorString)
{
    m_failed = true;

    // only the first error is the one that counts!
    if (m_errorCode == Error::None) {
        m_errorCode = errorCode;
        m_errorString = errorString;
    }
}

QT_END_NAMESPACE_AM
