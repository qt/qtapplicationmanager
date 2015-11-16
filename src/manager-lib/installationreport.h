/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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
****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>
#include <QByteArray>

QT_FORWARD_DECLARE_CLASS(QIODevice)

class InstallationReport
{
public:
    InstallationReport(const QString &applicationId = QString());

    QString applicationId() const;
    void setApplicationId(const QString &applicationId);

    QString installationLocationId() const;
    void setInstallationLocationId(const QString &installationLocationId);

    QByteArray digest() const;
    void setDigest(const QByteArray &sha1);

    quint64 diskSpaceUsed() const;
    void setDiskSpaceUsed(quint64 diskSpaceUsed);

    QByteArray developerSignature() const;
    void setDeveloperSignature(const QByteArray &developerSignature);

    QByteArray storeSignature() const;
    void setStoreSignature(const QByteArray &storeSignature);

    QStringList files() const;
    void addFile(const QString &file);
    void addFiles(const QStringList &files);

    bool isValid() const;

    bool deserialize(QIODevice *from);
    bool serialize(QIODevice *to) const;

private:
    QString m_applicationId;
    QString m_installationLocationId;
    QByteArray m_digest;
    quint64 m_diskSpaceUsed = 0;
    QStringList m_files;
    QByteArray m_developerSignature;
    QByteArray m_storeSignature;
};
