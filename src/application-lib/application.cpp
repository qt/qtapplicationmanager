/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

/*!
    \qmltype Application
    \inqmlmodule QtApplicationManager
    \brief The handle for an application known to the ApplicationManager.

    Most of the read-only properties map directly to values read from the application's
    \c info.yaml file - these are documented in the \l{Manifest Definition}.

    \note There is an unfortunate naming conflict with the (undocumented) \c Application object
          returned from \l{QtQml::Qt::application}{Qt.application}. If you want to use the enums
          defined in the application-manager's \c Application class, you need to use an alias import:

    \badcode
    import QtApplicationManager 1.0 as AppMan

    ...

    if (app.lastExitStatus == AppMan.Application.NormalExit)
        ...
    \endcode
*/

/*!
    \qmlproperty string Application::id
    \readonly

    This property returns the unique id of the application.
*/
/*!
    \qmlproperty string Application::runtimeName
    \readonly

    This property holds the name of the runtime, necessary to run the application's code.
*/
/*!
    \qmlproperty object Application::runtimeParameters
    \readonly

    This property holds a QVariantMap that is passed onto, and interpreted by the application's
    runtime.
*/
/*!
    \qmlproperty url Application::icon
    \readonly

    The URL of the application's icon - can be used as the source property of an \l Image.
*/
/*!
    \qmlproperty string Application::documentUrl
    \readonly

    This property always returns the default \c documentUrl specified in the manifest file, even if
    a different URL was used to start the application.
*/
/*!
    \qmlproperty real Application::importance
    \readonly

    A value between \c 0.0 and \c 1.0 specifying the inverse probability of being terminated in
    out-of-memory situations (the default is \c 0.0 - unimportant).
*/
/*!
    \qmlproperty bool Application::builtIn
    \readonly

    This property describes, if this application is part of the built-in set of applications of the
    current System-UI.
*/
/*!
    \qmlproperty bool Application::alias
    \readonly

    Will return \c true if this Application object is an alias to another one.
*/
/*!
    \qmlproperty bool Application::preload
    \readonly

    When set to true, the application-manager tries to start the application immediately after boot,
    but keeps it in the background.
*/
/*!
    \qmlproperty Application Application::nonAliased
    \readonly

    If this Application object is an alias, then you can access the non-alias, base Application
    object via this property.
*/
/*!
    \qmlproperty list<string> Application::capabilities
    \readonly

    A list of special access rights for the application - these capabilities can later be queried
    and verified by the middleware via the application-manager.
*/
/*!
    \qmlproperty list<string> Application::supportedMimeTypes
    \readonly

    An array of MIME types the application can handle.
*/
/*!
    \qmlproperty list<string> Application::categories
    \readonly

    A list of category names the application should be associated with. This is mainly for the
    automated app-store uploads as well as displaying the application within a fixed set of
    categories in the System-UI.
*/
/*!
    \qmlproperty object Application::applicationProperties
    \readonly

    All user-defined properties of this application as listed in the private and protected sections
    of the \c applicationProperties field in the manifest file.
*/
/*!
    \qmlproperty Runtime Application::runtime
    \readonly

    Will return a valid \l Runtime object, if the application is currently starting, running or
    shutting down. May return a \c null object, if the application was not yet started.
*/
/*!
    \qmlproperty int Application::lastExitCode
    \readonly

    This property holds the last exit-code of the application's process in multi-process mode.
    On successful application shutdown, this value should normally be \c 0, but can be whatever
    the application returns from its \c main() function.
*/
/*!
    \qmlproperty enumeration Application::lastExitStatus
    \readonly

    This property returns the last exit-status of the application's process in multi-process mode.

    \list
    \li Application.NormalExit - The application exited normally.
    \li Application.CrashExit - The application crashed.
    \li Application.ForcedExit - The application was killed by the application-manager, since it
                                 ignored the quit request originating from a call to
                                 ApplicationManager::stopApplication.
    \endlist

    \sa ApplicationInterface::quit, ApplicationInterface::acknowledgeQuit
*/
/*!
    \qmlproperty string Application::version
    \readonly

    Holds the version of the application as a string.
*/
/*!
    \qmlproperty enumeration Application::backgroundMode
    \readonly

    Specifies if and why the application needs to be kept running in the background - can be
    one of:

    \list
    \li Application.Auto - The application does not care.
    \li Application.Never - The application should not be kept running in the background.
    \li Application.ProvidesVoIP - The application provides VoIP services while in the background.
    \li Application.PlaysAudio - The application plays audio while in the background.
    \li Application.TracksLocation - The application tracks the current location while in the
                                     background.
    \endlist

    By default, the background mode is \c Auto.
    This is just a hint for the System-UI - the application-manager itself will not enforce this
    policy.
*/


