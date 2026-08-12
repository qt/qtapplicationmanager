// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:data-parser

#include "packageinfo.h"
#include "applicationinfo.h"
#include "intentinfo.h"
#include "exception.h"
#include "utilities.h"
#include "installationreport.h"
#include "yamlpackagescanner.h"

#include <memory>

QT_BEGIN_NAMESPACE_AM


PackageInfo::PackageInfo()
{ }

PackageInfo::~PackageInfo()
{
    qDeleteAll(m_intents);
    qDeleteAll(m_applications);
}

void PackageInfo::validate() const noexcept(false)
{
    QString errorMsg;
    if (!isValidApplicationId(id(), &errorMsg))
        throw Exception("the identifier (%1) is not a valid package-id: %2").arg(id()).arg(errorMsg);

    if (m_applications.isEmpty())
        throw Exception("package contains no applications");

    for (const auto &app : m_applications) {
        if (!isValidApplicationId(app->id(), &errorMsg))
            throw Exception("the identifier (%1) is not a valid application-id: %2").arg(app->id()).arg(errorMsg);

        if (app->codeFilePath().isEmpty())
            throw Exception("the 'code' field must not be empty on application %1").arg(app->id());

        if (app->runtimeName().isEmpty())
            throw Exception("the 'runtimeName' field must not be empty on application %1").arg(app->id());
    }
}

QString PackageInfo::id() const
{
    return m_id;
}

QMap<QString, QString> PackageInfo::names() const
{
    return m_names;
}

QMap<QString, QString> PackageInfo::descriptions() const
{
    return m_descriptions;
}

QString PackageInfo::icon() const
{
    return m_icon;
}

QStringList PackageInfo::categories() const
{
    return m_categories;
}

bool PackageInfo::isBuiltIn() const
{
    return m_builtIn;
}

void PackageInfo::setBuiltIn(bool builtIn)
{
    m_builtIn = builtIn;
}

QString PackageInfo::version() const
{
    return m_version;
}

const QDir &PackageInfo::baseDir() const

{
    return m_baseDir;
}

void PackageInfo::setBaseDir(const QDir &dir)
{
    m_baseDir = dir; // clazy:exclude=qt6-deprecated-api-fixes
}

QVector<ApplicationInfo *> PackageInfo::applications() const
{
    return m_applications;
}

QVector<IntentInfo *> PackageInfo::intents() const
{
    return m_intents;
}

const InstallationReport *PackageInfo::installationReport() const
{
    return m_installationReport.get();
}

void PackageInfo::setInstallationReport(InstallationReport *report)
{
    m_installationReport.reset(report);
}

bool PackageInfo::isValidApplicationId(const QString &appId, QString *errorString)
{
    try {
        validateIdForFilesystemUsage(appId);
        return true;
    } catch (const Exception &e) {
        if (errorString)
            *errorString = e.errorString();
        return false;
    }
}

QString PackageInfo::manifestPath() const
{
    return m_baseDir.filePath(m_manifestName);
}

PackageInfo *PackageInfo::fromManifest(const QString &manifestPath)
{
    return YamlPackageScanner().scan(manifestPath);
}

QT_END_NAMESPACE_AM
