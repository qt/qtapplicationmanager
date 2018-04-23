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

#include <QDebug>
#include <QBuffer>

#include "applicationinfo.h"
#include "exception.h"
#include "installationreport.h"

QT_BEGIN_NAMESPACE_AM

///////////////////////////////////////////////////////////////////////////////////////////////////
// AbstractApplicationInfo
///////////////////////////////////////////////////////////////////////////////////////////////////

bool AbstractApplicationInfo::isValidApplicationId(const QString &appId, bool isAliasName, QString *errorString)
{
    static const int maxLength = 150;

    try {
        if (appId.isEmpty())
            throw Exception(Error::Parse, "must not be empty");

        // we need to make sure that we can use the name as directory on a FAT formatted SD-card,
        // which has a 255 character path length restriction
        if (appId.length() > maxLength)
            throw Exception(Error::Parse, "the maximum length is %1 characters (found %2 characters)").arg(maxLength, appId.length());

        int aliasPos = -1;

        // aliases need to have the '@' marker
        if (isAliasName) {
            aliasPos = appId.indexOf(qL1C('@'));
            if (aliasPos < 0 || aliasPos == (appId.size() - 1))
                throw Exception(Error::Parse, "missing alias-id tag '@'");
        }

        // all characters need to be ASCII minus '@' and any filesystem special characters:
        bool spaceOnly = true;
        static const char forbiddenChars[] = "@<>:\"/\\|?*";
        for (int pos = 0; pos < appId.length(); ++pos) {
            if (pos == aliasPos)
                continue;
            ushort ch = appId.at(pos).unicode();
            if ((ch < 0x20) || (ch > 0x7f) || strchr(forbiddenChars, ch & 0xff)) {
                throw Exception(Error::Parse, "must consist of printable ASCII characters only, except any of \'%1'")
                        .arg(QString::fromLatin1(forbiddenChars));
            }
            if (spaceOnly)
                spaceOnly = QChar(ch).isSpace();
        }
        if (spaceOnly)
            throw Exception(Error::Parse, "must not consist of only white-space characters");

        return true;
    } catch (const Exception &e) {
        if (errorString)
            *errorString = e.errorString();
        return false;
    }
}

//TODO Make this really unique
static int uniqueCounter = 0;
static int nextUniqueNumber() {
    uniqueCounter++;
    if (uniqueCounter > 999)
        uniqueCounter = 0;

    return uniqueCounter;
}

AbstractApplicationInfo::AbstractApplicationInfo()
    : m_uniqueNumber(nextUniqueNumber())
{
}

QString AbstractApplicationInfo::id() const
{
    return m_id;
}

int AbstractApplicationInfo::uniqueNumber() const
{
    return m_uniqueNumber;
}

QMap<QString, QString> AbstractApplicationInfo::names() const
{
    return m_name;
}

QString AbstractApplicationInfo::name(const QString &language) const
{
    return m_name.value(language);
}

QString AbstractApplicationInfo::icon() const
{
    return m_icon;
}

QString AbstractApplicationInfo::documentUrl() const
{
    return m_documentUrl;
}

QVariantMap AbstractApplicationInfo::applicationProperties() const
{
    return m_sysAppProperties;
}

QVariantMap AbstractApplicationInfo::allAppProperties() const
{
    return m_allAppProperties;
}

void AbstractApplicationInfo::validate() const Q_DECL_NOEXCEPT_EXPR(false)
{
    QString appIdError;
    if (!isValidApplicationId(id(), isAlias(), &appIdError))
        throw Exception(Error::Parse, "the identifier (%1) is not a valid application-id: %2").arg(id()).arg(appIdError);

    if (icon().isEmpty())
        throw Exception(Error::Parse, "the 'icon' field must not be empty");

    if (names().isEmpty())
        throw Exception(Error::Parse, "the 'name' field must not be empty");

    // This check won't work during installations, since icon.png is extracted after info.json
    //        if (!QFile::exists(displayIcon()))
    //            throw Exception("the 'icon' field refers to a non-existent file");

    //TODO: check for valid capabilities
}

void AbstractApplicationInfo::read(QDataStream &ds)
{
    ds >> m_id
       >> m_uniqueNumber
       >> m_name
       >> m_icon
       >> m_documentUrl
       >> m_sysAppProperties
       >> m_allAppProperties;

    uniqueCounter = qMax(uniqueCounter, m_uniqueNumber);
}

void AbstractApplicationInfo::writeToDataStream(QDataStream &ds) const
{
    ds << isAlias()
       << m_id
       << m_uniqueNumber
       << m_name
       << m_icon
       << m_documentUrl
       << m_sysAppProperties
       << m_allAppProperties;
}

AbstractApplicationInfo *AbstractApplicationInfo::readFromDataStream(QDataStream &ds)
{
    bool isAlias;
    ds >> isAlias;

    QScopedPointer<AbstractApplicationInfo> app;

    if (isAlias)
        app.reset(new ApplicationAliasInfo);
    else
        app.reset(new ApplicationInfo);

    app->read(ds);

    return app.take();
}