QT_BEGIN_NAMESPACE_AM

//TODO Make this really unique
static int uniqueCounter = 0;
static int nextUniqueNumber() {
    uniqueCounter++;
    if (uniqueCounter > 999)
        uniqueCounter = 0;

    return uniqueCounter;
}

Application::Application()
    : m_uniqueNumber(nextUniqueNumber())
{ }

QVariantMap Application::toVariantMap() const
{
    //TODO: check if we can find a better method to keep this as similar as possible to
    //      ApplicationManager::get().
    //      This is used for RuntimeInterface::startApplication(), ContainerInterface and
    //      ApplicationInstaller::taskRequestingInstallationAcknowledge.

    QVariantMap map;
    map[qSL("id")] = m_id;
    map[qSL("uniqueNumber")] = m_uniqueNumber;
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
    map[qSL("codeDir")] = m_codeDir.absolutePath();
    map[qSL("manifestDir")] = m_manifestDir.absolutePath();
    map[qSL("environmentVariables")] = m_environmentVariables;
    map[qSL("installationLocationId")] = m_installationReport ? m_installationReport->installationLocationId() : QString();
    map[qSL("applicationProperties")] = m_allAppProperties;
    map[qSL("supportsApplicationInterface")] = m_supportsApplicationInterface;

    return map;
}


QString Application::id() const
{
    return m_id;
}

int Application::uniqueNumber() const
{
    return m_uniqueNumber;
}

