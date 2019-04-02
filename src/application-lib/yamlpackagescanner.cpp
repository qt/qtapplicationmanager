/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QScopedPointer>

#include "global.h"
#include "qtyaml.h"
#include "exception.h"
#include "logging.h"
#include "packageinfo.h"
#include "applicationinfo.h"
#include "intentinfo.h"
#include "yamlpackagescanner.h"
#include "utilities.h"

using Fields = std::vector<std::tuple<QByteArray, bool, const std::function<void(const QVariant &value)>>>;

QT_BEGIN_NAMESPACE_AM


YamlPackageScanner::YamlPackageScanner()
{ }

PackageInfo *YamlPackageScanner::scan(const QString &filePath) Q_DECL_NOEXCEPT_EXPR(false)
{
    auto parseMap = [](const QVariantMap &map, const Fields &fields) {
        QStringList keys = map.keys();

        for (const auto &field : fields) {
            QString fieldName = qL1S(std::get<0>(field));

            auto it = map.constFind(fieldName);
            if (it != map.cend()) {
                std::get<2>(field)(it.value());
            } else if (std::get<1>(field)) { // missing, but required
                throw Exception("required field missing in YAML: %1").arg(fieldName);
            }
            keys.removeOne(fieldName);
        }
        if (!keys.isEmpty())
            throw Exception("unsupported fields found in YAML: %1").arg(keys.join(qSL(", ")));
    };


    try {
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly))
            throw Exception(f, "could not open file for reading");

        QtYaml::ParseError parseError;
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(f.readAll(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            throw Exception(Error::IO, "YAML parse error at line %1, column %2: %3")
                    .arg(parseError.line).arg(parseError.column).arg(parseError.errorString());
        }

        bool legacy = false;
        try {
            checkYamlFormat(docs, 2 /*number of expected docs*/, { "am-package" }, 1);
        } catch (const Exception &e) {
            try {
                checkYamlFormat(docs, 2 /*number of expected docs*/, { "am-application" }, 1);
                qCDebug(LogSystem) << "Manifest file" << f.fileName() << "is still using the legacy 'am-application' format";
                legacy = true;
            } catch (const Exception &) {
                throw Exception(Error::Parse, "not a valid YAML application meta-data file: %1").arg(e.errorString());
            }
        }

        QStringList appIds; // duplicate check
        QScopedPointer<PackageInfo> pkgInfo(new PackageInfo);
        pkgInfo->setBaseDir(QFileInfo(f).absoluteDir());

        QScopedPointer<ApplicationInfo> legacyAppInfo(legacy ? new ApplicationInfo(pkgInfo.data()) : nullptr);

        // ----------------- package -----------------

        Fields fields;
        fields.emplace_back("id", true, [&pkgInfo, &legacyAppInfo](const QVariant &v) {
            QString id = v.toString();
            if (id.isEmpty())
                throw Exception(Error::Parse, "packages need to have an id");
            pkgInfo->m_id = id;
            if (legacyAppInfo)
                legacyAppInfo->m_id = pkgInfo->id();
        });
        fields.emplace_back("icon", true, [&pkgInfo](const QVariant &v) {
            pkgInfo->m_icon = v.toString();
        });
        fields.emplace_back("name", true, [&pkgInfo](const QVariant &v) {
            auto nameMap = v.toMap();
            for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
                pkgInfo->m_name.insert(it.key(), it.value().toString());

            if (pkgInfo->m_name.isEmpty())
                throw Exception(Error::Parse, "the 'name' field must not be empty");
        });
        if (!legacy) {
            fields.emplace_back("description", false, [&pkgInfo](const QVariant &v) {
                auto descriptionMap = v.toMap();
                for (auto it = descriptionMap.constBegin(); it != descriptionMap.constEnd(); ++it)
                    pkgInfo->m_description.insert(it.key(), it.value().toString());
            });
        }
        fields.emplace_back("categories", false, [&pkgInfo](const QVariant &v) {
            pkgInfo->m_categories = variantToStringList(v);
            pkgInfo->m_categories.sort();
        });
        fields.emplace_back("version", false, [&pkgInfo](const QVariant &v) {
            pkgInfo->m_version = v.toString();
        });
        fields.emplace_back("logging", false, [&pkgInfo](const QVariant &v) {
            const QVariantMap logging = v.toMap();
            if (!logging.isEmpty()) {
                if (logging.size() > 1 || logging.firstKey() != qSL("dlt"))
                    throw Exception(Error::Parse, "'logging' only supports the 'dlt' key");
                pkgInfo->m_dltConfiguration = logging.value(qSL("dlt")).toMap();

                // sanity check
                for (auto it = pkgInfo->m_dltConfiguration.cbegin(); it != pkgInfo->m_dltConfiguration.cend(); ++it) {
                    if (it.key() != qSL("id") && it.key() != qSL("description"))
                        throw Exception(Error::Parse, "unsupported key in 'logging/dlt'");
                }
            }
        });
        if (legacy) {
            fields.emplace_back("code", true, [&legacyAppInfo](const QVariant &v) {
                legacyAppInfo->m_codeFilePath = v.toString();
            });
            fields.emplace_back("runtime", true, [&legacyAppInfo](const QVariant &v) {
                legacyAppInfo->m_runtimeName = v.toString();
            });
            fields.emplace_back("runtimeParameters", false, [&legacyAppInfo](const QVariant &v) {
                legacyAppInfo->m_runtimeParameters = v.toMap();
            });
            fields.emplace_back("supportsApplicationInterface", false, [&legacyAppInfo](const QVariant &v) {
                legacyAppInfo->m_supportsApplicationInterface = v.toBool();
            });
            fields.emplace_back("capabilities", false, [&legacyAppInfo](const QVariant &v) {
                legacyAppInfo->m_capabilities = variantToStringList(v);
                legacyAppInfo->m_capabilities.sort();
            });
            fields.emplace_back("opengl", false, [&legacyAppInfo](const QVariant &v) {
                legacyAppInfo->m_openGLConfiguration = v.toMap();

                // sanity check - could be rewritten using the "fields" mechanism
                static QStringList validKeys = {
                    qSL("desktopProfile"),
                    qSL("esMajorVersion"),
                    qSL("esMinorVersion")
                };
                for (auto it = legacyAppInfo->m_openGLConfiguration.cbegin();
                     it != legacyAppInfo->m_openGLConfiguration.cend(); ++it) {
                    if (!validKeys.contains(it.key())) {
                        throw Exception(Error::Parse, "the 'opengl' object contains the unsupported key '%1'")
                                .arg(it.key());
                    }
                }
            });
            fields.emplace_back("applicationProperties", false, [&legacyAppInfo](const QVariant &v) {
                const QVariantMap rawMap = v.toMap();
                legacyAppInfo->m_sysAppProperties = rawMap.value(qSL("protected")).toMap();
                legacyAppInfo->m_allAppProperties = legacyAppInfo->m_sysAppProperties;
                const QVariantMap pri = rawMap.value(qSL("private")).toMap();
                for (auto it = pri.cbegin(); it != pri.cend(); ++it)
                    legacyAppInfo->m_allAppProperties.insert(it.key(), it.value());
            });
            fields.emplace_back("documentUrl", false, [](const QVariant &) {
                qCDebug(LogSystem) << " ignoring 'documentUrl'";
            });
            fields.emplace_back("mimeTypes", false, [&legacyAppInfo](const QVariant &v) {
                legacyAppInfo->m_supportedMimeTypes = variantToStringList(v);
                legacyAppInfo->m_supportedMimeTypes.sort();
            });
        }

        // ----------------- applications -----------------

        if (!legacy) {
            fields.emplace_back("applications", true, [&pkgInfo, &appIds, &parseMap](const QVariant &v) {
                QVariantList apps = v.toList();

                for (auto appsIt = apps.constBegin(); appsIt != apps.constEnd(); ++appsIt) {
                    QScopedPointer<ApplicationInfo> appInfo(new ApplicationInfo(pkgInfo.data()));
                    Fields appFields;

                    appFields.emplace_back("id", true, [&appInfo, &appIds](const QVariant &v) {
                        QString id = v.toString();
                        if (id.isEmpty())
                            throw Exception(Error::Intents, "applications need to have an id");
                        if (appIds.contains(id))
                            throw Exception(Error::Intents, "found two applications with id %1").arg(id);
                        appInfo->m_id = id;
                    });
                    appFields.emplace_back("code", true, [&appInfo](const QVariant &v) {
                        appInfo->m_codeFilePath = v.toString();
                    });
                    appFields.emplace_back("runtime", true, [&appInfo](const QVariant &v) {
                        appInfo->m_runtimeName = v.toString();
                    });
                    appFields.emplace_back("runtimeParameters", false, [&appInfo](const QVariant &v) {
                        appInfo->m_runtimeParameters = v.toMap();
                    });
                    appFields.emplace_back("supportsApplicationInterface", false, [&appInfo](const QVariant &v) {
                        appInfo->m_supportsApplicationInterface = v.toBool();
                    });
                    appFields.emplace_back("capabilities", false, [&appInfo](const QVariant &v) {
                        appInfo->m_capabilities = variantToStringList(v);
                        appInfo->m_capabilities.sort();
                    });
                    appFields.emplace_back("opengl", false, [&appInfo](const QVariant &v) {
                        appInfo->m_openGLConfiguration = v.toMap();

                        // sanity check - could be rewritten using the "fields" mechanism
                        static QStringList validKeys = {
                            qSL("desktopProfile"),
                            qSL("esMajorVersion"),
                            qSL("esMinorVersion")
                        };
                        for (auto it = appInfo->m_openGLConfiguration.cbegin();
                             it != appInfo->m_openGLConfiguration.cend(); ++it) {
                            if (!validKeys.contains(it.key())) {
                                throw Exception(Error::Parse, "the 'opengl' object contains the unsupported key '%1'")
                                        .arg(it.key());
                            }
                        }
                    });
                    appFields.emplace_back("applicationProperties", false, [&appInfo](const QVariant &v) {
                        const QVariantMap rawMap = v.toMap();
                        appInfo->m_sysAppProperties = rawMap.value(qSL("protected")).toMap();
                        appInfo->m_allAppProperties = appInfo->m_sysAppProperties;
                        const QVariantMap pri = rawMap.value(qSL("private")).toMap();
                        for (auto it = pri.cbegin(); it != pri.cend(); ++it)
                            appInfo->m_allAppProperties.insert(it.key(), it.value());
                    });

                    parseMap(appsIt->toMap(), appFields);
                    pkgInfo->m_applications << appInfo.take();
                }
            });
        }

        // ----------------- intents -----------------

        fields.emplace_back("intents", false, [&pkgInfo, &parseMap, &appIds, legacy](const QVariant &v) {
            QVariantList intents = v.toList();
            QStringList intentIds; // duplicate check

            for (auto intentsIt = intents.constBegin(); intentsIt != intents.constEnd(); ++intentsIt) {
                QScopedPointer<IntentInfo> intentInfo(new IntentInfo(pkgInfo.data()));
                Fields intentFields;

                intentFields.emplace_back("id", true, [&intentInfo, &intentIds, &pkgInfo](const QVariant &v) {
                    QString id = v.toString();
                    if (id.isEmpty())
                        throw Exception(Error::Intents, "intents need to have an id (package %1)").arg(pkgInfo->id());
                    if (intentIds.contains(id))
                        throw Exception(Error::Intents, "found two intent handlers for intent %2 (package %1)").arg(pkgInfo->id()).arg(id);
                    intentInfo->m_id = id;
                });
                intentFields.emplace_back("visibility", false, [&intentInfo](const QVariant &v) {
                    const QString visibilityStr = v.toString();
                    if (visibilityStr == qL1S("private")) {
                        intentInfo->m_visibility = IntentInfo::Private;
                    } else if (visibilityStr != qL1S("public")) {
                        throw Exception(Error::Intents, "intent visibilty '%2' is invalid on intent %1 (valid values are either 'public' or 'private'")
                                .arg(intentInfo->m_id).arg(visibilityStr);
                    }
                });
                intentFields.emplace_back(legacy ? "handledBy" : "handlingApplicationId", legacy ? false : true, [&pkgInfo, &intentInfo, &appIds](const QVariant &v) {
                    QString appId = v.toString();

                    if (appId.isEmpty()) {
                        if (pkgInfo->m_applications.count() == 1) {
                            intentInfo->m_handlingApplicationId = pkgInfo->m_applications.constFirst()->id();
                        } else {
                            throw Exception(Error::Intents, "a 'handlingApplicationId' field on intent %1 is needed if more than one application is defined")
                                    .arg(intentInfo->m_id);
                        }
                    } else {
                        if (appIds.contains(appId)) {
                            intentInfo->m_handlingApplicationId = appId;
                        } else {
                            throw Exception(Error::Intents, "the 'handlingApplicationId' field on intent %1 points to the unknown application id %2")
                                    .arg(intentInfo->m_id).arg(appId);
                        }
                    }
                });
                intentFields.emplace_back("requiredCapabilities", false, [&intentInfo](const QVariant &v) {
                    intentInfo->m_requiredCapabilities = variantToStringList(v);
                });
                intentFields.emplace_back("parameterMatch", false, [&intentInfo](const QVariant &v) {
                    intentInfo->m_parameterMatch = v.toMap();
                });
                intentFields.emplace_back("icon", false, [&intentInfo](const QVariant &v) {
                    intentInfo->m_icon = v.toString();
                });
                intentFields.emplace_back("name", false, [&intentInfo](const QVariant &v) {
                    auto nameMap = v.toMap();
                    for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
                        intentInfo->m_name.insert(it.key(), it.value().toString());
                });
                intentFields.emplace_back("description", false, [&intentInfo](const QVariant &v) {
                    auto descriptionMap = v.toMap();
                    for (auto it = descriptionMap.constBegin(); it != descriptionMap.constEnd(); ++it)
                        intentInfo->m_description.insert(it.key(), it.value().toString());
                });
                intentFields.emplace_back("categories", false, [&intentInfo](const QVariant &v) {
                    intentInfo->m_categories = variantToStringList(v);
                    intentInfo->m_categories.sort();
                });

                parseMap(intentsIt->toMap(), intentFields);
                pkgInfo->m_intents << intentInfo.take();
            }
        });

        parseMap(docs.at(1).toMap(), fields);

        if (legacy)
            pkgInfo->m_applications << legacyAppInfo.take();

        // validate the ids, runtime names and all referenced files
        pkgInfo->validate();
        return pkgInfo.take();
    } catch (const Exception &e) {
        throw Exception(e.errorCode(), "Failed to parse manifest file %1: %2").arg(QDir().relativeFilePath(filePath), e.errorString());
    }
}

QString YamlPackageScanner::metaDataFileName() const
{
    return qSL("info.yaml");
}


QT_END_NAMESPACE_AM
