/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QObject>

#include <functional>

#include <QtAppManCommon/error.h>

QT_FORWARD_DECLARE_CLASS(QUrl)
QT_FORWARD_DECLARE_CLASS(QDir)

QT_BEGIN_NAMESPACE_AM

class PackageExtractorPrivate;
class InstallationReport;


class PackageExtractor : public QObject
{
    Q_OBJECT

public:
    PackageExtractor(const QUrl &downloadUrl, const QDir &destinationDir, QObject *parent = nullptr);

    QDir destinationDirectory() const;
    void setDestinationDirectory(const QDir &destinationDir);

    void setFileExtractedCallback(const std::function<void(const QString &)> &callback);

    bool extract();

    const InstallationReport &installationReport() const;

    bool hasFailed() const;
    bool wasCanceled() const;

    Error errorCode() const;
    QString errorString() const;

public slots:
    void cancel();

signals:
    void progress(qreal progress);

private:
    PackageExtractorPrivate *d;

    friend class PackageExtractorPrivate;
};

QT_END_NAMESPACE_AM
