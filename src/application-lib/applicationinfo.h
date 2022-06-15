// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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

    QStringList categories() const;

    QMap<QString, QString> names() const;
    QMap<QString, QString> descriptions() const;
    QString icon() const;

    void writeToDataStream(QDataStream &ds) const;
    static ApplicationInfo *readFromDataStream(PackageInfo *pkg, QDataStream &ds);

private:
    void read(QDataStream &ds);

    // static part from the manifest
    PackageInfo *m_packageInfo;

    QString m_id;
    int m_uniqueNumber;

    QVariantMap m_sysAppProperties;
    QVariantMap m_allAppProperties;

    QString m_codeFilePath; // relative to the manifest's location
    QString m_runtimeName;
    QVariantMap m_runtimeParameters;
    bool m_supportsApplicationInterface = false;
    QStringList m_capabilities;
    QVariantMap m_openGLConfiguration;
    QStringList m_supportedMimeTypes; // deprecated
    QString m_documentUrl; // deprecated
    QVariantMap m_dltConfiguration;

    QStringList m_categories;
    QMap<QString, QString> m_names; // language -> name
    QMap<QString, QString> m_descriptions; // language -> description
    QString m_icon; // relative to the manifest's location

    friend class ApplicationManager; // needed to update installation status
    friend class PackageDatabase; // needed to create ApplicationInfo objects
    friend class InstallationTask; // needed to set m_uid and m_builtin during the installation

    friend class YamlPackageScanner;
    Q_DISABLE_COPY(ApplicationInfo)
};

QT_END_NAMESPACE_AM
