// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QStringList>
#include <QUrl>
#include <QDir>
#include <QByteArrayList>


class PSConfiguration
{
public:
    PSConfiguration() = default;
    PSConfiguration(const PSConfiguration &) = delete;

    QDir dataDirectory;
    QUrl listenUrl;

    QString projectId;

    QString storeSignPassword;
    QByteArray storeSignCertificate;
    QByteArrayList developerVerificationCaCertificates;

    static const QString DefaultListenAddress;
    static const QString DefaultConfigFile;
    static const QString DefaultProjectId;

public:
    void parse(const QStringList &args);

private:
    QString listenAddress;
    QString storeSignCertificateFile;
    QStringList developerVerificationCaCertificateFiles;
};
