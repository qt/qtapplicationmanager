// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDataStream>
#include <QBuffer>

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
        throw Exception(Error::Parse, "the identifier (%1) is not a valid package-id: %2").arg(id()).arg(errorMsg);

    if (m_applications.isEmpty())
        throw Exception(Error::Parse, "package contains no applications");

    for (const auto &app : m_applications) {
        if (!isValidApplicationId(app->id(), &errorMsg))
            throw Exception(Error::Parse, "the identifier (%1) is not a valid application-id: %2").arg(app->id()).arg(errorMsg);

        if (app->codeFilePath().isEmpty())
            throw Exception(Error::Parse, "the 'code' field must not be empty on application %1").arg(app->id());

        if (app->runtimeName().isEmpty())
            throw Exception(Error::Parse, "the 'runtimeName' field must not be empty on application %1").arg(app->id());
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
    m_baseDir = dir;
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

quint32 PackageInfo::dataStreamVersion()
{
    return 3
           + (ApplicationInfo::dataStreamVersion() << 8)
           + (IntentInfo::dataStreamVersion() << 16);
}

void PackageInfo::writeToDataStream(QDataStream &ds) const
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    QByteArray serializedReport;

    if (auto report = installationReport()) {
        QBuffer buffer(&serializedReport);
        buffer.open(QBuffer::WriteOnly);
        report->serialize(&buffer);
    }

    ds << m_id
       << m_names
       << m_icon
       << m_descriptions
       << m_categories
       << m_version
       << m_builtIn
       << m_baseDir.absolutePath()
       << serializedReport;

    ds << int(m_applications.size());
    for (const auto &app : m_applications)
        app->writeToDataStream(ds);

    ds << int(m_intents.size());
    for (const auto &intent : m_intents)
        intent->writeToDataStream(ds);
}

PackageInfo *PackageInfo::readFromDataStream(QDataStream &ds)
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    std::unique_ptr<PackageInfo> pkg(new PackageInfo);

    QString baseDir;
    QByteArray serializedReport;

    ds >> pkg->m_id
       >> pkg->m_names
       >> pkg->m_icon
       >> pkg->m_descriptions
       >> pkg->m_categories
       >> pkg->m_version
       >> pkg->m_builtIn
       >> baseDir
       >> serializedReport;

    pkg->m_baseDir.setPath(baseDir);

    if (!serializedReport.isEmpty()) {
        QBuffer buffer(&serializedReport);
        buffer.open(QBuffer::ReadOnly);
        pkg->m_installationReport = std::make_unique<InstallationReport>(pkg->id());
        try {
            pkg->m_installationReport->deserialize(&buffer);
        } catch (...) {
            return nullptr;
        }
    }

    int applicationsSize = 0;
    ds >> applicationsSize;
    while (--applicationsSize >= 0) {
        if (auto app = ApplicationInfo::readFromDataStream(pkg.get(), ds))
            pkg->m_applications << app;
        else
            return nullptr;
    }

    int intentsSize = 0;
    ds >> intentsSize;
    while (--intentsSize >= 0) {
        if (auto intent = IntentInfo::readFromDataStream(pkg.get(), ds))
            pkg->m_intents << intent;
        else
            return nullptr;
    }

    return pkg.release();
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
