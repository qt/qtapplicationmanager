/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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
#include <QThread>
#include <QAtomicInt>
#include <QFile>
#include <QDir>
#include <QRegExp>
#include <QDataStream>
#include <QUrl>
#include <QDebug>
#include <QCryptographicHash>

#include <archive.h>
#include <archive_entry.h>

#include "package_p.h"
#include "packageextractor.h"
#include "packageextractor_p.h"
#include "exception.h"
#include "error.h"
#include "installationreport.h"
#include "utilities.h"
#include "qtyaml.h"

// these are not defined on all platforms
#ifndef S_IREAD
#  define S_IREAD S_IRUSR
#endif
#ifndef S_IEXEC
#  define S_IEXEC S_IXUSR
#endif

QT_BEGIN_NAMESPACE_AM

PackageExtractor::PackageExtractor(const QUrl &downloadUrl, const QDir &destinationDir, QObject *parent)
    : QObject(parent)
    , d(new PackageExtractorPrivate(this, downloadUrl))
{
    setDestinationDirectory(destinationDir);
}

QDir PackageExtractor::destinationDirectory() const
{
    return QDir(d->m_destinationPath);
}

void PackageExtractor::setDestinationDirectory(const QDir &destinationDir)
{
    d->m_destinationPath = destinationDir.absolutePath() + qL1C('/');
}

void PackageExtractor::setFileExtractedCallback(const std::function<void(const QString &)> &callback)
{
    d->m_fileExtractedCallback = callback;
}

const InstallationReport &PackageExtractor::installationReport() const
{
    return d->m_report;
}

bool PackageExtractor::extract()
{
    if (!wasCanceled()) {
        d->m_failed = false;

        d->download(d->m_url);

        QMetaObject::invokeMethod(d, "extract", Qt::QueuedConnection);
        d->m_loop.exec();

        delete d->m_reply;
        d->m_reply = 0;
    }
    return !wasCanceled() && !hasFailed();
}

bool PackageExtractor::hasFailed() const
{
    return d->m_failed || wasCanceled();
}

bool PackageExtractor::wasCanceled() const
{
    return d->m_canceled.load();
}

Error PackageExtractor::errorCode() const
{
    return wasCanceled() ? Error::Canceled : (d->m_failed ? d->m_errorCode : Error::None);
}

QString PackageExtractor::errorString() const
{
    return wasCanceled() ? qSL("canceled") : (d->m_failed ? d->m_errorString : QString());
}

/*! \internal
  This function can be called from another thread while extract() is running
*/
void PackageExtractor::cancel()
{
    if (!d->m_canceled.fetchAndStoreOrdered(1)) {
        if (d->m_loop.isRunning())
            d->m_loop.wakeUp();
    }
}


/* * * * * * * * * * * * * * * * * * *
 *  vvv PackageExtractorPrivate vvv  *
 * * * * * * * * * * * * * * * * * * */

PackageExtractorPrivate::PackageExtractorPrivate(PackageExtractor *extractor, const QUrl &downloadUrl)
    : QObject(extractor)
    , q(extractor)
    , m_url(downloadUrl)
    , m_nam(new QNetworkAccessManager(this))
    , m_report(QString())
{
    m_buffer.resize(64 * 1024);
}

qint64 PackageExtractorPrivate::readTar(struct archive *ar, const void **archiveBuffer)
{
    forever {
        // the event loop is gone
        if (!m_loop.isRunning()) {
            archive_set_error(ar, -1, "no eventlopp");
            return -1;
        }
        // we have been canceled
        if (q->wasCanceled()) {
            archive_set_error(ar, -1, "canceled");
            return -1;
        }
        qint64 bytesAvailable = m_reply->bytesAvailable();

        // there is something to read
        // (or this is a FIFO and we need this ugly hack - for testing only though!)
        if ((bytesAvailable > 0) || m_downloadingFromFIFO) {
            qint64 bytesRead = m_reply->read(m_buffer.data(), m_buffer.size());

            if (bytesRead < 0) {
                // another FIFO hack: if the writer dies, we will get an -1 return from read()
                if (m_downloadingFromFIFO && m_reply->atEnd()) {
                    return 0;
                } else {
                    archive_set_error(ar, -1, "could not read from tar archive");
                    return -1;
                }
            }

            m_bytesReadTotal += bytesRead;
            *archiveBuffer = m_buffer.constData();

            qint64 progress = m_downloadTotal ? (100 * m_bytesReadTotal / m_downloadTotal) : 0;
            if (progress != m_lastProgress) {
                emit q->progress(qreal(progress) / 100);
                m_lastProgress = progress;
            }

            return bytesRead;
        }

        // got an error while reading
        if (m_reply->error() != QNetworkReply::NoError) {
            archive_set_error(ar, -1, "%s", m_reply->errorString().toLocal8Bit().constData());
            return -1;
        }

        // we're done
        if (m_reply->isFinished())
            return 0;

        m_loop.processEvents(QEventLoop::WaitForMoreEvents);
    }
}

