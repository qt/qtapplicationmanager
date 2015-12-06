/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
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

YamlApplicationScanner::YamlApplicationScanner()
{
}

Application *YamlApplicationScanner::scan(const QString &filePath) throw (Exception)
{
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
            || (docs.first().toMap().value(qSL("formatType")).toString() != qL1S("am-application"))
            || (docs.first().toMap().value(qSL("formatVersion")).toInt(0) != 1)) {
        throw Exception(Error::Parse, "not a valid YAML application meta-data file");
    }

    QVariantMap o = docs.at(1).toMap();

    QVariantMap map;
    map[qSL("id")] = o.value(qSL("id")).toString();
    map[qSL("codeFilePath")] = o.value(qSL("code")).toString();
    map[qSL("runtimeName")] = o.value(qSL("runtime")).toString();
    map[qSL("runtimeParameters")] = o.value(qSL("runtimeParameters")).toMap();

    map[qSL("displayName")] = o.value(qSL("name")).toMap();
    map[qSL("displayIcon")] = o.value(qSL("icon")).toString();
    map[qSL("preload")] = o.value(qSL("preload")).toBool();
    map[qSL("importance")] = o.value(qSL("importance")).toDouble();
    map[qSL("builtIn")] = o.value(qSL("built-in")).toBool();
    map[qSL("type")] = o.value(qSL("type")).toString();

    map[qSL("capabilities")] = o.value(qSL("capabilities")).toStringList();
    map[qSL("categories")] = o.value(qSL("categories")).toStringList();
    map[qSL("mimeTypes")] = o.value(qSL("mimeTypes")).toStringList();

    QString backgroundMode = o.value(qSL("backgroundMode")).toString();
    QString bgmode;
    if (backgroundMode == qL1S("never"))
        bgmode = qSL("Never");
    else if (backgroundMode == qL1S("voip"))
        bgmode = qSL("ProvidesVoIP");
    else if (backgroundMode == qL1S("audio"))
        bgmode = qSL("PlaysAudio");
    else if (backgroundMode == qL1S("location"))
        bgmode = qSL("TracksLocation");
    else if (backgroundMode == qL1S("auto") || backgroundMode.isEmpty())
        bgmode = qSL("Auto");
    else
        throw Exception(Error::Parse, "backgroundMode field is invalid - must be one of auto, never, voip, audio or location");
    map[qSL("backgroundMode")] = bgmode;
    map[qSL("version")] = o.value(qSL("version")).toString();

    map[qSL("baseDir")] = QFileInfo(f).absoluteDir().absolutePath();


    QString appError;
    Application *app = Application::fromVariantMap(map, &appError);
    if (!app)
        throw Exception(Error::Parse, appError);

    return app;
}

QString YamlApplicationScanner::metaDataFileName() const
{
    return qSL("info.yaml");
}
