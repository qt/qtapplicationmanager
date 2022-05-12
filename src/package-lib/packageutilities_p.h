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

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/exception.h>
#include <QVariantMap>

struct archive;
QT_FORWARD_DECLARE_CLASS(QFileInfo)
QT_FORWARD_DECLARE_CLASS(QCryptographicHash)

QT_BEGIN_NAMESPACE_AM

namespace PackageUtilities
{
void addFileMetadataToDigest(const QString &entryFilePath, const QFileInfo &fi, QCryptographicHash &digest);
void addHeaderDataToDigest(const QVariantMap &header, QCryptographicHash &digest) Q_DECL_NOEXCEPT_EXPR(false);

// key == field name, value == type to choose correct hashing algorithm
extern QVariantMap headerDataForDigest;
};

enum PackageEntryType {
    PackageEntry_Header,
    PackageEntry_File,
    PackageEntry_Dir,
    PackageEntry_Footer
};

class ArchiveException : public Exception
{
public:
    ArchiveException(struct ::archive *ar, const char *errorString);
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.