void PackageExtractorPrivate::extract()
{
    struct archive *ar = 0;

    try {
        ar = archive_read_new();
        if (!ar)
            throw Exception(Error::System, "[libarchive] could not create a new archive object");
        if (archive_read_support_format_tar(ar) != ARCHIVE_OK)
            throw ArchiveException(ar, "could not enable TAR support");
// disabled for now -- see libarchive.pro
//        if (archive_read_support_filter_xz(ar) != ARCHIVE_OK)
//            throw ArchiveException(ar, "could not enable XZ support");
        if (archive_read_support_filter_gzip(ar) != ARCHIVE_OK)
            throw ArchiveException(ar, "could not enable GZIP support");
#if !defined(Q_OS_ANDROID)
        if (archive_read_set_options(ar, "hdrcharset=UTF-8") != ARCHIVE_OK)
            throw ArchiveException(ar, "could not set the HDRCHARSET option");
#endif

        auto dummyCallback = [](archive *, void *){ return ARCHIVE_OK; };
        auto readCallback = [](archive *ar, void *user, const void **buffer) { return (__LA_SSIZE_T) reinterpret_cast<PackageExtractorPrivate *>(user)->readTar(ar, buffer); };

        if (archive_read_open(ar, this, dummyCallback, readCallback, dummyCallback) != ARCHIVE_OK)
            throw ArchiveException(ar, "could not open archive");

        bool seenHeader = false;
        bool seenFooter = false;
        QByteArray header;
        QByteArray footer;

        QCryptographicHash digest(QCryptographicHash::Sha256);

        // Iterate over all entries in the archive
        for (bool finished = false; !finished; ) {
            archive_entry *entry = 0;
            QFile f;

            // Try to read the next entry from the archive

            switch (archive_read_next_header(ar, &entry)) {
            case ARCHIVE_EOF:
                finished = true;
                continue;
            case ARCHIVE_OK:
                break;
            default:
                throw ArchiveException(ar, "could not read header");
            }

            // Make sure to quit if we get something funky, i.e. something other than files or dirs

            __LA_MODE_T entryMode = archive_entry_mode(entry);
            PackageEntryType packageEntryType;
            QString entryPath = QString::fromWCharArray(archive_entry_pathname_w(entry));

            switch (entryMode & S_IFMT) {
            case S_IFREG:
                packageEntryType = PackageEntry_File;
                break;
            case S_IFDIR:
                packageEntryType = PackageEntry_Dir;
                break;
            default:
                throw Exception(Error::Package, "file %1 in the archive has the unsupported type (mode) 0%2").arg(entryPath).arg(int(entryMode & S_IFMT), 0, 8);
            }

            // Check if this entry is special (metadata vs. data)

            if (entryPath == qL1S("--PACKAGE-HEADER--"))
                packageEntryType = PackageEntry_Header;
            else if (entryPath.startsWith(qL1S("--PACKAGE-FOOTER--")))
                packageEntryType = PackageEntry_Footer;
            else if (entryPath.startsWith(qL1S("--")))
                throw Exception(Error::Package, "filename %1 in the archive starts with the reserved characters '--'").arg(entryPath);

            // The first (and only the first) file in every package needs to be --PACKAGE-HEADER--

            if (seenHeader == (packageEntryType == PackageEntry_Header))
                throw Exception(Error::Package, "the first (and only the first) file of the package must be --PACKAGE-HEADER--");

            // There cannot be and normal files after the first --PACKAGE-FOOTER--
            if (seenFooter && (packageEntryType != PackageEntry_Footer))
                throw Exception(Error::Package, "only --PACKAGE-FOOTER--* files are allowed at the end of the package");

            switch (packageEntryType) {
            case PackageEntry_Dir:
                if (!entryPath.endsWith(qL1C('/')))
                    throw Exception(Error::Package, "invalid archive entry '%1': directory name is missing '/' at the end").arg(entryPath);
                entryPath.chop(1);
                // no break;
            case PackageEntry_File: {
                // get the directory, where the new entry will be created
                QDir entryDir(QString(m_destinationPath + entryPath).section(qL1C('/'), 0, -2));
                if (!entryDir.exists())
                    throw Exception(Error::Package, "invalid archive entry '%1': parent directory is missing").arg(entryPath);

                QString entryCanonicalPath = entryDir.canonicalPath() + qL1C('/');
                QString baseCanonicalPath = QDir(m_destinationPath).canonicalPath() + qL1C('/');

                // security check: make sure that entryCanonicalPath is NOT outside of baseCanonicalPath
                if (!entryCanonicalPath.startsWith(baseCanonicalPath))
                    throw Exception(Error::Package, "invalid archive entry '%1': pointing outside of extraction directory").arg(entryPath);

                if (packageEntryType == PackageEntry_Dir) {
                    QString entryName = entryPath.section(qL1C('/'), -1, -1);

                    if ((entryName != qL1S(".")) && !entryDir.mkdir(entryName))
                        throw Exception(Error::IO, "could not create directory '%1'").arg(entryDir.filePath(entryName));

                    archive_read_data_skip(ar);

                } else { // PackageEntry_File
                    f.setFileName(m_destinationPath + entryPath);
                    if (!f.open(QFile::WriteOnly | QFile::Truncate))
                        throw Exception(f, "could not create file");

                    if (entryMode & S_IEXEC)
                        f.setPermissions(f.permissions() | QFile::ExeUser);
                }

                m_report.addFile(entryPath);
                break;
            }
            case PackageEntry_Footer:
                seenFooter = true;
                break;
            case PackageEntry_Header:
                seenHeader = true;
                break;
            default:
                archive_read_data_skip(ar);
                continue;
            }

            // Read in the entry's data (which can be a normal file or header/footer metadata)

            if (archive_entry_size(entry)) {
                __LA_INT64_T readPosition = 0;

                for (bool fileFinished = false; !fileFinished; ) {
                    const char *buffer;
                    size_t bytesRead;
                    __LA_INT64_T offset;

                    switch (archive_read_data_block(ar, reinterpret_cast<const void **>(&buffer), &bytesRead, &offset)) {
                    case ARCHIVE_EOF:
                        fileFinished = true;
                        continue;
                    case ARCHIVE_OK:
                        break;
                    default:
                        throw ArchiveException(ar, "could not read from archive");
                    }

                    if (offset != readPosition)
                        throw Exception(Error::Package, "[libarchive] current read position (%1) does not match requested offset (%2)").arg(readPosition).arg(offset);

                    readPosition += bytesRead;

                    switch (packageEntryType) {
                    case PackageEntry_File:
                        digest.addData(buffer, int(bytesRead));

                        if (!f.write(buffer, bytesRead))
                            throw Exception(f, "could not write to file");
                        break;
                    case PackageEntry_Header:
                        header.append(buffer, int(bytesRead));
                        break;
                    case PackageEntry_Footer:
                        footer.append(buffer, int(bytesRead));
                        break;
                    default:
                        break;
                    }
                }
            }

            // We finished reading an entry from the archive. Now we need to
            // post-process it, depending on its type

            switch (packageEntryType) {
            case PackageEntry_Header:
                processMetaData(header, digest, true /*header*/);
                break;
            case PackageEntry_File:
                f.close();
                // no break
            case PackageEntry_Dir: {
                // Just to be on the safe side, we also add the file's meta-data to the digest
                PackageUtilities::addFileMetadataToDigest(entryPath, QFileInfo(m_destinationPath + entryPath), digest);

                // Finally call the user's code to post-process whatever was extracted right now
                if (m_fileExtractedCallback)
                    m_fileExtractedCallback(entryPath);
                break;
            }
            default:
                break;
            }
        }

        // Finished extracting

        // We are only post-processing the footer now, because we allow for multiple --PACKAGE-FOOTER--
        // files in the archive, so we can only start processing them, when we are sure that there
        // are no more. This makes it easier for 3rd party tools like e.g. app-stores to add the required
        // signature metadata
        processMetaData(footer, digest, false /*footer*/);

        emit q->progress(1);

    } catch (const Exception &e) {
        if (!q->wasCanceled())
            setError(e.errorCode(), e.errorString());
    }

    if (ar)
        archive_read_free(ar);

    m_loop.quit();
}

