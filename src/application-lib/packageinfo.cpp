/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QDataStream>
#include <QBuffer>

#include "packageinfo.h"
#include "applicationinfo.h"
#include "intentinfo.h"
#include "exception.h"
#include "utilities.h"
#include "installationreport.h"
#include "yamlpackagescanner.h"


QT_BEGIN_NAMESPACE_AM


PackageInfo::PackageInfo()
{ }

PackageInfo::~PackageInfo()
{
    qDeleteAll(m_intents);
    qDeleteAll(m_applications);
}

void PackageInfo::validate() const Q_DECL_NOEXCEPT_EXPR(false)
{
    QString errorMsg;
    if (!isValidApplicationId(id(), &errorMsg))
        throw Exception(Error::Parse, "the identifier (%1) is not a valid package-id: %2").arg(id()).arg(errorMsg);

    if (m_applications.isEmpty())
        throw Exception(Error::Parse, "package contains no applications");

    for (const auto &app : m_applications) {
        if (!isValidApplicationId(app->id(), &errorMsg))
            throw Exception(Error::Parse, "the identifier (%1) is not a valid application-id: %2").arg(app->id()).arg(errorMsg);

        if (app->codeFilePath().isEmpty())
            throw Exception(Error::Parse, "the 'code' field must not be empty on application %1").arg(app->id());

        if (app->runtimeName().isEmpty())
            throw Exception(Error::Parse, "the 'runtimeName' field must not be empty on application %1").arg(app->id());
    }
}

QString PackageInfo::id() const
{
    return m_id;
}

QMap<QString, QString> PackageInfo::names() const
{
    return m_names;
}

QString PackageInfo::name(const QString &language) const
{
    return m_names.value(language);
}

QMap<QString, QString> PackageInfo::descriptions() const
{
    return m_descriptions;
}

QString PackageInfo::description(const QString &language) const
{
    return m_descriptions.value(language);
}

QString PackageInfo::icon() const
{
    return m_icon;
}

QStringList PackageInfo::categories() const
{
    return m_categories;
}

bool PackageInfo::isBuiltIn() const
{
    return m_builtIn;
}

void PackageInfo::setBuiltIn(bool builtIn)
{
    m_builtIn = builtIn;
}

QString PackageInfo::version() const
{
    return m_version;
}

const QDir &PackageInfo::baseDir() const

{
    return m_baseDir;
}

void PackageInfo::setBaseDir(const QDir &dir)
{
    m_baseDir = dir;
}

QVector<ApplicationInfo *> PackageInfo::applications() const
{
    return m_applications;
}

QVector<IntentInfo *> PackageInfo::intents() const
{
    return m_intents;
}

const InstallationReport *PackageInfo::installationReport() const
{
    return m_installationReport.data();
}

void PackageInfo::setInstallationReport(InstallationReport *report)
{
    m_installationReport.reset(report);
}


const quint32 PackageInfo::DataStreamVersion = 2 \
        + (ApplicationInfo::DataStreamVersion << 8) \
        + (IntentInfo::DataStreamVersion << 16);


void PackageInfo::writeToDataStream(QDataStream &ds) const
{
    //NOTE: increment DataStreamVersion above, if you make any changes here

    QByteArray serializedReport;

    if (auto report = installationReport()) {
        QBuffer buffer(&serializedReport);
        buffer.open(QBuffer::WriteOnly);
        report->serialize(&buffer);
    }

    ds << m_id
       << m_names
       << m_icon
       << m_descriptions
       << m_categories
       << m_version
       << m_builtIn
       << m_uid
       << m_baseDir.absolutePath()
       << serializedReport;

    ds << m_applications.size();
    for (const auto &app : m_applications)
        app->writeToDataStream(ds);

    ds << m_intents.size();
    for (const auto &intent : m_intents)
        intent->writeToDataStream(ds);
}

PackageInfo *PackageInfo::readFromDataStream(QDataStream &ds)
{
    //NOTE: increment DataStreamVersion above, if you make any changes here

    QScopedPointer<PackageInfo> pkg(new PackageInfo);

    QString baseDir;
    QByteArray serializedReport;

    ds >> pkg->m_id
       >> pkg->m_names
       >> pkg->m_icon
       >> pkg->m_descriptions
       >> pkg->m_categories
       >> pkg->m_version
       >> pkg->m_builtIn
       >> pkg->m_uid
       >> baseDir
       >> serializedReport;

    pkg->m_baseDir.setPath(baseDir);

    if (!serializedReport.isEmpty()) {
        QBuffer buffer(&serializedReport);
        buffer.open(QBuffer::ReadOnly);
        pkg->m_installationReport.reset(new InstallationReport(pkg->id()));
        try {
            pkg->m_installationReport->deserialize(&buffer);
        } catch (...) {
            return nullptr;
        }
    }

    int applicationsSize;
    ds >> applicationsSize;
    while (--applicationsSize >= 0) {
        if (auto app = ApplicationInfo::readFromDataStream(pkg.data(), ds))
            pkg->m_applications << app;
        else
            return nullptr;
    }

    int intentsSize;
    ds >> intentsSize;
    while (--intentsSize >= 0) {
        if (auto intent = IntentInfo::readFromDataStream(pkg.data(), ds))
            pkg->m_intents << intent;
        else
            return nullptr;
    }

    return pkg.take();
}

bool PackageInfo::isValidApplicationId(const QString &appId, QString *errorString)
{
    try {
        validateIdForFilesystemUsage(appId);
        return true;
    } catch (const Exception &e) {
        if (errorString)
            *errorString = e.errorString();
        return false;
    }
}

bool PackageInfo::isValidIcon(const QString &icon, QString *errorString)
{
    try {
        if (icon.isEmpty())
            throw Exception("empty path");

        QFileInfo fileInfo(icon);

        if (fileInfo.fileName() != icon)
            throw Exception("'%1' is not a valid file name").arg(icon);

        return true;
    } catch (const Exception &e) {
        if (errorString)
            *errorString = e.errorString();
        return false;
    }
}

QString PackageInfo::manifestPath() const
{
    return m_baseDir.filePath(m_manifestName);
}

PackageInfo *PackageInfo::fromManifest(const QString &manifestPath)
{
    return YamlPackageScanner().scan(manifestPath);
}

QT_END_NAMESPACE_AM
