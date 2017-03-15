/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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

class Packager
{
public:
    static Packager *create(const QString &destinationName, const QString &sourceDir);

    static Packager *developerSign(const QString &sourceName, const QString &destinationName,
                                   const QString &certificateFile, const QString &passPhrase);
    static Packager *developerVerify(const QString &sourceName, const QStringList &certificateFiles);

    static Packager *storeSign(const QString &sourceName, const QString &destinationName,
                               const QString &certificateFile, const QString &passPhrase,
                               const QString &hardwareId);
    static Packager *storeVerify(const QString &sourceName, const QStringList &certificateFiles,
                                 const QString &hardwareId);

    void execute() Q_DECL_NOEXCEPT_EXPR(false);

    QByteArray packageDigest() const;
    QString output() const;
    int resultCode() const;

private:
    Packager();

    enum Mode {
        Create,
        DeveloperSign,
        DeveloperVerify,
        StoreSign,
        StoreVerify,
    };

    Mode m_mode;
    QByteArray m_digest;
    QString m_error;
    QString m_output;
    int m_resultCode = 0;

    QString m_sourceName;
    QString m_destinationName; // create and signing only
    QString m_sourceDir; // create only
    QStringList m_certificateFiles;
    QString m_passphrase;  // sign only
    QString m_hardwareId; // store sign/verify only
};

