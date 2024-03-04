// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef PACKAGINGJOB_H
#define PACKAGINGJOB_H

#include <QtAppManCommon/global.h>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class PackagingJob
{
public:
    static PackagingJob *create(const QString &destinationName, const QString &sourceDir,
                                const QVariantMap &extraMetaData = QVariantMap(),
                                const QVariantMap &extraSignedMetaData = QVariantMap(),
                                bool asJson = false);

    static PackagingJob *developerSign(const QString &sourceName, const QString &destinationName,
                                       const QString &certificateFile, const QString &passPhrase,
                                       bool asJson = false);
    static PackagingJob *developerVerify(const QString &sourceName, const QStringList &certificateFiles);

    static PackagingJob *storeSign(const QString &sourceName, const QString &destinationName,
                                   const QString &certificateFile, const QString &passPhrase,
                                   const QString &hardwareId, bool asJson = false);
    static PackagingJob *storeVerify(const QString &sourceName, const QStringList &certificateFiles,
                                     const QString &hardwareId);

    void execute() noexcept(false);

    QString output() const;
    int resultCode() const;

private:
    PackagingJob();

    enum Mode {
        Create,
        DeveloperSign,
        DeveloperVerify,
        StoreSign,
        StoreVerify,
    };

    Mode m_mode;
    QString m_output;
    int m_resultCode = 0;
    bool m_asJson = false;

    QString m_sourceName;
    QString m_destinationName; // create and signing only
    QString m_sourceDir; // create only
    QStringList m_certificateFiles;
    QString m_passphrase;  // sign only
    QString m_hardwareId; // store sign/verify only
    QVariantMap m_extraMetaData;
    QVariantMap m_extraSignedMetaData;
};

#endif // PACKAGINGJOB_H
