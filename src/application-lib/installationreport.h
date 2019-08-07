/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVariantMap>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QIODevice)

QT_BEGIN_NAMESPACE_AM

class InstallationReport
{
public:
    InstallationReport(const QString &packageId = QString());

    QString packageId() const;
    void setPackageId(const QString &packageId);

    QVariantMap extraMetaData() const;
    void setExtraMetaData(const QVariantMap &extraMetaData);
    QVariantMap extraSignedMetaData() const;
    void setExtraSignedMetaData(const QVariantMap &extraSignedMetaData);

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

    void deserialize(QIODevice *from);
    bool serialize(QIODevice *to) const;

private:
    QString m_packageId;
    QByteArray m_digest;
    quint64 m_diskSpaceUsed = 0;
    QStringList m_files;
    QByteArray m_developerSignature;
    QByteArray m_storeSignature;
    QVariantMap m_extraMetaData;
    QVariantMap m_extraSignedMetaData;
};

QT_END_NAMESPACE_AM
