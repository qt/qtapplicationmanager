/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QByteArray>
#include <QString>

#include "exception.h"

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

    void execute() throw (Exception);

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

