/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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
#include "application.h"
#include "yamlapplicationscanner.h"

AM_BEGIN_NAMESPACE

YamlApplicationScanner::YamlApplicationScanner()
{
}

Application *YamlApplicationScanner::scan(const QString &filePath) throw (Exception)
{
    return scanInternal(filePath, false, nullptr);
}

Application *YamlApplicationScanner::scanAlias(const QString &filePath, const Application *application) throw (Exception)
{
    return scanInternal(filePath, true, application);
}

Application *YamlApplicationScanner::scanInternal(const QString &filePath, bool scanAlias, const Application *application) throw (Exception)
{
    try {
        if (scanAlias && !application)
            throw Exception(Error::System, "cannot scan an alias without a valid base application");

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly))
            throw Exception(f, "could not open file for reading");

        QtYaml::ParseError parseError;
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(f.readAll(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            throw Exception(Error::IO, "YAML parse error at line %1, column %2: %3")
                    .arg(parseError.line).arg(parseError.column).arg(parseError.errorString());
        }

        if ((docs.size() != 2)
                || (docs.first().toMap().value(qSL("formatVersion")).toInt(0) != 1)) {
            throw Exception(Error::Parse, "not a valid YAML application meta-data file");
        }

        bool isApp = (docs.first().toMap().value(qSL("formatType")).toString() == qL1S("am-application"));
        bool isAlias = (docs.first().toMap().value(qSL("formatType")).toString() == qL1S("am-application-alias"));

        if (!isApp && !isAlias)
            throw Exception(Error::Parse, "not a valid YAML application manifest");
        if (isAlias && !scanAlias)
            throw Exception(Error::Parse, "is an alias, but expected a normal manifest");
        if (!isAlias && scanAlias)
            throw Exception(Error::Parse, "is an not alias, although expected such a manifest");

        QScopedPointer<Application> app(new Application);
        app->m_baseDir = QFileInfo(f).absoluteDir().absolutePath();

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
                    app->m_nonAliased = application;
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
                if (field == "code") {
                    app->m_codeFilePath = v.toString();
                } else if (field == "runtime") {
                    app->m_runtimeName = v.toString();
                } else if (field == "runtimeParameters") {
                    app->m_runtimeParameters = v.toMap();
                } else if (field == "preload") {
                    app->m_preload = v .toBool();
                } else if (field == "importance") {
                    app->m_importance = v.toReal();
                } else if (field == "builtIn" || field == "built-in") {
                    app->m_builtIn = v.toBool();
                } else if (field == "type") {
                    app->m_type = (v.toString() == qL1S("headless") ? Application::Headless : Application::Gui);
                } else if (field == "capabilities") {
                    app->m_capabilities = v.toStringList();
                    app->m_capabilities.sort();
                } else if (field == "categories") {
                    app->m_categories = v.toStringList();
                    app->m_categories.sort();
                } else if (field == "mimeTypes") {
                    app->m_mimeTypes = v.toStringList();
                    app->m_mimeTypes.sort();
                } else if (field == "version") {
                    app->m_version = v.toString();
                } else if (field == "backgroundMode") {
                    static const QPair<const char *, Application::BackgroundMode> backgroundMap[] = {
                        { "never",    Application::Never },
                        { "voip",     Application::ProvidesVoIP },
                        { "audio",    Application::PlaysAudio },
                        { "location", Application::TracksLocation },
                        { "auto",     Application::Auto },
                        { 0,          Application::Auto }
                    };
                    QByteArray enumValue = v.toString().toLatin1();

                    bool found = false;
                    for (auto it = backgroundMap; backgroundMap->first; ++it) {
                        if (enumValue == it->first) {
                            app->m_backgroundMode = it->second;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        throw Exception(Error::Parse, "the 'backgroundMode' value '%1' is not valid").arg(enumValue);
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

AM_END_NAMESPACE
