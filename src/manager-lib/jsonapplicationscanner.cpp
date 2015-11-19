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

#include "exception.h"
#include "application.h"
#include "jsonapplicationscanner.h"

JsonApplicationScanner::JsonApplicationScanner()
{
}

Application *JsonApplicationScanner::scan(const QString &filePath) throw (Exception)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        throw Exception(f, "could not open file for reading");

    QJsonParseError parseError;
    QJsonDocument json = QJsonDocument::fromJson(f.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        throw Exception(Error::IO, "JSON parse error at offset %1: %2")
                .arg(parseError.offset).arg(parseError.errorString());
    }

    if (json.isEmpty() || json.isNull() || !json.isObject())
        throw Exception(Error::Parse, "not a valid JSON application meta-data file");
    QJsonObject o = json.object();

    QVariantMap map;
    map["id"] = o.value("id").toString();
    map["codeFilePath"] = o.value("code").toString();
    map["runtimeName"] = o.value("runtime").toString();
    map["runtimeParameters"] = o.value("runtimeParameters").toObject().toVariantMap();

    QJsonObject oname = o.value("name").toObject();
    QVariantMap nameMap;
    for (QJsonObject::ConstIterator it = oname.begin(); it != oname.end(); ++it)
        nameMap.insert(it.key(), it.value().toString());
    map["displayName"] = nameMap;
    map["displayIcon"] = o.value("icon").toString();
    map["preload"] = o.value("preload").toBool();
    map["importance"] = o.value("importance").toDouble();
    map["builtIn"] = o.value("built-in").toBool();
    map["type"] = o.value("type").toString();

    QJsonArray acaps = o.value("capabilities").toArray();
    QStringList capsList;
    foreach (const QJsonValue &val, acaps)
        capsList.append(val.toString());
    map["capabilities"] = capsList;

    QJsonArray acategories = o.value("categories").toArray();
    QStringList categoriesList;
    foreach (const QJsonValue &val, acategories)
        categoriesList.append(val.toString());
    map["categories"] = categoriesList;

    QJsonArray mimeTypes = o.value("mimeTypes").toArray();
    QStringList mimeList;
    foreach (const QJsonValue &val, mimeTypes)
        mimeList.append(val.toString());
    map["mimeTypes"] = mimeList;

    QString backgroundMode = o.value("backgroundMode").toString();
    QString bgmode;
    if (backgroundMode == QLatin1String("never"))
        bgmode = "Never";
    else if (backgroundMode == QLatin1String("voip"))
        bgmode = "ProvidesVoIP";
    else if (backgroundMode == QLatin1String("audio"))
        bgmode = "PlaysAudio";
    else if (backgroundMode == QLatin1String("location"))
        bgmode = "TracksLocation";
    else if (backgroundMode == QLatin1String("auto") || backgroundMode.isEmpty())
        bgmode = "Auto";
    else
        throw Exception(Error::Parse, "backgroundMode field is invalid - must be one of auto, never, voip, audio or location");
    map["backgroundMode"] = bgmode;
    map["version"] = o.value("version").toString();

    map["baseDir"] = QFileInfo(f).absoluteDir().absolutePath();


    QString appError;
    Application *app = Application::fromVariantMap(map, &appError);
    if (!app)
        throw Exception(Error::Parse, appError);

    return app;
}

QString JsonApplicationScanner::metaDataFileName() const
{
    return QLatin1String("info.json");
}