void PackageExtractorPrivate::processMetaData(const QByteArray &metadata, QCryptographicHash &digest, bool isHeader) throw(Exception)
{
    QtYaml::ParseError error;
    QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(metadata, &error);

    if (error.error != QJsonParseError::NoError)
        throw Exception(Error::Package, "metadata is not a valid YAML document: %1 (line: %2, column %3)")
            .arg(error.errorString()).arg(error.line).arg(error.column);

    if ((docs.size() < 2)
        || (docs.first().toMap().value(qSL("formatType")).toString() != qL1S(isHeader ? "am-package-header" : "am-package-footer"))
        || (docs.first().toMap().value(qSL("formatVersion")).toInt(0) != 1))
        throw Exception(Error::Package, "metadata has an invalid format specification");

    QVariantMap map = docs.at(1).toMap();

    if (isHeader) {
        QString applicationId = map.value(qSL("applicationId")).toString();
        quint64 diskSpaceUsed = map.value(qSL("diskSpaceUsed")).toULongLong();

        if (applicationId.isNull() || !isValidDnsName(applicationId))
            throw Exception(Error::Package, "metadata has an invalid applicationId field (%1)").arg(applicationId);
        m_report.setApplicationId(applicationId);

        if (!diskSpaceUsed)
            throw Exception(Error::Package, "metadata has an invalid diskSpaceUsed field (%1)").arg(diskSpaceUsed);
        m_report.setDiskSpaceUsed(diskSpaceUsed);

        PackageUtilities::addImportantHeaderDataToDigest(map, digest);

    } else { // footer(s)
        for (int i = 2; i < docs.size(); ++i)
            map = map.unite(docs.at(i).toMap());

        QByteArray packageDigest = QByteArray::fromHex(map.value(qSL("digest")).toString().toLatin1());

        if (packageDigest.isEmpty())
            throw Exception(Error::Package, "metadata is missing the digest field");
        m_report.setDigest(packageDigest);

        QByteArray calculatedDigest = digest.result();
        if (calculatedDigest != packageDigest)
            throw Exception(Error::Package, "package digest mismatch (is %1, but should be %2").arg(calculatedDigest.toHex()).arg(packageDigest.toHex());

        m_report.setStoreSignature(QByteArray::fromBase64(map.value(qSL("storeSignature")).toString().toLatin1()));
        m_report.setDeveloperSignature(QByteArray::fromBase64(map.value(qSL("developerSignature")).toString().toLatin1()));
    }
}

