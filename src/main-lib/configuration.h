/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
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

#pragma once

#include <QtAppManCommon/global.h>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QCommandLineParser>

QT_BEGIN_NAMESPACE_AM

struct ConfigurationData;

class Configuration
{
public:
    Configuration(const QStringList &defaultConfigFilePaths, const QString &buildConfigFilePath,
                  const char *additionalDescription = nullptr, bool onlyOnePositionalArgument = true);
    Configuration(const char *additionalDescription = nullptr, bool onlyOnePositionalArgument = true);

    virtual ~Configuration();
    Q_DECL_DEPRECATED void parse(QStringList *deploymentWarnings);
    Q_DECL_DEPRECATED void parse();
    Q_DECL_DEPRECATED virtual void parseWithArguments(const QStringList &arguments, QStringList *deploymentWarnings);
    virtual void parseWithArguments(const QStringList &arguments);
    QVariant buildConfig() const;

    QString mainQmlFile() const;

    bool noCache() const;
    bool clearCache() const;

    QStringList builtinAppsManifestDirs() const;
    QString documentDir() const;
    QString installationDir() const;
    bool disableInstaller() const;
    bool disableIntents() const;
    int intentTimeoutForDisambiguation() const;
    int intentTimeoutForStartApplication() const;
    int intentTimeoutForReplyFromApplication() const;
    int intentTimeoutForReplyFromSystem() const;

    bool fullscreen() const;
    bool noFullscreen() const;
    QString windowIcon() const;
    QStringList importPaths() const;
    QStringList pluginPaths() const;
    bool verbose() const;
    void setForceVerbose(bool forceVerbose);
    bool slowAnimations() const;
    bool loadDummyData() const;
    bool noSecurity() const;
    bool developmentMode() const;
    bool allowUnsignedPackages() const;
    bool noUiWatchdog() const;
    bool noDltLogging() const;
    bool forceSingleProcess() const;
    bool forceMultiProcess() const;
    bool qmlDebugging() const;
    QString singleApp() const;
    QStringList loggingRules() const;
    QString messagePattern() const;
    QVariant useAMConsoleLogger() const;
    QString style() const;
    QString iconThemeName() const;
    QStringList iconThemeSearchPaths() const;
    bool enableTouchEmulation() const;
    QString dltId() const;
    QString dltDescription() const;
    QStringList resources() const;

    QVariantMap openGLConfiguration() const;

    QVariantList installationLocations() const;

    QList<QPair<QString, QString>> containerSelectionConfiguration() const;
    QVariantMap containerConfigurations() const;
    QVariantMap runtimeConfigurations() const;

    QVariantMap dbusPolicy(const char *interfaceName) const;
    QString dbusRegistration(const char *interfaceName) const;

    QVariantMap rawSystemProperties() const;

    bool applicationUserIdSeparation(uint *minUserId, uint *maxUserId, uint *commonGroupId) const;

    qreal quickLaunchIdleLoad() const;
    int quickLaunchRuntimesPerContainer() const;

    QString waylandSocketName() const;
    QVariantList waylandExtraSockets() const;

    QVariantMap managerCrashAction() const;

    QStringList caCertificates() const;

    QStringList pluginFilePaths(const char *type) const;

    QStringList testRunnerArguments() const;

private:
    enum MessageType { UsageMessage, ErrorMessage };

    void showParserMessage(const QString &message, MessageType type);

    template <typename T> T value(const char *clname, const T &cfvalue = T()) const
    {
        Q_UNUSED(clname)
        Q_UNUSED(cfvalue)
        return T();
    }

    QStringList m_defaultConfigFilePaths;
    QString m_buildConfigFilePath;
    QCommandLineParser m_clp;
    QScopedPointer<ConfigurationData> m_data;
    QString m_mainQmlFile;
    bool m_onlyOnePositionalArgument = false;
    bool m_forceVerbose = false;
    mutable QString m_installationDir; // cached value
    mutable QString m_documentDir;     // cached value
};

QT_END_NAMESPACE_AM

