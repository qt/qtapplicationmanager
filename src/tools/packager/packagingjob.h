/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

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

    void execute() Q_DECL_NOEXCEPT_EXPR(false);

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
