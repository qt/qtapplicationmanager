// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDataStream>
#include <QBuffer>

#include "applicationinfo.h"
#include "exception.h"
#include "installationreport.h"
#include "packageinfo.h"
#include "utilities.h"

#include <memory>

QT_BEGIN_NAMESPACE_AM

//TODO Make this really unique
static int uniqueCounter = 0;
static int nextUniqueNumber() {
    uniqueCounter++;
    if (uniqueCounter > 999)
        uniqueCounter = 0;

    return uniqueCounter;
}


ApplicationInfo::ApplicationInfo(PackageInfo *packageInfo)
    : m_packageInfo(packageInfo)
    , m_uniqueNumber(nextUniqueNumber())
{ }

PackageInfo *ApplicationInfo::packageInfo() const
{
    return m_packageInfo;
}

QString ApplicationInfo::id() const
{
    return m_id;
}

int ApplicationInfo::uniqueNumber() const
{
    return m_uniqueNumber;
}

QVariantMap ApplicationInfo::applicationProperties() const
{
    return m_sysAppProperties;
}

QVariantMap ApplicationInfo::allAppProperties() const
{
    return m_allAppProperties;
}


const quint32 ApplicationInfo::DataStreamVersion = 4;


void ApplicationInfo::writeToDataStream(QDataStream &ds) const
{
    //NOTE: increment DataStreamVersion above, if you make any changes here

    ds << m_id
       << m_uniqueNumber
       << m_sysAppProperties
       << m_allAppProperties
       << m_codeFilePath
       << m_runtimeName
       << m_runtimeParameters
       << m_supportsApplicationInterface
       << m_capabilities
       << m_openGLConfiguration
       << m_dltConfiguration
       << m_supportedMimeTypes
       << m_categories
       << m_names
       << m_descriptions
       << m_icon;
}

ApplicationInfo *ApplicationInfo::readFromDataStream(PackageInfo *pkg, QDataStream &ds)
{
    //NOTE: increment DataStreamVersion above, if you make any changes here

    auto app = std::make_unique<ApplicationInfo>(pkg);

    ds >> app->m_id
       >> app->m_uniqueNumber
       >> app->m_sysAppProperties
       >> app->m_allAppProperties
       >> app->m_codeFilePath
       >> app->m_runtimeName
       >> app->m_runtimeParameters
       >> app->m_supportsApplicationInterface
       >> app->m_capabilities
       >> app->m_openGLConfiguration
       >> app->m_dltConfiguration
       >> app->m_supportedMimeTypes
       >> app->m_categories
       >> app->m_names
       >> app->m_descriptions
       >> app->m_icon;

    uniqueCounter = qMax(uniqueCounter, app->m_uniqueNumber);
    app->m_capabilities.sort();

    return app.release();
}


QVariantMap ApplicationInfo::toVariantMap() const
{
    QVariantMap map;
    //TODO: check if we can find a better method to keep this as similar as possible to
    //      ApplicationManager::get().
    //      This is used for RuntimeInterface::startApplication() and the ContainerInterface

    map[qSL("id")] = m_id;
    map[qSL("uniqueNumber")] = m_uniqueNumber;

    {
        QVariantMap displayName;
        const auto n = names();
        for (auto it = n.constBegin(); it != n.constEnd(); ++it)
            displayName.insert(it.key(), it.value());
        map[qSL("displayName")] = displayName;
    }

    map[qSL("displayIcon")] = icon();
    map[qSL("applicationProperties")] = m_allAppProperties;
    map[qSL("codeFilePath")] = m_codeFilePath;
    map[qSL("runtimeName")] = m_runtimeName;
    map[qSL("runtimeParameters")] = m_runtimeParameters;
    map[qSL("capabilities")] = m_capabilities;
    map[qSL("mimeTypes")] = m_supportedMimeTypes;

    map[qSL("categories")] = categories();
    map[qSL("version")] = packageInfo()->version();
    map[qSL("baseDir")] = packageInfo()->baseDir().absolutePath();
    map[qSL("codeDir")] = map[qSL("baseDir")];     // 5.12 backward compatibility
    map[qSL("manifestDir")] = map[qSL("baseDir")]; // 5.12 backward compatibility
    map[qSL("installationLocationId")] = packageInfo()->installationReport() ? qSL("internal-0") : QString();
    map[qSL("supportsApplicationInterface")] = m_supportsApplicationInterface;
    map[qSL("dlt")] = m_dltConfiguration;

    return map;
}

QString ApplicationInfo::absoluteCodeFilePath() const
{
    return toAbsoluteFilePath(m_codeFilePath, packageInfo()->baseDir().path());
}

QString ApplicationInfo::codeFilePath() const
{
    return m_codeFilePath;
}

QString ApplicationInfo::runtimeName() const
{
    return m_runtimeName;
}

QVariantMap ApplicationInfo::runtimeParameters() const
{
    return m_runtimeParameters;
}

QStringList ApplicationInfo::capabilities() const
{
    return m_capabilities;
}

QStringList ApplicationInfo::supportedMimeTypes() const
{
    return m_supportedMimeTypes;
}

QString ApplicationInfo::documentUrl() const
{
    return m_documentUrl;
}

QVariantMap ApplicationInfo::openGLConfiguration() const
{
    return m_openGLConfiguration;
}

bool ApplicationInfo::supportsApplicationInterface() const
{
    return m_supportsApplicationInterface;
}

QVariantMap ApplicationInfo::dltConfiguration() const
{
    return m_dltConfiguration;
}

QStringList ApplicationInfo::categories() const
{
    return m_categories.isEmpty() ? m_packageInfo->categories() : m_categories;
}

QMap<QString, QString> ApplicationInfo::names() const
{
    return m_names.isEmpty() ? m_packageInfo->names() : m_names;
}

QMap<QString, QString> ApplicationInfo::descriptions() const
{
    return m_descriptions.isEmpty() ? m_packageInfo->descriptions() : m_descriptions;
}

QString ApplicationInfo::icon() const
{
    return m_icon.isEmpty() ? m_packageInfo->icon() : m_icon;
}

QT_END_NAMESPACE_AM
