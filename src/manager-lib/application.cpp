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

#include <QDebug>
#include <QDataStream>

#include "application.h"
#include "utilities.h"
#include "exception.h"
#include "installationreport.h"

QVariantMap Application::toVariantMap() const
{
    QVariantMap map;
    map["id"] = m_id;
    map["codeFilePath"] = m_codeFilePath;
    map["runtimeName"] = m_runtimeName;
    map["runtimeParameters"] = m_runtimeParameters;
    QVariantMap displayName;
    for (auto it = m_displayName.constBegin(); it != m_displayName.constEnd(); ++it)
        displayName.insert(it.key(), it.value());
    map["displayName"] = displayName;
    map["displayIcon"] = m_displayIcon;
    map["preload"] = m_preload;
    map["importance"] = m_importance;
    map["capabilities"] = m_capabilities;
    map["mimeTypes"] = m_mimeTypes;
    map["categories"] = m_categories;
    QString backgroundMode;
    switch (m_backgroundMode) {
    default:
    case Auto:           backgroundMode = "Auto"; break;
    case Never:          backgroundMode = "Never"; break;
    case ProvidesVoIP:   backgroundMode = "ProvidesVoIP"; break;
    case PlaysAudio:     backgroundMode = "PlaysAudio"; break;
    case TracksLocation: backgroundMode = "TracksLocation"; break;
    }
    map["backgroundMode"] = backgroundMode;
    map["version"] = m_version;
    map["baseDir"] = m_baseDir.absolutePath();
    map["installationLocationId"] = m_installationReport ? m_installationReport->installationLocationId() : QString();
    return map;
}

Application *Application::fromVariantMap(const QVariantMap &map, QString *error)
{
    QScopedPointer<Application> app(new Application());

    app->m_id = map["id"].toString();
    app->m_codeFilePath = map["codeFilePath"].toString();
    app->m_runtimeName = map["runtimeName"].toString();
    app->m_type = (map["type"].toString() == "headless" ? Headless : Gui);

    app->m_runtimeParameters = map["runtimeParameters"].toMap();
    QVariantMap nameMap = map["displayName"].toMap();
    for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
        app->m_displayName.insert(it.key(), it.value().toString());
    app->m_displayIcon = map["displayIcon"].toString();
    app->m_preload = map["preload"].toBool();
    app->m_importance = map["importance"].toReal();
    app->m_builtIn = map["builtIn"].toBool();
    app->m_capabilities = map["capabilities"].toStringList();
    app->m_capabilities.sort();
    app->m_mimeTypes = map["mimeTypes"].toStringList();
    app->m_mimeTypes.sort();
    app->m_categories = map["categories"].toStringList();
    app->m_categories.sort();
    if (map.contains("backgroundMode")) {
        QString bgmode = map["backgroundMode"].toString();
        if (bgmode == "Auto")
            app->m_backgroundMode = Auto;
        else if (bgmode == "Never")
            app->m_backgroundMode = Never;
        else if (bgmode == "ProvidesVoIP")
            app->m_backgroundMode = ProvidesVoIP;
        else if (bgmode == "PlaysAudio")
            app->m_backgroundMode = PlaysAudio;
        else if (bgmode == "TracksLocation")
            app->m_backgroundMode = TracksLocation;
        else {
            if (error)
                *error = QString::fromLatin1("the 'backgroundMode' %1 is not valid").arg(bgmode);
            return 0;
        }
    }
    app->m_version = map["version"].toString();
    app->m_baseDir = map["baseDir"].toString();

    if (!app->validate(error))
        return 0;

    return app.take();
}

