/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** SPDX-License-Identifier: GPL-3.0
**
** $QT_BEGIN_LICENSE:GPL3$
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

    QVariantMap runtimeConfigurations() const;

    QVariantMap dbusPolicy(const QString &interfaceName) const;
    QString dbusRegistration(const QString &interfaceName) const;

    QVariantMap additionalUiConfiguration() const;

    bool applicationUserIdSeparation(uint *minUserId, uint *maxUserId, uint *commonGroupId) const;

private:
    void initialize();

    ConfigurationPrivate *d;
};

