// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QDir>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>

#include <memory>

#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QDataStream)

class tst_Application;

QT_BEGIN_NAMESPACE_AM

class InstallationReport;
class IntentInfo;
class ApplicationInfo;
class YamlPackageScanner;

class PackageInfo
{
public:
    ~PackageInfo();

    static quint32 dataStreamVersion();

    void validate() const Q_DECL_NOEXCEPT_EXPR(false);

    QString id() const;

    QMap<QString, QString> names() const;
    QMap<QString, QString> descriptions() const;
    QString icon() const;
    QStringList categories() const;

    bool isBuiltIn() const;
    void setBuiltIn(bool builtIn);
    QString version() const;
    uint uid() const { return m_uid; }

    const QDir &baseDir() const;
    void setBaseDir(const QDir &dir);

    QVector<ApplicationInfo *> applications() const;
    QVector<IntentInfo *> intents() const;

    const InstallationReport *installationReport() const;
    void setInstallationReport(InstallationReport *report);

    void writeToDataStream(QDataStream &ds) const;
    static PackageInfo *readFromDataStream(QDataStream &ds);

    static bool isValidApplicationId(const QString &appId, QString *errorString = nullptr);

    QString manifestPath() const;

    static PackageInfo *fromManifest(const QString &manifestPath);

private:
    PackageInfo();

    QString m_manifestName;
    QString m_id;
    QMap<QString, QString> m_names; // language -> name
    QMap<QString, QString> m_descriptions; // language -> description
    QStringList m_categories;
    QString m_icon; // relative to the manifest's location
    QString m_version;
    bool m_builtIn = false; // system package - not removable
    uint m_uid = uint(-1); // unix user id - move to installationReport
    QVector<ApplicationInfo *> m_applications;
    QVector<IntentInfo *> m_intents;

    // added by installer
    std::unique_ptr<InstallationReport> m_installationReport;
    QDir m_baseDir;

    friend class YamlPackageScanner;
    friend class InstallationTask;
    Q_DISABLE_COPY(PackageInfo)
};

QT_END_NAMESPACE_AM
