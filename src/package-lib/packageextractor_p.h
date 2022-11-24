/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
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

#include <QObject>
#include <QNetworkReply>
#include <QEventLoop>

#include <archive.h>

#include <QtAppManPackage/packageextractor.h>
#include <QtAppManApplication/installationreport.h>

QT_FORWARD_DECLARE_CLASS(QCryptographicHash)

QT_BEGIN_NAMESPACE_AM


class PackageExtractorPrivate : public QObject
{
    Q_OBJECT

public:
    PackageExtractorPrivate(PackageExtractor *extractor, const QUrl &downloadUrl);

    Q_INVOKABLE void extract();

    void download(const QUrl &url);

private slots:
    void networkError(QNetworkReply::NetworkError);
    void handleRedirect();
    void downloadProgressChanged(qint64 downloaded, qint64 total);

private:
    void setError(Error errorCode, const QString &errorString);
    qint64 readTar(struct archive *ar, const void **archiveBuffer);
    void processMetaData(const QByteArray &metadata, QCryptographicHash &digest, bool isHeader) Q_DECL_NOEXCEPT_EXPR(false);

private:
    PackageExtractor *q;

    QUrl m_url;
    QString m_destinationPath;
    std::function<void(const QString &)> m_fileExtractedCallback;
    bool m_failed = false;
    QAtomicInt m_canceled;
    Error m_errorCode = Error::None;
    QString m_errorString;

    QEventLoop m_loop;
    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
    bool m_downloadingFromFIFO = false;
    QByteArray m_buffer;
    InstallationReport m_report;

    qint64 m_downloadTotal = 0;
    qint64 m_bytesReadTotal = 0;
    qint64 m_lastProgress = 0;

    friend class PackageExtractor;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
