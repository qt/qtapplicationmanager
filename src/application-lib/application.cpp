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

#include <QDebug>
#include <QDataStream>
#include <QBuffer>

#include "application.h"
#include "utilities.h"
#include "exception.h"
#include "installationreport.h"
#include "yamlapplicationscanner.h"

QT_BEGIN_NAMESPACE_AM

Application::Application()
{ }

QVariantMap Application::toVariantMap() const
{
    //TODO: only used in the installer -- replace there with code that mimicks
    // ApplicationManager::get() to get consistent key names in the objects

    QVariantMap map;
    map[qSL("id")] = m_id;
    map[qSL("codeFilePath")] = m_codeFilePath;
    map[qSL("runtimeName")] = m_runtimeName;
    map[qSL("runtimeParameters")] = m_runtimeParameters;
    QVariantMap displayName;
    for (auto it = m_name.constBegin(); it != m_name.constEnd(); ++it)
        displayName.insert(it.key(), it.value());
    map[qSL("displayName")] = displayName;
    map[qSL("displayIcon")] = m_icon;
    map[qSL("preload")] = m_preload;
    map[qSL("importance")] = m_importance;
    map[qSL("capabilities")] = m_capabilities;
    map[qSL("mimeTypes")] = m_mimeTypes;
    map[qSL("categories")] = m_categories;
    QString backgroundMode;
    switch (m_backgroundMode) {
    default:
    case Auto:           backgroundMode = qSL("Auto"); break;
    case Never:          backgroundMode = qSL("Never"); break;
    case ProvidesVoIP:   backgroundMode = qSL("ProvidesVoIP"); break;
    case PlaysAudio:     backgroundMode = qSL("PlaysAudio"); break;
    case TracksLocation: backgroundMode = qSL("TracksLocation"); break;
    }
    map[qSL("backgroundMode")] = backgroundMode;
    map[qSL("version")] = m_version;
    map[qSL("baseDir")] = m_baseDir.absolutePath();
    map[qSL("installationLocationId")] = m_installationReport ? m_installationReport->installationLocationId() : QString();
    return map;
}


QString Application::id() const
{
    return m_id;
}

QString Application::absoluteCodeFilePath() const
{
    QString code = m_nonAliased ? m_nonAliased->m_codeFilePath : m_codeFilePath;
    return code.isEmpty() ? QString() : baseDir().absoluteFilePath(code);
}

QString Application::codeFilePath() const
{
    return m_nonAliased ? m_nonAliased->m_codeFilePath : m_codeFilePath;
}

QString Application::runtimeName() const
{
    return m_nonAliased ? m_nonAliased->m_runtimeName : m_runtimeName;
}

QVariantMap Application::runtimeParameters() const
{
    return m_nonAliased ? m_nonAliased->m_runtimeParameters : m_runtimeParameters;
}

QMap<QString, QString> Application::names() const
{
    return m_name;
}

QString Application::name(const QString &language) const
{
    return m_name.value(language);
}

QString Application::icon() const
{
    return m_icon.isEmpty() ? QString() : baseDir().absoluteFilePath(m_icon);
}

QString Application::documentUrl() const
{
    return m_documentUrl;
}

int Application::lastExitCode() const
{
    return m_lastExitCode;
}

Application::ExitStatus Application::lastExitStatus() const
{
    return m_lastExitStatus;
}

bool Application::isPreloaded() const
{
    return m_nonAliased ? m_nonAliased->m_preload : m_preload;
}

qreal Application::importance() const
{
    return m_nonAliased ? m_nonAliased->m_importance : m_importance;
}

bool Application::isBuiltIn() const
{
    return m_nonAliased ? m_nonAliased->m_builtIn : m_builtIn;
}

bool Application::isAlias() const
{
    return (m_nonAliased);
}

const Application *Application::nonAliased() const
{
    return m_nonAliased;
}

QStringList Application::capabilities() const
{
    return m_nonAliased ? m_nonAliased->m_capabilities : m_capabilities;
}

QStringList Application::supportedMimeTypes() const
{
    return m_nonAliased ? m_nonAliased->m_mimeTypes : m_mimeTypes;
}

QStringList Application::categories() const
{
    return m_nonAliased ? m_nonAliased->m_categories : m_categories;
}

Application::Type Application::type() const
{
    return m_nonAliased ? m_nonAliased->m_type : m_type;
}

Application::BackgroundMode Application::backgroundMode() const
{
    return m_nonAliased ? m_nonAliased->m_backgroundMode : m_backgroundMode;
}

QString Application::version() const
{
    return m_nonAliased ? m_nonAliased->m_version : m_version;
}

void Application::validate() const throw(Exception)
{
    if (isAlias()) {
        if (!m_id.startsWith(nonAliased()->id()))
            throw Exception(Error::Parse, "aliasId '%1' does not match base application id '%2'")
                    .arg(m_id, nonAliased()->id());
    }

    QString rdnsError;
    if (!isValidDnsName(id(), isAlias(), &rdnsError))
        throw Exception(Error::Parse, "the identifier (%1) is not a valid reverse-DNS name: %2").arg(id()).arg(rdnsError);
    if (absoluteCodeFilePath().isEmpty())
        throw Exception(Error::Parse, "the 'code' field must not be empty");

    if (runtimeName().isEmpty())
        throw Exception(Error::Parse, "the 'runtimeName' field must not be empty");

    if (type() == Gui) {
        if (icon().isEmpty())
            throw Exception(Error::Parse, "the 'icon' field must not be empty");

        if (names().isEmpty())
            throw Exception(Error::Parse, "the 'name' field must not be empty");
    }

    // This check won't work during installations, since icon.png is extracted after info.json
    //        if (!QFile::exists(displayIcon()))
    //            throw Exception("the 'icon' field refers to a non-existent file");

    //TODO: check for valid capabilities
}


