/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
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
#include <QVector>
#include <QString>

#include <QtAppManApplication/packageinfo.h>

QT_BEGIN_NAMESPACE_AM

class PackageInfo;


class PackageDatabase
{
public:
    PackageDatabase(const QStringList &builtInPackagesDirs, const QString &installedPackagesDir = QString());
    PackageDatabase(const QString &singlePackagePath);
    ~PackageDatabase();

    QString installedPackagesDir() const;

    void enableLoadFromCache();
    void enableSaveToCache();

    void parse();

    QVector<PackageInfo *> builtInPackages() const;
    QVector<PackageInfo *> installedPackages() const;

    // runtime installations
    void addPackageInfo(PackageInfo *package);
    void removePackageInfo(PackageInfo *package);

private:
    Q_DISABLE_COPY(PackageDatabase)

    bool builtInHasRemovableUpdate(PackageInfo *packageInfo) const;
    QStringList findManifestsInDir(const QDir &manifestDir, bool scanningBuiltInApps);

    bool m_loadFromCache = false;
    bool m_saveToCache = false;
    bool m_parsed = false;
    QStringList m_builtInPackagesDirs;
    QString m_installedPackagesDir;
    QString m_singlePackagePath;

    QVector<PackageInfo *> m_builtInPackages;
    QVector<PackageInfo *> m_installedPackages;

};

QT_END_NAMESPACE_AM
