/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
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
#include "applicationinfo.h"
#include "yamlapplicationscanner.h"
#include "utilities.h"

QT_BEGIN_NAMESPACE_AM

YamlApplicationScanner::YamlApplicationScanner()
{ }

ApplicationInfo *YamlApplicationScanner::scan(const QString &filePath) Q_DECL_NOEXCEPT_EXPR(false)
{
    return static_cast<ApplicationInfo*>(scanInternal(filePath, false, nullptr));
}

ApplicationAliasInfo *YamlApplicationScanner::scanAlias(const QString &filePath,
                                                        const ApplicationInfo *application) Q_DECL_NOEXCEPT_EXPR(false)
{
    return static_cast<ApplicationAliasInfo*>(scanInternal(filePath, true, application));
}

AbstractApplicationInfo *YamlApplicationScanner::scanInternal(const QString &filePath, bool scanAlias,
        const ApplicationInfo *application) Q_DECL_NOEXCEPT_EXPR(false)
{
    try {
        if (scanAlias && !application)
            throw Exception("cannot scan an alias without a valid base application");

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly))
            throw Exception(f, "could not open file for reading");

        QtYaml::ParseError parseError;
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(f.readAll(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            throw Exception(Error::IO, "YAML parse error at line %1, column %2: %3")
                    .arg(parseError.line).arg(parseError.column).arg(parseError.errorString());
        }

        try {
            checkYamlFormat(docs, 2 /*number of expected docs*/, { "am-application", "am-application-alias" }, 1);
        } catch (const Exception &e) {
            throw Exception(Error::Parse, "not a valid YAML application meta-data file: %1").arg(e.errorString());
        }

        const auto header = docs.constFirst().toMap();
        bool isApp = (header.value(qSL("formatType")).toString() == qL1S("am-application"));
        bool isAlias = (header.value(qSL("formatType")).toString() == qL1S("am-application-alias"));

        if (!isApp && !isAlias)
            throw Exception(Error::Parse, "not a valid YAML application manifest");
        if (isAlias && !scanAlias)
            throw Exception(Error::Parse, "is an alias, but expected a normal manifest");
        if (!isAlias && scanAlias)
            throw Exception(Error::Parse, "is an not alias, although expected such a manifest");

        QScopedPointer<AbstractApplicationInfo> app;
        if (isAlias)
            app.reset(new ApplicationAliasInfo);
        else {
            app.reset(new ApplicationInfo);
            auto *appInfo = static_cast<ApplicationInfo*>(app.data());
            appInfo->m_manifestDir = QFileInfo(f).absoluteDir();
            appInfo->m_codeDir = appInfo->manifestDir();
        }

        QVariantMap yaml = docs.at(1).toMap();
        for (auto it = yaml.constBegin(); it != yaml.constEnd(); ++it) {
            QByteArray field = it.key().toLatin1();
            bool unknownField = false;
            const QVariant &v = it.value();

            if ((!isAlias && (field == "id"))
                    || (isAlias && (field == "aliasId"))) {
                app->m_id = v.toString();
                if (isAlias) {
                    int sepPos = app->m_id.indexOf(qL1C('@'));
                    if (sepPos < 0 || sepPos == (app->m_id.size() - 1))
                        throw Exception(Error::Parse, "malformed aliasId '%1'").arg(app->m_id);
                    QString realId = app->m_id.left(sepPos);
                    if (application->id() != realId) {
                        throw Exception(Error::Parse, "aliasId '%1' does not match base application id '%2'")
                                .arg(app->m_id, application->id());
                    }
                }
            } else if (field == "icon") {
                app->m_icon = v.toString();
            } else if (field == "name") {
                auto nameMap = v.toMap();
                for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
                    app->m_name.insert(it.key(), it.value().toString());
            } else if (field == "documentUrl") {
                app->m_documentUrl = v.toString();
            } else if (!isAlias) {
                auto *appInfo = static_cast<ApplicationInfo*>(app.data());
                if (field == "code") {
                    appInfo->m_codeFilePath = v.toString();
                } else if (field == "runtime") {
                    appInfo->m_runtimeName = v.toString();
                } else if (field == "runtimeParameters") {
                    appInfo->m_runtimeParameters = v.toMap();
                }  else if (field == "supportsApplicationInterface") {
                    appInfo->m_supportsApplicationInterface = v.toBool();
                } else if (field == "capabilities") {
                    appInfo->m_capabilities = variantToStringList(v);
                    appInfo->m_capabilities.sort();
                } else if (field == "categories") {
                    appInfo->m_categories = variantToStringList(v);
                    appInfo->m_categories.sort();
                } else if (field == "mimeTypes") {
                    appInfo->m_mimeTypes = variantToStringList(v);
                    appInfo->m_mimeTypes.sort();
                } else if (field == "applicationProperties") {
                    const QVariantMap rawMap = v.toMap();
                    appInfo->m_sysAppProperties = rawMap.value(qSL("protected")).toMap();
                    appInfo->m_allAppProperties = appInfo->m_sysAppProperties;
                    const QVariantMap pri = rawMap.value(qSL("private")).toMap();
                    for (auto it = pri.cbegin(); it != pri.cend(); ++it)
                        appInfo->m_allAppProperties.insert(it.key(), it.value());
                } else if (field == "version") {
                    appInfo->m_version = v.toString();
                } else if (field == "opengl") {
                    appInfo->m_openGLConfiguration = v.toMap();

                    // sanity check
                    static QStringList validKeys = {
                        qSL("desktopProfile"),
                        qSL("esMajorVersion"),
                        qSL("esMinorVersion")
                    };
                    for (auto it = appInfo->m_openGLConfiguration.cbegin(); it != appInfo->m_openGLConfiguration.cend(); ++it) {
                        if (!validKeys.contains(it.key()))
                            throw Exception(Error::Parse, "the 'opengl' object contains the unsupported key '%1'").arg(it.key());
                    }
                } else {
                    unknownField = true;
                }
            } else {
                unknownField = true;
            }
            if (unknownField)
                throw Exception(Error::Parse, "contains unsupported field: '%1'").arg(field);
        }

        app->validate();
        return app.take();
    } catch (const Exception &e) {
        throw Exception(e.errorCode(), "Failed to parse manifest file %1: %2").arg(filePath, e.errorString());
    }
}


QString YamlApplicationScanner::metaDataFileName() const
{
    return qSL("info.yaml");
}

QT_END_NAMESPACE_AM