QString Application::absoluteCodeFilePath() const
{
    QString code = m_nonAliased ? m_nonAliased->m_codeFilePath : m_codeFilePath;
    return code.isEmpty() ? QString() : codeDir().absoluteFilePath(code);
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

QVariantMap Application::environmentVariables() const
{
    return m_environmentVariables;
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
    return m_icon.isEmpty() ? QString() : manifestDir().absoluteFilePath(m_icon);
}

QUrl Application::iconUrl() const
{
    return QUrl::fromLocalFile(icon());
}

QString Application::documentUrl() const
{
    return m_documentUrl;
}

bool Application::supportsApplicationInterface() const
{
    return m_supportsApplicationInterface;
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

QVariantMap Application::applicationProperties() const
{
    return m_sysAppProperties;
}

QVariantMap Application::allAppProperties() const
{
    return m_allAppProperties;
}

Application::BackgroundMode Application::backgroundMode() const
{
    return m_nonAliased ? m_nonAliased->m_backgroundMode : m_backgroundMode;
}

QString Application::version() const
{
    return m_nonAliased ? m_nonAliased->m_version : m_version;
}

void Application::validate() const Q_DECL_NOEXCEPT_EXPR(false)
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

    if (icon().isEmpty())
        throw Exception(Error::Parse, "the 'icon' field must not be empty");

    if (names().isEmpty())
        throw Exception(Error::Parse, "the 'name' field must not be empty");

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
    app->m_environmentVariables = m_environmentVariables;
    app->m_name = m_name;
    app->m_icon = m_icon;
    app->m_documentUrl = m_documentUrl;
    app->m_allAppProperties = m_allAppProperties;
    app->m_sysAppProperties = m_sysAppProperties;
    app->m_supportsApplicationInterface = m_supportsApplicationInterface;
    app->m_preload = m_preload;
    app->m_importance = m_importance;
    app->m_capabilities = m_capabilities;
    app->m_categories = m_categories;
    app->m_mimeTypes = m_mimeTypes;
    app->m_backgroundMode = m_backgroundMode;
    app->m_version = m_version;
    emit app->bulkChange();
}

const InstallationReport *Application::installationReport() const
{
    return m_installationReport.data();
}

void Application::setInstallationReport(InstallationReport *report)
{
    m_installationReport.reset(report);
}

QDir Application::manifestDir() const
{
    return m_manifestDir;
}

QDir Application::codeDir() const
{
    switch (m_state) {
    default:
    case Installed:
        return m_codeDir;
    case BeingInstalled:
    case BeingUpdated:
        return QDir(m_codeDir.absolutePath() + QLatin1Char('+'));
    case BeingRemoved:
        return QDir(m_codeDir.absolutePath() + QLatin1Char('-'));
    }
}

uint Application::uid() const
{
    return m_nonAliased ? m_nonAliased->m_uid : m_uid;
}

void Application::setCodeDir(const QString &path)
{
    m_codeDir = path;
}

void Application::setManifestDir(const QString &path)
{
    m_manifestDir = path;
}

void Application::setBuiltIn(bool builtIn)
{
    m_builtIn = builtIn;
}

void Application::setSupportsApplicationInterface(bool supportsAppInterface)
{
    m_supportsApplicationInterface = supportsAppInterface;
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
    emit runtimeChanged();
}

bool Application::isBlocked() const
{
    return (m_nonAliased ? m_nonAliased->m_blocked : m_blocked).load() == 1;
}

bool Application::block() const
{
    return (m_nonAliased ? m_nonAliased->m_blocked : m_blocked).testAndSetOrdered(0, 1);
}

bool Application::unblock() const
{
    return (m_nonAliased ? m_nonAliased->m_blocked : m_blocked).testAndSetOrdered(1, 0);
}

Application::State Application::state() const
{
    return m_nonAliased ? m_nonAliased->m_state : m_state;
}

qreal Application::progress() const
{
    return m_nonAliased ? m_nonAliased->m_progress : m_progress;
}

Application *Application::readFromDataStream(QDataStream &ds, const QVector<const Application *> &applicationDatabase) Q_DECL_NOEXCEPT_EXPR(false)
{
    QScopedPointer<Application> app(new Application);
    bool isAlias;
    qint32 backgroundMode;
    QString codeDir;
    QString manifestDir;
    QByteArray installationReport;

    ds >> app->m_id
       >> app->m_uniqueNumber
       >> app->m_codeFilePath
       >> app->m_runtimeName
       >> app->m_runtimeParameters
       >> app->m_name
       >> app->m_icon
       >> app->m_documentUrl
       >> app->m_allAppProperties
       >> app->m_sysAppProperties
       >> app->m_supportsApplicationInterface
       >> app->m_preload
       >> app->m_importance
       >> app->m_builtIn
       >> isAlias
       >> app->m_capabilities
       >> app->m_categories
       >> app->m_mimeTypes
       >> backgroundMode
       >> app->m_version
       >> codeDir
       >> manifestDir
       >> app->m_uid
       >> app->m_environmentVariables
       >> installationReport;

    uniqueCounter = qMax(uniqueCounter, app->m_uniqueNumber);

    app->m_capabilities.sort();
    app->m_categories.sort();
    app->m_mimeTypes.sort();

    app->m_backgroundMode = static_cast<Application::BackgroundMode>(backgroundMode);
    app->m_codeDir.setPath(codeDir);
    app->m_manifestDir.setPath(manifestDir);
    if (!installationReport.isEmpty()) {
        QBuffer buffer(&installationReport);
        buffer.open(QBuffer::ReadOnly);
        app->m_installationReport.reset(new InstallationReport(app->m_id));
        if (!app->m_installationReport->deserialize(&buffer))
            app->m_installationReport.reset();
    }

    if (isAlias) {
        QString baseId = app->m_id.section(qL1C('@'), 0, 0);
        bool found = false;
        for (const Application *otherApp : applicationDatabase) {
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

void Application::writeToDataStream(QDataStream &ds, const QVector<const Application *> &applicationDatabase) const Q_DECL_NOEXCEPT_EXPR(false)
{
    QByteArray serializedReport;

    if (auto report = installationReport()) {
        QBuffer buffer(&serializedReport);
        buffer.open(QBuffer::WriteOnly);
        report->serialize(&buffer);
    }

    ds << m_id
       << m_uniqueNumber
       << m_codeFilePath
       << m_runtimeName
       << m_runtimeParameters
       << m_name
       << m_icon
       << m_documentUrl
       << m_allAppProperties
       << m_sysAppProperties
       << m_supportsApplicationInterface
       << m_preload
       << m_importance
       << m_builtIn
       << bool(m_nonAliased && applicationDatabase.contains(m_nonAliased))
       << m_capabilities
       << m_categories
       << m_mimeTypes
       << qint32(m_backgroundMode)
       << m_version
       << m_codeDir.absolutePath()
       << m_manifestDir.absolutePath()
       << m_uid
       << m_environmentVariables
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