void Application::mergeInto(Application *app) const
{
    if (app->m_id != m_id)
        return;
    app->m_codeFilePath = m_codeFilePath;
    app->m_runtimeName = m_runtimeName;
    app->m_runtimeParameters = m_runtimeParameters;
    app->m_name = m_name;
    app->m_icon = m_icon;
    app->m_preload = m_preload;
    app->m_importance = m_importance;
    app->m_capabilities = m_capabilities;
    app->m_mimeTypes = m_mimeTypes;
    app->m_categories = m_categories;
    app->m_backgroundMode = m_backgroundMode;
    app->m_version = m_version;
}

const InstallationReport *Application::installationReport() const
{
    return m_installationReport.data();
}

void Application::setInstallationReport(InstallationReport *report)
{
    m_installationReport.reset(report);
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

uint Application::uid() const
{
    return m_nonAliased ? m_nonAliased->m_uid : m_uid;
}

void Application::setBaseDir(const QString &path)
{
    m_baseDir = path;
}

void Application::setBuiltIn(bool builtIn)
{
    m_builtIn = builtIn;
}

AbstractRuntime *Application::currentRuntime() const
{
    return m_nonAliased ? m_nonAliased->m_runtime : m_runtime;
}

void Application::setCurrentRuntime(AbstractRuntime *rt) const
{
    if (m_nonAliased)
        m_nonAliased->m_runtime = rt;
    else
        m_runtime = rt;
}

bool Application::isLocked() const
{
    return (m_nonAliased ? m_nonAliased->m_locked : m_locked).load() == 1;
}

bool Application::lock() const
{
    return (m_nonAliased ? m_nonAliased->m_locked : m_locked).testAndSetOrdered(0, 1);
}

bool Application::unlock() const
{
    return (m_nonAliased ? m_nonAliased->m_locked : m_locked).testAndSetOrdered(1, 0);
}

Application::State Application::state() const
{
    return m_nonAliased ? m_nonAliased->m_state : m_state;
}

qreal Application::progress() const
{
    return m_nonAliased ? m_nonAliased->m_progress : m_progress;
}

Application *Application::readFromDataStream(QDataStream &ds, const QVector<const Application *> &applicationDatabase) throw (Exception)
{
    QScopedPointer<Application> app(new Application);
    bool isAlias;
    qint32 backgroundMode;
    QString baseDir;
    QByteArray installationReport;

    ds >> app->m_id
       >> app->m_codeFilePath
       >> app->m_runtimeName
       >> app->m_runtimeParameters
       >> app->m_name
       >> app->m_icon
       >> app->m_documentUrl
       >> app->m_preload
       >> app->m_importance
       >> app->m_builtIn
       >> isAlias
       >> app->m_capabilities
       >> app->m_categories
       >> app->m_mimeTypes
       >> backgroundMode
       >> app->m_version
       >> baseDir
       >> app->m_uid
       >> installationReport;

    app->m_capabilities.sort();
    app->m_categories.sort();
    app->m_mimeTypes.sort();

    app->m_backgroundMode = static_cast<Application::BackgroundMode>(backgroundMode);
    app->m_baseDir.setPath(baseDir);
    if (!installationReport.isEmpty()) {
        QBuffer buffer(&installationReport);
        buffer.open(QBuffer::ReadOnly);
        app->m_installationReport.reset(new InstallationReport(app->m_id));
        if (!app->m_installationReport->deserialize(&buffer))
            app->m_installationReport.reset(0);
    }

    if (isAlias) {
        QString baseId = app->m_id.section(qL1C('@'), 0, 0);
        bool found = false;
        foreach (const Application *otherApp, applicationDatabase) {
            if (otherApp->id() == baseId) {
                app->m_nonAliased = otherApp;
                found = true;
                break;
            }
        }
        if (!found)
            throw Exception(Error::Parse, "Could not find base app id %1 for alias id %2").arg(baseId, app->m_id);
    }

    return app.take();
}

void Application::writeToDataStream(QDataStream &ds, const QVector<const Application *> &applicationDatabase) const throw (Exception)
{
    QByteArray serializedReport;

    if (auto report = installationReport()) {
        QBuffer buffer(&serializedReport);
        buffer.open(QBuffer::WriteOnly);
        report->serialize(&buffer);
    }

    ds << m_id
       << m_codeFilePath
       << m_runtimeName
       << m_runtimeParameters
       << m_name
       << m_icon
       << m_documentUrl
       << m_preload
       << m_importance
       << m_builtIn
       << bool(m_nonAliased && applicationDatabase.contains(m_nonAliased))
       << m_capabilities
       << m_categories
       << m_mimeTypes
       << qint32(m_backgroundMode)
       << m_version
       << m_baseDir.absolutePath()
       << m_uid
       << serializedReport;
}

QT_END_NAMESPACE_AM

QDebug operator<<(QDebug debug, const QT_PREPEND_NAMESPACE_AM(Application) *app)
{
    debug << "App Object:";
    if (app)
        debug << app->toVariantMap();
    else
        debug << "(null)";
    return debug;
}

