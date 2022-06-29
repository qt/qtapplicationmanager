// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

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

    QString instanceId() const;
    QString mainQmlFile() const;

    bool noCache() const;
    bool clearCache() const;

    QStringList builtinAppsManifestDirs() const;
    QString documentDir() const;
    QString installationDir() const;
    QString installationDirMountPoint() const;
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
    bool allowUnknownUiClients() const;
    bool noUiWatchdog() const;
    void setForceNoUiWatchdog(bool noUiWatchdog);
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
    QString dltId() const;
    QString dltDescription() const;
    QString dltLongMessageBehavior() const;

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
    bool m_forceNoUiWatchdog = false;
    mutable QString m_installationDir; // cached value
    mutable QString m_documentDir;     // cached value
};

QT_END_NAMESPACE_AM