void PackageExtractorPrivate::setError(Error errorCode, const QString &errorString)
{
    m_failed = true;

    // only the first error is the one that counts!
    if (m_errorCode == Error::None) {
        m_errorCode = errorCode;
        m_errorString = errorString;
    }
}


void PackageExtractorPrivate::download(const QUrl &url)
{
    QNetworkRequest request(url);
    m_reply = m_nam->get(request);

#if defined(Q_OS_UNIX)
    // This is an ugly hack, but it allows us to use FIFOs in the unit tests.
    // (the problem being, that bytesAvailable() on a QFile wrapping a FIFO will always return 0)

    if (url.isLocalFile()) {
        struct stat statBuffer;
        if (stat(url.toLocalFile().toLocal8Bit(), &statBuffer) == 0) {
            if (S_ISFIFO(statBuffer.st_mode)) {
                m_downloadingFromFIFO = true;
            }
        }
    }
#endif

    connect(m_reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
            this, &PackageExtractorPrivate::networkError);
    connect(m_reply, &QNetworkReply::metaDataChanged,
            this, &PackageExtractorPrivate::handleRedirect);
    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &PackageExtractorPrivate::downloadProgressChanged);
}

void PackageExtractorPrivate::networkError(QNetworkReply::NetworkError)
{
    setError(Error::Network, qobject_cast<QNetworkReply *>(sender())->errorString());
    QMetaObject::invokeMethod(&m_loop, "quit", Qt::QueuedConnection);
}

void PackageExtractorPrivate::handleRedirect()
{
    int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if ((status >= 300) && (status < 400)) {
        QUrl url = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        m_reply->disconnect();
        m_reply->deleteLater();
        QNetworkRequest request(url);
        m_reply = m_nam->get(request);
    }
}

void PackageExtractorPrivate::downloadProgressChanged(qint64 downloaded, qint64 total)
{
    Q_UNUSED(downloaded)
    m_downloadTotal = total;
}

QT_END_NAMESPACE_AM