void AbstractApplicationInfo::toVariantMapHelper(QVariantMap &map) const
{
    //TODO: check if we can find a better method to keep this as similar as possible to
    //      ApplicationManager::get().
    //      This is used for RuntimeInterface::startApplication(), ContainerInterface and
    //      ApplicationInstaller::taskRequestingInstallationAcknowledge.

    map[qSL("id")] = m_id;
    map[qSL("uniqueNumber")] = m_uniqueNumber;

    {
        QVariantMap displayName;
        auto names = m_name;
        for (auto it = names.constBegin(); it != names.constEnd(); ++it)
            displayName.insert(it.key(), it.value());
        map[qSL("displayName")] = displayName;
    }

    map[qSL("displayIcon")] = m_icon;
    map[qSL("applicationProperties")] = m_allAppProperties;
}

QVariantMap AbstractApplicationInfo::toVariantMap() const
{
    QVariantMap map;
    toVariantMapHelper(map);
    return map;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationInfo
///////////////////////////////////////////////////////////////////////////////////////////////////

ApplicationInfo::ApplicationInfo()
{ }

void ApplicationInfo::validate() const Q_DECL_NOEXCEPT_EXPR(false)
{
    AbstractApplicationInfo::validate();

    if (absoluteCodeFilePath().isEmpty())
        throw Exception(Error::Parse, "the 'code' field must not be empty");

    if (runtimeName().isEmpty())
        throw Exception(Error::Parse, "the 'runtimeName' field must not be empty");
}

QString ApplicationInfo::absoluteCodeFilePath() const
{
    QString code = m_codeFilePath;
    return code.isEmpty() ? QString() : QDir(codeDir()).absoluteFilePath(code);
}

QString ApplicationInfo::codeFilePath() const
{
    return m_codeFilePath;
}

QString ApplicationInfo::runtimeName() const
{
    return m_runtimeName;
}

QVariantMap ApplicationInfo::runtimeParameters() const
{
    return m_runtimeParameters;
}

bool ApplicationInfo::isBuiltIn() const
{
    return m_builtIn;
}

QStringList ApplicationInfo::capabilities() const
{
    return m_capabilities;
}

QStringList ApplicationInfo::supportedMimeTypes() const
{
    return m_mimeTypes;
}

QStringList ApplicationInfo::categories() const
{
    return m_categories;
}

QString ApplicationInfo::version() const
{
    return m_version;
}

QVariantMap ApplicationInfo::openGLConfiguration() const
{
    return m_openGLConfiguration;
}

void ApplicationInfo::setBuiltIn(bool builtIn)
{
    m_builtIn = builtIn;
}

void ApplicationInfo::setSupportsApplicationInterface(bool supportsAppInterface)
{
    m_supportsApplicationInterface = supportsAppInterface;
}

void ApplicationInfo::writeToDataStream(QDataStream &ds) const
{
    AbstractApplicationInfo::writeToDataStream(ds);

    QByteArray serializedReport;

    if (auto report = installationReport()) {
        QBuffer buffer(&serializedReport);
        buffer.open(QBuffer::WriteOnly);
        report->serialize(&buffer);
    }

    ds << m_codeFilePath
       << m_runtimeName
       << m_runtimeParameters
       << m_supportsApplicationInterface
       << m_builtIn
       << m_capabilities
       << m_categories
       << m_mimeTypes
       << m_version
       << m_openGLConfiguration
       << serializedReport
       << m_manifestDir.absolutePath()
       << m_codeDir.absolutePath()
       << m_uid;
}

bool ApplicationInfo::supportsApplicationInterface() const
{
    return m_supportsApplicationInterface;
}

void ApplicationInfo::read(QDataStream &ds)
{
    AbstractApplicationInfo::read(ds);

    QString codeDir;
    QString manifestDir;
    QByteArray installationReport;

    ds >> m_codeFilePath
       >> m_runtimeName
       >> m_runtimeParameters
       >> m_supportsApplicationInterface
       >> m_builtIn
       >> m_capabilities
       >> m_categories
       >> m_mimeTypes
       >> m_version
       >> m_openGLConfiguration
       >> installationReport
       >> manifestDir
       >> codeDir
       >> m_uid;

    m_capabilities.sort();
    m_categories.sort();
    m_mimeTypes.sort();

    m_codeDir.setPath(codeDir);
    m_manifestDir.setPath(manifestDir);
    if (!installationReport.isEmpty()) {
        QBuffer buffer(&installationReport);
        buffer.open(QBuffer::ReadOnly);
        m_installationReport.reset(new InstallationReport(m_id));
        if (!m_installationReport->deserialize(&buffer))
            m_installationReport.reset();
    }
}

void ApplicationInfo::toVariantMapHelper(QVariantMap &map) const
{
    AbstractApplicationInfo::toVariantMapHelper(map);

    map[qSL("codeFilePath")] = m_codeFilePath;
    map[qSL("runtimeName")] = m_runtimeName;
    map[qSL("runtimeParameters")] = m_runtimeParameters;
    map[qSL("capabilities")] = m_capabilities;
    map[qSL("mimeTypes")] = m_mimeTypes;
    map[qSL("categories")] = m_categories;
    map[qSL("version")] = m_version;
    map[qSL("codeDir")] = m_codeDir.absolutePath();
    map[qSL("manifestDir")] = m_manifestDir.absolutePath();
    map[qSL("installationLocationId")] = installationReport() ? installationReport()->installationLocationId()
                                                              : QString();
    map[qSL("supportsApplicationInterface")] = m_supportsApplicationInterface;
}

QT_END_NAMESPACE_AM
