// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>

#include "global.h"
#include "qtyaml.h"
#include "exception.h"
#include "logging.h"
#include "packageinfo.h"
#include "applicationinfo.h"
#include "intentinfo.h"
#include "yamlpackagescanner.h"
#include "utilities.h"

#include <memory>

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

PackageInfo *YamlPackageScanner::scan(const QString &fileName) noexcept(false)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        throw Exception(f, "Cannot open for reading");
    return scan(&f, f.fileName());
}

PackageInfo *YamlPackageScanner::scan(QIODevice *source, const QString &fileName) noexcept(false)
{
    try {
        YamlParser yp(source->readAll());

        bool legacy = false;
        try {
            auto header = yp.parseHeader();
            if ((header.first == u"am-application") && (header.second == 1))
                legacy = true;
            else if (!((header.first == u"am-package") && (header.second == 1)))
                throw Exception("Unsupported format type and/or version");
            yp.nextDocument();
        } catch (const Exception &e) {
            throw YamlParserException(&yp, "not a valid YAML package meta-data file: %1").arg(e.errorString());
        }

        QStringList appIds; // duplicate check
        std::unique_ptr<PackageInfo> pkgInfo(new PackageInfo);
        if (!fileName.isEmpty()) {
            QFileInfo fi(fileName);
            pkgInfo->m_baseDir = fi.absoluteDir();  // clazy:exclude=qt6-deprecated-api-fixes
            pkgInfo->m_manifestName = fi.fileName();
        }

        std::unique_ptr<ApplicationInfo> legacyAppInfo =
            legacy ? std::make_unique<ApplicationInfo>(pkgInfo.get()) : nullptr;

        // ----------------- package -----------------

        yp.parseFields({
            { "id", true, YamlParser::Scalar, [&]() {
                 QString id = yp.parseString();
                 if (id.isEmpty())
                     throw YamlParserException(&yp, "packages need to have an id");
                 pkgInfo->m_id = id;
                 if (legacyAppInfo)
                     legacyAppInfo->m_id = pkgInfo->id(); } },
            { "icon", false, YamlParser::Scalar, [&]() {
                 pkgInfo->m_icon = yp.parseString(); } },
            { "name", false, YamlParser::Map, [&]() {
                 pkgInfo->m_names = yp.parseStringMap(); } },
            { !legacy, "description", false, YamlParser::Map, [&]() {
                 pkgInfo->m_descriptions = yp.parseStringMap(); } },
            { "categories", false, YamlParser::Scalar | YamlParser::List, [&]() {
                 pkgInfo->m_categories = yp.parseStringOrStringList();
                 pkgInfo->m_categories.sort(); } },
            { "version", false, YamlParser::Scalar, [&]() {
                 pkgInfo->m_version = yp.parseString(); } },
            { legacy, "code", true, YamlParser::Scalar, [&]() {
                 legacyAppInfo->m_codeFilePath = yp.parseString(); } },
            { legacy, "runtime", true, YamlParser::Scalar, [&]() {
                 legacyAppInfo->m_runtimeName = yp.parseString(); } },
            { legacy, "runtimeParameters", false, YamlParser::Map, [&]() {
                 legacyAppInfo->m_runtimeParameters = yp.parseMap(); } },
            { legacy, "supportsApplicationInterface", false, YamlParser::Scalar, [&]() {
                 legacyAppInfo->m_supportsApplicationInterface = yp.parseBool(); } },
            { legacy, "capabilities", false, YamlParser::Scalar | YamlParser::List, [&]() {
                 legacyAppInfo->m_capabilities = yp.parseStringOrStringList();
                 legacyAppInfo->m_capabilities.sort(); } },
            { legacy, "opengl", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "desktopProfile", false, YamlParser::Scalar, [&]() {
                          legacyAppInfo->m_openGLConfiguration.desktopProfile = yp.parseString(); } },
                     { "esMajorVersion", false, YamlParser::Scalar, [&]() {
                          legacyAppInfo->m_openGLConfiguration.esMajorVersion = yp.parseInt(2); } },
                     { "esMinorVersion", false, YamlParser::Scalar, [&]() {
                          legacyAppInfo->m_openGLConfiguration.esMinorVersion = yp.parseInt(0); } },
                 }); } },
            { legacy, "applicationProperties", false, YamlParser::Map, [&]() {
                 const QVariantMap rawMap = yp.parseMap();
                 legacyAppInfo->m_sysAppProperties = rawMap.value(u"protected"_s).toMap();
                 legacyAppInfo->m_allAppProperties = legacyAppInfo->m_sysAppProperties;
                 const QVariantMap pri = rawMap.value(u"private"_s).toMap();
                 for (auto it = pri.cbegin(); it != pri.cend(); ++it)
                     legacyAppInfo->m_allAppProperties.insert(it.key(), it.value()); } },
            { legacy, "documentUrl", false, YamlParser::Scalar, [&]() {
                 legacyAppInfo->m_documentUrl = yp.parseString(); } },
            { legacy, "mimeTypes", false, YamlParser::Scalar | YamlParser::List, [&]() {
                 legacyAppInfo->m_supportedMimeTypes = yp.parseStringOrStringList();
                 legacyAppInfo->m_supportedMimeTypes.sort(); } },
            { legacy, "logging", false, YamlParser::Map, [&]() {
                 yp.parseFields({
                     { "dlt", false, YamlParser::Map, [&]() {
                          yp.parseFields({
                              { "id", false, YamlParser::Scalar, [&]() {
                                   legacyAppInfo->m_dltId = yp.parseString(); } },
                              { "description", false, YamlParser::Scalar, [&]() {
                                   legacyAppInfo->m_dltDescription = yp.parseString(); } },
                          }); } }
                 }); } },
            { legacy, "environmentVariables", false, YamlParser::Map, [&]() {
                 qCDebug(LogSystem) << "ignoring 'environmentVariables'";
                 (void) yp.parseMap(); } },

            // ----------------- applications -----------------

            { !legacy, "applications", true, YamlParser::List, [&]() {
                 yp.parseList([&]() {
                     auto appInfo = std::make_unique<ApplicationInfo>(pkgInfo.get());

                     yp.parseFields({
                         { "id", true, YamlParser::Scalar, [&]() {
                              QString id = yp.parseString();
                              if (id.isEmpty())
                                  throw YamlParserException(&yp, "applications need to have an id");
                              if (appIds.contains(id))
                                  throw YamlParserException(&yp, "found two applications with the same id %1").arg(id);
                              appInfo->m_id = id; } },
                         { "icon", false, YamlParser::Scalar, [&]() {
                              appInfo->m_icon = yp.parseString(); } },
                         { "name", false, YamlParser::Map, [&]() {
                              appInfo->m_names = yp.parseStringMap(); } },
                         { "description", false, YamlParser::Map, [&]() {
                              appInfo->m_descriptions = yp.parseStringMap(); } },
                         { "categories", false, YamlParser::Scalar | YamlParser::List, [&]() {
                              appInfo->m_categories = yp.parseStringOrStringList();
                              appInfo->m_categories.sort(); } },
                         { "code", true, YamlParser::Scalar, [&]() {
                              appInfo->m_codeFilePath = yp.parseString(); } },
                         { "runtime", true, YamlParser::Scalar, [&]() {
                              appInfo->m_runtimeName = yp.parseString(); } },
                         { "runtimeParameters", false, YamlParser::Map, [&]() {
                              appInfo->m_runtimeParameters = yp.parseMap(); } },
                         { "supportsApplicationInterface", false, YamlParser::Scalar, [&]() {
                              appInfo->m_supportsApplicationInterface = yp.parseBool(); } },
                         { "capabilities", false, YamlParser::Scalar | YamlParser::List, [&]() {
                              appInfo->m_capabilities = yp.parseStringOrStringList();
                              appInfo->m_capabilities.sort(); } },
                         { "opengl", false, YamlParser::Map, [&]() {
                              yp.parseFields({
                                  { "desktopProfile", false, YamlParser::Scalar, [&]() {
                                       appInfo->m_openGLConfiguration.desktopProfile = yp.parseString(); } },
                                  { "esMajorVersion", false, YamlParser::Scalar, [&]() {
                                       appInfo->m_openGLConfiguration.esMajorVersion = yp.parseInt(2); } },
                                  { "esMinorVersion", false, YamlParser::Scalar, [&]() {
                                       appInfo->m_openGLConfiguration.esMinorVersion = yp.parseInt(0); } },
                              }); } },
                         { "applicationProperties", false, YamlParser::Map, [&]() {
                              const QVariantMap rawMap = yp.parseMap();
                              appInfo->m_sysAppProperties = rawMap.value(u"protected"_s).toMap();
                              appInfo->m_allAppProperties = appInfo->m_sysAppProperties;
                              const QVariantMap pri = rawMap.value(u"private"_s).toMap();
                              for (auto it = pri.cbegin(); it != pri.cend(); ++it)
                                  appInfo->m_allAppProperties.insert(it.key(), it.value()); } },
                         { "logging", false, YamlParser::Map, [&]() {
                              yp.parseFields({
                                  { "dlt", false, YamlParser::Map, [&]() {
                                       yp.parseFields({
                                           { "id", false, YamlParser::Scalar, [&]() {
                                                appInfo->m_dltId = yp.parseString(); } },
                                           { "description", false, YamlParser::Scalar, [&]() {
                                                appInfo->m_dltDescription = yp.parseString(); } },
                                       }); } }
                              }); } }
                     });

                     appIds << appInfo->id();
                     pkgInfo->m_applications << appInfo.release();
                 }); } },

            // ----------------- intents -----------------

            { "intents", false, YamlParser::List, [&, legacy]() {
                 QStringList intentIds; // duplicate check
                 yp.parseList([&, legacy]() {
                     auto intentInfo = std::make_unique<IntentInfo>(pkgInfo.get());

                     yp.parseFields({

                         { "id", true, YamlParser::Scalar, [&]() {
                              QString id = yp.parseString();
                              if (id.isEmpty())
                                  throw YamlParserException(&yp, "intents need to have an id (package %1)").arg(pkgInfo->id());
                              if (intentIds.contains(id))
                                  throw YamlParserException(&yp, "found two intent handlers for intent %2 (package %1)").arg(pkgInfo->id()).arg(id);
                              intentInfo->m_id = id; } },
                         { "visibility", false, YamlParser::Scalar, [&]() {
                              const QString visibilityStr = yp.parseString();
                              if (visibilityStr == u"private") {
                                  intentInfo->m_visibility = IntentInfo::Private;
                              } else if (visibilityStr != u"public") {
                                  throw YamlParserException(&yp, "intent visibilty '%2' is invalid on intent %1 (valid values are either 'public' or 'private'")
                                      .arg(intentInfo->m_id).arg(visibilityStr);
                              } } },
                         { legacy ? "handledBy" : "handlingApplicationId", false,  YamlParser::Scalar, [&]() {
                              QString appId = yp.parseString();
                              if (appIds.contains(appId)) {
                                  intentInfo->m_handlingApplicationId = appId;
                              } else {
                                  throw YamlParserException(&yp, "the 'handlingApplicationId' field on intent %1 points to the unknown application id %2")
                                      .arg(intentInfo->m_id).arg(appId);
                              } } },
                         { "requiredCapabilities", false, YamlParser::Scalar | YamlParser::List, [&]() {
                              intentInfo->m_requiredCapabilities = yp.parseStringOrStringList(); } },
                         { "parameterMatch", false, YamlParser::Map, [&]() {
                              intentInfo->m_parameterMatch = yp.parseMap(); } },
                         { "icon", false, YamlParser::Scalar, [&]() {
                              intentInfo->m_icon = yp.parseString(); } },
                         { "name", false, YamlParser::Map, [&]() {
                              intentInfo->m_names = yp.parseStringMap(); } },
                         { "description", false, YamlParser::Map, [&]() {
                              intentInfo->m_descriptions = yp.parseStringMap(); } },
                         { "categories", false, YamlParser::Scalar | YamlParser::List, [&]() {
                              intentInfo->m_categories = yp.parseStringOrStringList();
                              intentInfo->m_categories.sort(); } },
                         { "handleOnlyWhenRunning", false, YamlParser::Scalar, [&]() {
                              intentInfo->m_handleOnlyWhenRunning = yp.parseBool(); } },
                     });

                     if (intentInfo->handlingApplicationId().isEmpty()) {
                         if (legacy) {
                             intentInfo->m_handlingApplicationId = pkgInfo->id();
                         } else if (pkgInfo->m_applications.count() == 1) {
                             intentInfo->m_handlingApplicationId = pkgInfo->m_applications.constFirst()->id();
                         } else {
                             throw Exception(Error::Parse, "a 'handlingApplicationId' field on intent %1 is needed if more than one application is defined")
                                 .arg(intentInfo->m_id);
                         }
                     }

                     pkgInfo->m_intents << intentInfo.release();
                 }); } }
        });

        if (legacy)
            pkgInfo->m_applications << legacyAppInfo.release();

        // validate the ids, runtime names and all referenced files
        pkgInfo->validate();
        return pkgInfo.release();
    } catch (const Exception &e) {
        throw Exception(e.errorCode(), "Failed to parse manifest file %1: %2")
            .arg(!fileName.isEmpty() ? QDir().relativeFilePath(fileName) : u"<stream>"_s, e.errorString());
    }
}


QT_END_NAMESPACE_AM
