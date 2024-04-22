// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PACKAGEEXTRACTOR_P_H
#define PACKAGEEXTRACTOR_P_H

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

private Q_SLOTS:
    void networkError(QNetworkReply::NetworkError);
    void handleRedirect();
    void downloadProgressChanged(qint64 downloaded, qint64 total);

private:
    void setError(Error errorCode, const QString &errorString);
    qint64 readTar(struct archive *ar, const void **archiveBuffer);
    void processMetaData(const QByteArray &metadata, QCryptographicHash &digest, bool isHeader) noexcept(false);

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

#endif // PACKAGEEXTRACTOR_P_H
