// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PACKAGEEXTRACTOR_H
#define PACKAGEEXTRACTOR_H

#include <QtCore/QObject>

#include <functional>

#include <QtAppManPackage/qtappmanpackageglobal.h>

QT_FORWARD_DECLARE_CLASS(QUrl)
QT_FORWARD_DECLARE_CLASS(QDir)

QT_BEGIN_NAMESPACE_AM

class PackageExtractorPrivate;
class InstallationReport;


class Q_APPMANPACKAGE_EXPORT PackageExtractor : public QObject
{
    Q_OBJECT

public:
    PackageExtractor(const QUrl &downloadUrl, const QDir &destinationDir, QObject *parent = nullptr);

    QDir destinationDirectory() const;
    void setDestinationDirectory(const QDir &destinationDir);

    void setFileExtractedCallback(const std::function<void(const QString &)> &callback);
    void setExtendedAttributeCallback(const std::function<void(const QString &, QByteArrayView, QByteArrayView)> &callback);

    bool extract();

    const InstallationReport &installationReport() const;

    bool hasFailed() const;
    bool wasCanceled() const;

    QString errorString() const;

public Q_SLOTS:
    void cancel();

Q_SIGNALS:
    void progress(qreal progress);

private:
    PackageExtractorPrivate *d;

    friend class PackageExtractorPrivate;
};

QT_END_NAMESPACE_AM

#endif // PACKAGEEXTRACTOR_H
