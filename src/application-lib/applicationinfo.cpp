// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QDataStream>

#include "applicationinfo.h"
#include "packageinfo.h"
#include "utilities.h"

#include <QCoreApplication>
#include <QThread>
#include <memory>

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

ApplicationInfo::ApplicationInfo(PackageInfo *packageInfo)
    : m_packageInfo(packageInfo)
{
    static QAtomicInteger<uint> dltAppId; // the package scanner is multi-threaded
    m_dltId = QString::asprintf("A%03u", (++dltAppId) % 1000);
}

PackageInfo *ApplicationInfo::packageInfo() const
{
    return m_packageInfo;
}

QString ApplicationInfo::id() const
{
    return m_id;
}

QVariantMap ApplicationInfo::applicationProperties() const
{
    return m_sysAppProperties;
}

QVariantMap ApplicationInfo::allAppProperties() const
{
    return m_allAppProperties;
}

quint32 ApplicationInfo::dataStreamVersion()
{
    return 6;
}

void ApplicationInfo::writeToDataStream(QDataStream &ds) const
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    ds << m_id
       << m_sysAppProperties
       << m_allAppProperties
       << m_codeFilePath
       << m_runtimeName
       << m_runtimeParameters
       << m_supportsApplicationInterface
       << m_capabilities
       << m_openGLConfiguration.desktopProfile
       << m_openGLConfiguration.esMajorVersion
       << m_openGLConfiguration.esMinorVersion
       << m_dltId
       << m_dltDescription
       << m_supportedMimeTypes
       << m_categories
       << m_names
       << m_descriptions
       << m_icon;
}

ApplicationInfo *ApplicationInfo::readFromDataStream(PackageInfo *pkg, QDataStream &ds)
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    auto app = std::make_unique<ApplicationInfo>(pkg);

    ds >> app->m_id
       >> app->m_sysAppProperties
       >> app->m_allAppProperties
       >> app->m_codeFilePath
       >> app->m_runtimeName
       >> app->m_runtimeParameters
       >> app->m_supportsApplicationInterface
       >> app->m_capabilities
       >> app->m_openGLConfiguration.desktopProfile
       >> app->m_openGLConfiguration.esMajorVersion
       >> app->m_openGLConfiguration.esMinorVersion
       >> app->m_dltId
       >> app->m_dltDescription
       >> app->m_supportedMimeTypes
       >> app->m_categories
       >> app->m_names
       >> app->m_descriptions
       >> app->m_icon;

    app->m_capabilities.sort();

    return app.release();
}


QVariantMap ApplicationInfo::toVariantMap() const
{
    QVariantMap map;
    //TODO: check if we can find a better method to keep this as similar as possible to
    //      ApplicationManager::get().
    //      This is used for RuntimeInterface::startApplication() and the ContainerInterface

    map[u"id"_s] = m_id;
    {
        QVariantMap displayName;
        const auto n = names();
        for (auto it = n.constBegin(); it != n.constEnd(); ++it)
            displayName.insert(it.key(), it.value());
        map[u"displayName"_s] = displayName;
    }

    map[u"displayIcon"_s] = icon();
    map[u"applicationProperties"_s] = m_allAppProperties;
    map[u"codeFilePath"_s] = m_codeFilePath;
    map[u"runtimeName"_s] = m_runtimeName;
    map[u"runtimeParameters"_s] = m_runtimeParameters;
    map[u"capabilities"_s] = m_capabilities;
    map[u"mimeTypes"_s] = m_supportedMimeTypes;

    map[u"categories"_s] = categories();
    map[u"version"_s] = packageInfo()->version();
    map[u"baseDir"_s] = packageInfo()->baseDir().absolutePath();
    map[u"codeDir"_s] = map[u"baseDir"_s];     // 5.12 backward compatibility
    map[u"manifestDir"_s] = map[u"baseDir"_s]; // 5.12 backward compatibility
    map[u"supportsApplicationInterface"_s] = m_supportsApplicationInterface;
    QVariantMap dltMap;
    dltMap[u"id"_s] = m_dltId;
    dltMap[u"description"_s] = m_dltDescription;
    map[u"dlt"_s] = dltMap;

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

OpenGLConfiguration ApplicationInfo::openGLConfiguration() const
{
    return m_openGLConfiguration;
}

bool ApplicationInfo::supportsApplicationInterface() const
{
    return m_supportsApplicationInterface;
}

QString ApplicationInfo::dltId() const
{
    return m_dltId;
}

QString ApplicationInfo::dltDescription() const
{
    return m_dltDescription;
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
