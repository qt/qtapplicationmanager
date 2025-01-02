// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>

#include <QtAppManCommon/error.h>

QT_FORWARD_DECLARE_CLASS(QIODevice)
QT_FORWARD_DECLARE_CLASS(QDir)

QT_BEGIN_NAMESPACE_AM

class PackageCreatorPrivate;
class InstallationReport;


class PackageCreator : public QObject
{
    Q_OBJECT

public:
    PackageCreator(const QDir &sourceDir, QIODevice *output, const InstallationReport &report,
                   QObject *parent = nullptr);
    ~PackageCreator();

    QDir sourceDirectory() const;
    void setSourceDirectory(const QDir &sourceDir);

    bool create();

    QByteArray createdDigest() const;
    QVariantMap metaData() const;

    bool hasFailed() const;
    bool wasCanceled() const;

    Error errorCode() const;
    QString errorString() const;

public slots:
    void cancel();

signals:
    void progress(qreal progress);

private:
    PackageCreatorPrivate *d;

    friend class PackageCreatorPrivate;
};

QT_END_NAMESPACE_AM
