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

QT_BEGIN_NAMESPACE_AM

YamlPackageScanner::YamlPackageScanner()
{ }

PackageInfo *YamlPackageScanner::scan(const QString &fileName) Q_DECL_NOEXCEPT_EXPR(false)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        throw Exception(f, "Cannot open for reading");
    return scan(&f, f.fileName());
}

PackageInfo *YamlPackageScanner::scan(QIODevice *source, const QString &fileName) Q_DECL_NOEXCEPT_EXPR(false)
{
    try {
        YamlParser p(source->readAll());

        bool legacy = false;
        try {
            auto header = p.parseHeader();
            if (header.first == qL1S("am-application") && header.second == 1)
                legacy = true;
            else if (!(header.first == qL1S("am-package") && header.second == 1))
                throw Exception("Unsupported format type and/or version");
            p.nextDocument();
        } catch (const Exception &e) {
            throw YamlParserException(&p, "not a valid YAML package meta-data file: %1").arg(e.errorString());
        }

        QStringList appIds; // duplicate check
        std::unique_ptr<PackageInfo> pkgInfo(new PackageInfo);
        if (!fileName.isEmpty()) {
            QFileInfo fi(fileName);
            pkgInfo->m_baseDir = fi.absoluteDir();
            pkgInfo->m_manifestName = fi.fileName();
        }

        std::unique_ptr<ApplicationInfo> legacyAppInfo =
                legacy ? std::make_unique<ApplicationInfo>(pkgInfo.get()) : nullptr;

        // ----------------- package -----------------

        YamlParser::Fields fields;
        fields.emplace_back("id", true, YamlParser::Scalar, [&pkgInfo, &legacyAppInfo](YamlParser *p) {
            QString id = p->parseString();
            if (id.isEmpty())
                throw YamlParserException(p, "packages need to have an id");
            pkgInfo->m_id = id;
            if (legacyAppInfo)
                legacyAppInfo->m_id = pkgInfo->id();
        });
        fields.emplace_back("icon", false, YamlParser::Scalar, [&pkgInfo](YamlParser *p) {
            pkgInfo->m_icon = p->parseString();
        });
        fields.emplace_back("name", false, YamlParser::Map, [&pkgInfo](YamlParser *p) {
            auto nameMap = p->parseMap();
            for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
                pkgInfo->m_names.insert(it.key(), it.value().toString());
        });
        if (!legacy) {
            fields.emplace_back("description", false, YamlParser::Map, [&pkgInfo](YamlParser *p) {
                auto descriptionMap = p->parseMap();
                for (auto it = descriptionMap.constBegin(); it != descriptionMap.constEnd(); ++it)
                    pkgInfo->m_descriptions.insert(it.key(), it.value().toString());
            });
        }
        fields.emplace_back("categories", false, YamlParser::Scalar | YamlParser::List, [&pkgInfo](YamlParser *p) {
            pkgInfo->m_categories = p->parseStringOrStringList();
            pkgInfo->m_categories.sort();
        });
        fields.emplace_back("version", false, YamlParser::Scalar, [&pkgInfo](YamlParser *p) {
            pkgInfo->m_version = p->parseString();
        });
        if (legacy) {
            fields.emplace_back("code", true, YamlParser::Scalar, [&legacyAppInfo](YamlParser *p) {
                legacyAppInfo->m_codeFilePath = p->parseString();
            });
            fields.emplace_back("runtime", true, YamlParser::Scalar, [&legacyAppInfo](YamlParser *p) {
                legacyAppInfo->m_runtimeName = p->parseString();
            });
            fields.emplace_back("runtimeParameters", false, YamlParser::Map, [&legacyAppInfo](YamlParser *p) {
                legacyAppInfo->m_runtimeParameters = p->parseMap();
            });
            fields.emplace_back("supportsApplicationInterface", false, YamlParser::Scalar, [&legacyAppInfo](YamlParser *p) {
                legacyAppInfo->m_supportsApplicationInterface = p->parseScalar().toBool();
            });
            fields.emplace_back("capabilities", false, YamlParser::Scalar | YamlParser::List, [&legacyAppInfo](YamlParser *p) {
                legacyAppInfo->m_capabilities = p->parseStringOrStringList();
                legacyAppInfo->m_capabilities.sort();
            });
            fields.emplace_back("opengl", false, YamlParser::Map, [&legacyAppInfo](YamlParser *p) {
                legacyAppInfo->m_openGLConfiguration = p->parseMap();

                // sanity check - could be rewritten using the "fields" mechanism
                static QStringList validKeys = {
                    qSL("desktopProfile"),
                    qSL("esMajorVersion"),
                    qSL("esMinorVersion")
                };
                for (auto it = legacyAppInfo->m_openGLConfiguration.cbegin();
                     it != legacyAppInfo->m_openGLConfiguration.cend(); ++it) {
                    if (!validKeys.contains(it.key())) {
                        throw YamlParserException(p, "the 'opengl' object contains the unsupported key '%1'")
                                .arg(it.key());
                    }
                }
            });
            fields.emplace_back("applicationProperties", false, YamlParser::Map, [&legacyAppInfo](YamlParser *p) {
                const QVariantMap rawMap = p->parseMap();
                legacyAppInfo->m_sysAppProperties = rawMap.value(qSL("protected")).toMap();
                legacyAppInfo->m_allAppProperties = legacyAppInfo->m_sysAppProperties;
                const QVariantMap pri = rawMap.value(qSL("private")).toMap();
                for (auto it = pri.cbegin(); it != pri.cend(); ++it)
                    legacyAppInfo->m_allAppProperties.insert(it.key(), it.value());
            });
            fields.emplace_back("documentUrl", false, YamlParser::Scalar, [&legacyAppInfo](YamlParser *p) {
                legacyAppInfo->m_documentUrl = p->parseScalar().toString();
            });
            fields.emplace_back("mimeTypes", false, YamlParser::Scalar | YamlParser::List, [&legacyAppInfo](YamlParser *p) {
                legacyAppInfo->m_supportedMimeTypes = p->parseStringOrStringList();
                legacyAppInfo->m_supportedMimeTypes.sort();
            });
            fields.emplace_back("logging", false, YamlParser::Map, [&legacyAppInfo](YamlParser *p) {
                const QVariantMap logging = p->parseMap();
                if (!logging.isEmpty()) {
                    if (logging.size() > 1 || logging.firstKey() != qSL("dlt"))
                        throw YamlParserException(p, "'logging' only supports the 'dlt' key");
                    legacyAppInfo->m_dltConfiguration = logging.value(qSL("dlt")).toMap();

                    // sanity check
                    for (auto it = legacyAppInfo->m_dltConfiguration.cbegin(); it != legacyAppInfo->m_dltConfiguration.cend(); ++it) {
                        if (it.key() != qSL("id") && it.key() != qSL("description"))
                            throw YamlParserException(p, "unsupported key in 'logging/dlt'");
                    }
                }
            });
            fields.emplace_back("environmentVariables", false, YamlParser::Map, [](YamlParser *p) {
                qCDebug(LogSystem) << "ignoring 'environmentVariables'";
                (void) p->parseMap();
            });
        }

        // ----------------- applications -----------------

        if (!legacy) {
            fields.emplace_back("applications", true, YamlParser::List, [&pkgInfo, &appIds](YamlParser *p) {
                p->parseList([&pkgInfo, &appIds](YamlParser *p) {
                    auto appInfo = std::make_unique<ApplicationInfo>(pkgInfo.get());
                    YamlParser::Fields appFields;

                    appFields.emplace_back("id", true, YamlParser::Scalar, [&appInfo, &appIds](YamlParser *p) {
                        QString id = p->parseString();
                        if (id.isEmpty())
                            throw YamlParserException(p, "applications need to have an id");
                        if (appIds.contains(id))
                            throw YamlParserException(p, "found two applications with the same id %1").arg(id);
                        appInfo->m_id = id;
                    });
                    appFields.emplace_back("icon", false, YamlParser::Scalar, [&appInfo](YamlParser *p) {
                        appInfo->m_icon = p->parseString();
                    });
                    appFields.emplace_back("name", false, YamlParser::Map, [&appInfo](YamlParser *p) {
                        const auto nameMap = p->parseMap();
                        for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
                            appInfo->m_names.insert(it.key(), it.value().toString());
                    });
                    appFields.emplace_back("description", false, YamlParser::Map, [&appInfo](YamlParser *p) {
                        const auto descriptionMap = p->parseMap();
                        for (auto it = descriptionMap.constBegin(); it != descriptionMap.constEnd(); ++it)
                            appInfo->m_descriptions.insert(it.key(), it.value().toString());
                    });
                    appFields.emplace_back("categories", false, YamlParser::Scalar | YamlParser::List, [&appInfo](YamlParser *p) {
                        appInfo->m_categories = p->parseStringOrStringList();
                        appInfo->m_categories.sort();
                    });
                    appFields.emplace_back("code", true, YamlParser::Scalar, [&appInfo](YamlParser *p) {
                        appInfo->m_codeFilePath = p->parseString();
                    });
                    appFields.emplace_back("runtime", true, YamlParser::Scalar, [&appInfo](YamlParser *p) {
                        appInfo->m_runtimeName = p->parseString();
                    });
                    appFields.emplace_back("runtimeParameters", false, YamlParser::Map, [&appInfo](YamlParser *p) {
                        appInfo->m_runtimeParameters = p->parseMap();
                    });
                    appFields.emplace_back("supportsApplicationInterface", false, YamlParser::Scalar, [&appInfo](YamlParser *p) {
                        appInfo->m_supportsApplicationInterface = p->parseScalar().toBool();
                    });
                    appFields.emplace_back("capabilities", false, YamlParser::Scalar | YamlParser::List, [&appInfo](YamlParser *p) {
                        appInfo->m_capabilities = p->parseStringOrStringList();
                        appInfo->m_capabilities.sort();
                    });
                    appFields.emplace_back("opengl", false, YamlParser::Map, [&appInfo](YamlParser *p) {
                        appInfo->m_openGLConfiguration = p->parseMap();

                        // sanity check - could be rewritten using the "fields" mechanism
                        static QStringList validKeys = {
                            qSL("desktopProfile"),
                            qSL("esMajorVersion"),
                            qSL("esMinorVersion")
                        };
                        for (auto it = appInfo->m_openGLConfiguration.cbegin();
                             it != appInfo->m_openGLConfiguration.cend(); ++it) {
                            if (!validKeys.contains(it.key())) {
                                throw YamlParserException(p, "the 'opengl' object contains the unsupported key '%1'")
                                        .arg(it.key());
                            }
                        }
                    });
                    appFields.emplace_back("applicationProperties", false, YamlParser::Map, [&appInfo](YamlParser *p) {
                        const QVariantMap rawMap = p->parseMap();
                        appInfo->m_sysAppProperties = rawMap.value(qSL("protected")).toMap();
                        appInfo->m_allAppProperties = appInfo->m_sysAppProperties;
                        const QVariantMap pri = rawMap.value(qSL("private")).toMap();
                        for (auto it = pri.cbegin(); it != pri.cend(); ++it)
                            appInfo->m_allAppProperties.insert(it.key(), it.value());
                    });
                    appFields.emplace_back("logging", false, YamlParser::Map, [&appInfo](YamlParser *p) {
                        const QVariantMap logging = p->parseMap();
                        if (!logging.isEmpty()) {
                            if (logging.size() > 1 || logging.firstKey() != qSL("dlt"))
                                throw YamlParserException(p, "'logging' only supports the 'dlt' key");
                            appInfo->m_dltConfiguration = logging.value(qSL("dlt")).toMap();

                            // sanity check
                            for (auto it = appInfo->m_dltConfiguration.cbegin(); it != appInfo->m_dltConfiguration.cend(); ++it) {
                                if (it.key() != qSL("id") && it.key() != qSL("description"))
                                    throw YamlParserException(p, "unsupported key in 'logging/dlt'");
                            }
                        }
                    });

                    p->parseFields(appFields);
                    appIds << appInfo->id();
                    pkgInfo->m_applications << appInfo.release();
                });
            });
        }

        // ----------------- intents -----------------

        fields.emplace_back("intents", false, YamlParser::List, [&pkgInfo, &appIds, legacy](YamlParser *p) {
            QStringList intentIds; // duplicate check
            p->parseList([&pkgInfo, &appIds, &intentIds, legacy](YamlParser *p) {
                auto intentInfo = std::make_unique<IntentInfo>(pkgInfo.get());
                YamlParser::Fields intentFields;

                intentFields.emplace_back("id", true, YamlParser::Scalar, [&intentInfo, &intentIds, &pkgInfo](YamlParser *p) {
                    QString id = p->parseString();
                    if (id.isEmpty())
                        throw YamlParserException(p, "intents need to have an id (package %1)").arg(pkgInfo->id());
                    if (intentIds.contains(id))
                        throw YamlParserException(p, "found two intent handlers for intent %2 (package %1)").arg(pkgInfo->id()).arg(id);
                    intentInfo->m_id = id;
                });
                intentFields.emplace_back("visibility", false, YamlParser::Scalar, [&intentInfo](YamlParser *p) {
                    const QString visibilityStr = p->parseString();
                    if (visibilityStr == qL1S("private")) {
                        intentInfo->m_visibility = IntentInfo::Private;
                    } else if (visibilityStr != qL1S("public")) {
                        throw YamlParserException(p, "intent visibilty '%2' is invalid on intent %1 (valid values are either 'public' or 'private'")
                                .arg(intentInfo->m_id).arg(visibilityStr);
                    }
                });
                intentFields.emplace_back(legacy ? "handledBy" : "handlingApplicationId", false,  YamlParser::Scalar, [&intentInfo, &appIds](YamlParser *p) {
                    QString appId = p->parseString();
                    if (appIds.contains(appId)) {
                        intentInfo->m_handlingApplicationId = appId;
                    } else {
                        throw YamlParserException(p, "the 'handlingApplicationId' field on intent %1 points to the unknown application id %2")
                                .arg(intentInfo->m_id).arg(appId);
                    }
                });
                intentFields.emplace_back("requiredCapabilities", false, YamlParser::Scalar | YamlParser::List, [&intentInfo](YamlParser *p) {
                    intentInfo->m_requiredCapabilities = p->parseStringOrStringList();
                });
                intentFields.emplace_back("parameterMatch", false, YamlParser::Map, [&intentInfo](YamlParser *p) {
                    intentInfo->m_parameterMatch = p->parseMap();
                });
                intentFields.emplace_back("icon", false, YamlParser::Scalar, [&intentInfo](YamlParser *p) {
                    intentInfo->m_icon = p->parseString();
                });
                intentFields.emplace_back("name", false, YamlParser::Map, [&intentInfo](YamlParser *p) {
                    auto nameMap = p->parseMap();
                    for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
                        intentInfo->m_names.insert(it.key(), it.value().toString());
                });
                intentFields.emplace_back("description", false, YamlParser::Map, [&intentInfo](YamlParser *p) {
                    auto descriptionMap = p->parseMap();
                    for (auto it = descriptionMap.constBegin(); it != descriptionMap.constEnd(); ++it)
                        intentInfo->m_descriptions.insert(it.key(), it.value().toString());
                });
                intentFields.emplace_back("categories", false, YamlParser::Scalar | YamlParser::List, [&intentInfo](YamlParser *p) {
                    intentInfo->m_categories = p->parseStringOrStringList();
                    intentInfo->m_categories.sort();
                });

                p->parseFields(intentFields);

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
            });
        });

        p.parseFields(fields);

        if (legacy)
            pkgInfo->m_applications << legacyAppInfo.release();

        // validate the ids, runtime names and all referenced files
        pkgInfo->validate();
        return pkgInfo.release();
    } catch (const Exception &e) {
        throw Exception(e.errorCode(), "Failed to parse manifest file %1: %2")
                .arg(!fileName.isEmpty() ? QDir().relativeFilePath(fileName) : qSL("<stream>"), e.errorString());
    }
}


QT_END_NAMESPACE_AM