bool Application::validate(QString *error) const
{
    try {
        QString rdnsError;
        if (!isValidDnsName(id(), &rdnsError))
            throw Exception(Error::Parse, "the identifier (%1) is not a valid reverse-DNS name: %2").arg(id()).arg(rdnsError);
        if (absoluteCodeFilePath().isEmpty())
            throw Exception(Error::Parse, "the 'code' field must not be empty");

        if (runtimeName().isEmpty())
            throw Exception(Error::Parse, "the 'runtimeName' field must not be empty");

        if (type() == Gui) {
            if (displayIcon().isEmpty())
                throw Exception(Error::Parse, "the 'icon' field must not be empty");

            if (displayNames().isEmpty())
                throw Exception(Error::Parse, "the 'name' field must not be empty");
        }

// This check won't work during installations, since icon.png is extracted after info.json
//        if (!QFile::exists(displayIcon()))
//            throw Exception("the 'icon' field refers to a non-existent file");

        //TODO: check for valid capabilities

        return true;
    } catch (const Exception &e) {
        if (error)
            *error = e.errorString();
        return false;
    }
}


void Application::mergeInto(Application *app) const
{
    if (app->m_id != m_id)
        return;
    app->m_codeFilePath = m_codeFilePath;
    app->m_runtimeName = m_runtimeName;
    app->m_runtimeParameters = m_runtimeParameters;
    app->m_displayName = m_displayName;
    app->m_displayIcon = m_displayIcon;
    app->m_preload = m_preload;
    app->m_importance = m_importance;
    app->m_capabilities = m_capabilities;
    app->m_mimeTypes = m_mimeTypes;
    app->m_categories = m_categories;
    app->m_backgroundMode = m_backgroundMode;
    app->m_version = m_version;
}

QDir Application::baseDir() const
{
    switch (m_state) {
    default:
    case Installed:
        return m_baseDir;
    case BeingInstalled:
    case BeingUpdated:
        return QDir(m_baseDir.absolutePath() + QLatin1Char('+'));
    case BeingRemoved:
        return QDir(m_baseDir.absolutePath() + QLatin1Char('-'));
    }
}

void Application::setBaseDir(const QString &path)
{
    m_baseDir = path;
}

Application::Application()
{ }

#include <QBuffer>

QDataStream &operator<<(QDataStream &ds, const Application &app)
{
    QByteArray installationReport;

    if (auto report = app.installationReport()) {
        QBuffer buffer(&installationReport);
        buffer.open(QBuffer::ReadWrite);
        report->serialize(&buffer);
    }

    ds << app.m_id
       << app.m_codeFilePath
       << app.m_runtimeName
       << app.m_runtimeParameters
       << app.m_displayName
       << app.m_displayIcon
       << app.m_preload
       << app.m_importance
       << app.m_builtIn
       << app.m_capabilities
       << app.m_categories
       << app.m_mimeTypes
       << qint32(app.m_backgroundMode)
       << app.m_version
       << app.m_baseDir.absolutePath()
       << app.m_uid
       << installationReport;
    return ds;
}

QDataStream &operator>>(QDataStream &ds, Application &app)
{
    qint32 backgroundMode;
    QString baseDir;
    QByteArray installationReport;

    ds >> app.m_id
       >> app.m_codeFilePath
       >> app.m_runtimeName
       >> app.m_runtimeParameters
       >> app.m_displayName
       >> app.m_displayIcon
       >> app.m_preload
       >> app.m_importance
       >> app.m_builtIn
       >> app.m_capabilities
       >> app.m_categories
       >> app.m_mimeTypes
       >> backgroundMode
       >> app.m_version
       >> baseDir
       >> app.m_uid
       >> installationReport;

    app.m_capabilities.sort();
    app.m_categories.sort();
    app.m_mimeTypes.sort();

    app.m_backgroundMode = static_cast<Application::BackgroundMode>(backgroundMode);
    app.m_baseDir.setPath(baseDir);
    if (!installationReport.isEmpty()) {
        QBuffer buffer(&installationReport);
        buffer.open(QBuffer::ReadOnly);
        app.m_installationReport.reset(new InstallationReport(app.m_id));
        if (!app.m_installationReport->deserialize(&buffer))
            app.m_installationReport.reset(0);
    }

    return ds;
}


QDebug operator<<(QDebug debug, const Application *app)
{
    debug << "App Object:";
    if (app)
        debug << app->toVariantMap();
    else
        debug << "(null)";
    return debug;
}
