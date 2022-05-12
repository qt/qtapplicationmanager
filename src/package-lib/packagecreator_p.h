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
