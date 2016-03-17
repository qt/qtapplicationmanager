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

#include "packagecreator.h"

#include <archive.h>


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

    friend class PackageCreator;
};
