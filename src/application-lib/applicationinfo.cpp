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

#include <QDataStream>
#include <QBuffer>

#include "applicationinfo.h"
#include "exception.h"
#include "installationreport.h"
#include "packageinfo.h"

QT_BEGIN_NAMESPACE_AM


//TODO Make this really unique
static int uniqueCounter = 0;
static int nextUniqueNumber() {
    uniqueCounter++;
    if (uniqueCounter > 999)
        uniqueCounter = 0;

    return uniqueCounter;
}

ApplicationInfo::ApplicationInfo(PackageInfo *packageInfo)
    : m_packageInfo(packageInfo)
    , m_uniqueNumber(nextUniqueNumber())
{ }

PackageInfo *ApplicationInfo::packageInfo() const
{
    return m_packageInfo;
}

QString ApplicationInfo::id() const
{
    return m_id;
}

int ApplicationInfo::uniqueNumber() const
{
    return m_uniqueNumber;
}

QVariantMap ApplicationInfo::applicationProperties() const
{
    return m_sysAppProperties;
}

QVariantMap ApplicationInfo::allAppProperties() const
{
    return m_allAppProperties;
}

void ApplicationInfo::writeToDataStream(QDataStream &ds) const
{
    ds << m_id
       << m_uniqueNumber
       << m_sysAppProperties
       << m_allAppProperties
       << m_codeFilePath
       << m_runtimeName
       << m_runtimeParameters
       << m_supportsApplicationInterface
       << m_capabilities
       << m_openGLConfiguration;
}

ApplicationInfo *ApplicationInfo::readFromDataStream(PackageInfo *pkg, QDataStream &ds)
{
    QScopedPointer<ApplicationInfo> app(new ApplicationInfo(pkg));

    ds >> app->m_id
       >> app->m_uniqueNumber
       >> app->m_sysAppProperties
       >> app->m_allAppProperties
       >> app->m_codeFilePath
       >> app->m_runtimeName
       >> app->m_runtimeParameters
       >> app->m_supportsApplicationInterface
       >> app->m_capabilities
       >> app->m_openGLConfiguration;

    uniqueCounter = qMax(uniqueCounter, app->m_uniqueNumber);
    app->m_capabilities.sort();

    return app.take();
}


QVariantMap ApplicationInfo::toVariantMap() const
{
    QVariantMap map;
    //TODO: check if we can find a better method to keep this as similar as possible to
    //      ApplicationManager::get().
    //      This is used for RuntimeInterface::startApplication() and the ContainerInterface

    map[qSL("id")] = m_id;
    map[qSL("uniqueNumber")] = m_uniqueNumber;

    {
        QVariantMap displayName;
        auto names = packageInfo()->names();
        for (auto it = names.constBegin(); it != names.constEnd(); ++it)
            displayName.insert(it.key(), it.value());
        map[qSL("displayName")] = displayName;
    }

    map[qSL("displayIcon")] = packageInfo()->icon();
    map[qSL("applicationProperties")] = m_allAppProperties;
    map[qSL("codeFilePath")] = m_codeFilePath;
    map[qSL("runtimeName")] = m_runtimeName;
    map[qSL("runtimeParameters")] = m_runtimeParameters;
    map[qSL("capabilities")] = m_capabilities;
    map[qSL("mimeTypes")] = m_supportedMimeTypes;

    map[qSL("categories")] = packageInfo()->categories();
    map[qSL("version")] = packageInfo()->version();
    map[qSL("baseDir")] = packageInfo()->baseDir().absolutePath();
    map[qSL("codeDir")] = map[qSL("baseDir")];     // 5.12 backward compatibility
    map[qSL("manifestDir")] = map[qSL("baseDir")]; // 5.12 backward compatibility
    map[qSL("installationLocationId")] = packageInfo()->installationReport() ? qSL("internal-0") : QString();
    map[qSL("supportsApplicationInterface")] = m_supportsApplicationInterface;
    map[qSL("dlt")] = packageInfo()->dltConfiguration();

    return map;
}

QString ApplicationInfo::absoluteCodeFilePath() const
{
    QString code = m_codeFilePath;
    return code.isEmpty() ? QString() : QDir(packageInfo()->baseDir()).absoluteFilePath(code);
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

QStringList ApplicationInfo::capabilities() const
{
    return m_capabilities;
}

QStringList ApplicationInfo::supportedMimeTypes() const
{
    return m_supportedMimeTypes;
}

QVariantMap ApplicationInfo::openGLConfiguration() const
{
    return m_openGLConfiguration;
}

bool ApplicationInfo::supportsApplicationInterface() const
{
    return m_supportsApplicationInterface;
}

QT_END_NAMESPACE_AM
