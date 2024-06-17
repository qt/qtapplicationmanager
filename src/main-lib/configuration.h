// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QtAppManCommon/global.h>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>

#include <chrono>
#include <memory>

#include "openglconfiguration.h"
#include "watchdogconfiguration.h"

QT_BEGIN_NAMESPACE_AM

// IMPORTANT: if you add/remove/change anything in this struct, you also have to adjust the
//            dataStreamVersion(), serialize() and merge() functions in the cpp file!
struct ConfigurationData
{
    QString instanceId;

    struct Runtimes {
        QStringList additionalLaunchers;
        QVariantMap configurations;
    } runtimes;

    struct {
        QVariantMap configurations;
        QList<std::pair<QString, QString>> selection;
    } containers;

    struct {
        struct {
            std::chrono::milliseconds disambiguation { 10000 };
            std::chrono::milliseconds startApplication { 3000 };
            std::chrono::milliseconds replyFromApplication { 5000 };
            std::chrono::milliseconds replyFromSystem { 20000 };
        } timeouts;
    } intents;

    struct {
        QStringList startup;
        QStringList container;
    } plugins;

    struct {
        struct {
            QString id;
            QString description;
            QString longMessageBehavior;
        } dlt;
        QStringList rules;
        QString messagePattern;
        QVariant useAMConsoleLogger; // true / false / invalid
    } logging;

    struct {
        QStringList caCertificates;
    } installer;

    struct {
        QVariantMap policies;
        QVariantMap registrations;
    } dbus;

    struct {
        double idleLoad = 0.;
        QMap<std::pair<QString, QString>, int> runtimesPerContainer;
        int failedStartLimit = 5;
        std::chrono::seconds failedStartLimitIntervalSec { 10 };
    } quicklaunch;

    struct Ui {
        OpenGLConfiguration opengl;
        QStringList iconThemeSearchPaths;
        QString iconThemeName;
        QString style;
        bool loadDummyData = false;
        QStringList importPaths;
        QStringList pluginPaths;
        QString windowIcon;
        bool fullscreen = false;
        QString mainQml;
        QStringList resources;
    } ui;

    struct {
        QStringList builtinAppsManifestDir;
        QString installationDir;
        QString documentDir;
        QString installationDirMountPoint;
    } applications; // TODO: rename to package?

    struct {
        bool printBacktrace = true;
        bool printQmlStack = true;
        std::chrono::seconds waitForGdbAttach { 0 };
        bool dumpCore = true;
        struct {
            int onCrash = -1;
            int onException = -1;
        } stackFramesToIgnore;
    } crashAction;

    QVariantMap systemProperties;

    struct {
        bool forceSingleProcess = false;
        bool forceMultiProcess = false;
        bool noSecurity = false;
        bool developmentMode = false;
        bool allowUnsignedPackages = false;
        bool allowUnknownUiClients = false;
    } flags;

    struct Wayland {
        QString socketName;
        struct ExtraSocket {
            QString path;
            int permissions = -1;
            int userId = -1;
            int groupId = -1;
        };
        QList<ExtraSocket> extraSockets;
    } wayland;

    WatchdogConfiguration watchdog;
};

class ConfigurationPrivate;

class Configuration
{
public:
    Configuration(const QStringList &defaultConfigFilePaths, const QString &buildConfigFilePath,
                  const char *additionalDescription = nullptr, bool onlyOnePositionalArgument = true);
    Configuration(const char *additionalDescription = nullptr, bool onlyOnePositionalArgument = true);

    virtual ~Configuration();
    virtual void parseWithArguments(const QStringList &arguments);
    QVariant buildConfig() const;

    // all command line arguments that do not map to YAML fields
    bool noCache() const;
    bool clearCache() const;
    bool verbose() const;
    void setForceVerbose(bool forceVerbose);
    bool isWatchdogDisabled() const;
    void setForceDisableWatchdog(bool forceDisableWatchdog);
    bool slowAnimations() const;
    bool noDltLogging() const;
    bool qmlDebugging() const;
    QString singleApp() const;
    QString dbus() const;
    QString waylandSocketName() const;

    // for backwards compatibility
    inline bool fullscreen() const { return yaml.ui.fullscreen; }
    inline bool noFullscreen() const { return !yaml.ui.fullscreen; }

    // the testrunner only
    QStringList testRunnerArguments() const;
    QString testRunnerSourceFile() const;

private:
    std::unique_ptr<ConfigurationPrivate> d;

public:
    // the YAML configuration, mapped (nearly) 1:1 to nested C structs
    const ConfigurationData &yaml;
};

QT_END_NAMESPACE_AM


#endif // CONFIGURATION_H
