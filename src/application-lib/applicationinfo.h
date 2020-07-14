/****************************************************************************
**
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

#include <QDir>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QDataStream)

QT_BEGIN_NAMESPACE_AM

class PackageInfo;

class ApplicationInfo
{
public:
    ApplicationInfo(PackageInfo *packageInfo);

    static const quint32 DataStreamVersion;

    PackageInfo *packageInfo() const;

    QVariantMap toVariantMap() const;
    QString id() const;
    int uniqueNumber() const;
    QVariantMap applicationProperties() const;
    QVariantMap allAppProperties() const;
    QString absoluteCodeFilePath() const;
    QString codeFilePath() const;
    QString runtimeName() const;
    QVariantMap runtimeParameters() const;
    QStringList capabilities() const;
    QStringList supportedMimeTypes() const;
    QString documentUrl() const;
    QVariantMap openGLConfiguration() const;
    bool supportsApplicationInterface() const;
    QVariantMap dltConfiguration() const;

    void writeToDataStream(QDataStream &ds) const;
    static ApplicationInfo *readFromDataStream(PackageInfo *pkg, QDataStream &ds);

private:
    void read(QDataStream &ds);

    // static part from info.json
    PackageInfo *m_packageInfo;

    QString m_id;
    int m_uniqueNumber;

    QVariantMap m_sysAppProperties;
    QVariantMap m_allAppProperties;

    QString m_codeFilePath; // relative to info.json location
    QString m_runtimeName;
    QVariantMap m_runtimeParameters;
    bool m_supportsApplicationInterface = false;
    QStringList m_capabilities;
    QVariantMap m_openGLConfiguration;
    QStringList m_supportedMimeTypes; // deprecated
    QString m_documentUrl; // deprecated
    QVariantMap m_dltConfiguration;

    friend class ApplicationManager; // needed to update installation status
    friend class PackageDatabase; // needed to create ApplicationInfo objects
    friend class InstallationTask; // needed to set m_uid and m_builtin during the installation

    friend class YamlPackageScanner;
    Q_DISABLE_COPY(ApplicationInfo)
};

QT_END_NAMESPACE_AM
