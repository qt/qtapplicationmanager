/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QStringList>
#include <QVariantMap>

class ConfigurationPrivate;

class Configuration
{
public:
    Configuration();

    QString mainQmlFile() const;
    QString database() const;
    bool recreateDatabase() const;

    QString builtinAppsManifestDir() const;
    QString installedAppsManifestDir() const;
    QString appImageMountDir() const;

    bool fullscreen() const;
    bool noFullscreen() const;
    QString windowIcon() const;
    QStringList importPaths() const;
    bool verbose() const;
    bool slowAnimations() const;
    bool loadDummyData() const;
    bool noSecurity() const;
    bool noUiWatchdog() const;
    bool forceSingleProcess() const;
    QString singleApp() const;
    QStringList loggingRules() const;

    QVariantList installationLocations() const;

    QVariantMap containerConfigurations() const;
    QVariantMap runtimeConfigurations() const;

    QVariantMap dbusPolicy(const QString &interfaceName) const;
    QString dbusRegistration(const QString &interfaceName) const;

    QVariantMap additionalUiConfiguration() const;

    bool applicationUserIdSeparation(uint *minUserId, uint *maxUserId, uint *commonGroupId) const;

    qreal quickLaunchIdleLoad() const;
    int quickLaunchRuntimesPerContainer() const;

    QString waylandSocketName() const;

    QString telnetAddress() const;
    quint16 telnetPort() const;

private:
    void initialize();

    ConfigurationPrivate *d;
};

