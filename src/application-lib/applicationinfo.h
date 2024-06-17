// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef APPLICATIONINFO_H
#define APPLICATIONINFO_H

#include <QtCore/QDir>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/openglconfiguration.h>
#include <QtAppManCommon/watchdogconfiguration.h>

QT_FORWARD_DECLARE_CLASS(QDataStream)

QT_BEGIN_NAMESPACE_AM

class PackageInfo;

class ApplicationInfo
{
public:
    ApplicationInfo(PackageInfo *packageInfo);
    ~ApplicationInfo() = default;

    static quint32 dataStreamVersion();

    PackageInfo *packageInfo() const;

    QVariantMap toVariantMap() const;
    QString id() const;
    QVariantMap applicationProperties() const;
    QVariantMap allAppProperties() const;
    QString absoluteCodeFilePath() const;
    QString codeFilePath() const;
    QString runtimeName() const;
    QVariantMap runtimeParameters() const;
    QStringList capabilities() const;
    QStringList supportedMimeTypes() const;
    QString documentUrl() const;
    OpenGLConfiguration openGLConfiguration() const;
    WatchdogConfiguration watchdogConfiguration() const;
    bool supportsApplicationInterface() const;
    QString dltId() const;
    QString dltDescription() const;

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

    QVariantMap m_sysAppProperties;
    QVariantMap m_allAppProperties;

    QString m_codeFilePath; // relative to the manifest's location
    QString m_runtimeName;
    QVariantMap m_runtimeParameters;
    bool m_supportsApplicationInterface = false;
    QStringList m_capabilities;
    OpenGLConfiguration m_openGLConfiguration;
    WatchdogConfiguration m_watchdogConfiguration;
    QStringList m_supportedMimeTypes; // deprecated
    QString m_documentUrl; // deprecated
    QString m_dltId;
    QString m_dltDescription;

    QStringList m_categories;
    QMap<QString, QString> m_names; // language -> name
    QMap<QString, QString> m_descriptions; // language -> description
    QString m_icon; // relative to the manifest's location

    friend class ApplicationManager; // needed to update installation status
    friend class PackageDatabase; // needed to create ApplicationInfo objects
    friend class InstallationTask; // needed to set m_uid and m_builtin during the installation

    friend class YamlPackageScanner;
    Q_DISABLE_COPY_MOVE(ApplicationInfo)
};

QT_END_NAMESPACE_AM

#endif // APPLICATIONINFO_H
