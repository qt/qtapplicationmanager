// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManPackage/packagecreator.h>

#include <archive.h>

QT_BEGIN_NAMESPACE_AM

class PackageCreatorPrivate
{
public:
    PackageCreatorPrivate(PackageCreator *creator, QIODevice *output, const InstallationReport &report);

    bool create();

private:
    bool addVirtualFile(struct archive *ar, const QString &filename, const QByteArray &data);
    void setError(Error errorCode, const QString &errorString);

private:
    PackageCreator *q;

    QIODevice *m_output;
    QString m_sourcePath;
    bool m_failed = false;
    QAtomicInt m_canceled;
    Error m_errorCode = Error::None;
    QString m_errorString;

    QByteArray m_digest;
    const InstallationReport &m_report;
    QVariantMap m_metaData;

    friend class PackageCreator;
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
