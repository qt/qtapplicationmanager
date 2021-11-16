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

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QMetaEnum>
#include <QProcessEnvironment>
#include <private/qvariant_p.h>

#include <QDataStream>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>

#if !defined(AM_HEADLESS)
#  include <QGuiApplication>
#endif

#include <functional>

#if defined(Q_OS_LINUX)
#  include <sys/file.h>
#endif

#include "global.h"
#include "logging.h"
#include "qtyaml.h"
#include "configcache.h"
#include "utilities.h"
#include "exception.h"
#include "utilities.h"
#include "configuration.h"
#include "configuration_p.h"

QT_BEGIN_NAMESPACE_AM


template<> bool Configuration::value(const char *clname, const bool &cfvalue) const
{
    return (clname && m_clp.isSet(qL1S(clname))) || cfvalue;
}

template<> QString Configuration::value(const char *clname, const QString &cfvalue) const
{
    return (clname && m_clp.isSet(qL1S(clname))) ? m_clp.value(qL1S(clname)) : cfvalue;
}

template<> QStringList Configuration::value(const char *clname, const QStringList &cfvalue) const
{
    QStringList result;
    if (clname)
        result = m_clp.values(qL1S(clname));
    if (!cfvalue.isEmpty())
        result += cfvalue;
    return result;
}


// the templated adaptor class needed to instantiate ConfigCache<ConfigurationData> in parse() below
template<> class ConfigCacheAdaptor<ConfigurationData>
{
public:
    static ConfigurationData *loadFromSource(QIODevice *source, const QString &fileName)
    {
        return ConfigurationData::loadFromSource(source, fileName);
    }
    void preProcessSourceContent(QByteArray &sourceContent, const QString &fileName)
    {
        sourceContent = ConfigurationData::substituteVars(sourceContent, fileName);
    }
    ConfigurationData *loadFromCache(QDataStream &ds)
    {
        return ConfigurationData::loadFromCache(ds);
    }
    void saveToCache(QDataStream &ds, const ConfigurationData *cd)
    {
        cd->saveToCache(ds);
    }
    static void merge(ConfigurationData *to, const ConfigurationData *from)
    {
        to->mergeFrom(from);
    }
};


Configuration::Configuration(const char *additionalDescription,
                             bool onlyOnePositionalArgument)
    : Configuration(QStringList(), qSL(":/build-config.yaml"),
                    additionalDescription, onlyOnePositionalArgument)
{ }

Configuration::Configuration(const QStringList &defaultConfigFilePaths,
                             const QString &buildConfigFilePath,
                             const char *additionalDescription,
                             bool onlyOnePositionalArgument)
    : m_defaultConfigFilePaths(defaultConfigFilePaths)
    , m_buildConfigFilePath(buildConfigFilePath)
    , m_onlyOnePositionalArgument(onlyOnePositionalArgument)
{
    // using QStringLiteral for all strings here adds a few KB of ro-data, but will also improve
    // startup times slightly: less allocations and copies. MSVC cannot cope with multi-line though

    const char *description =
        "In addition to the command line options below, the following environment\n"
        "variables can be set:\n\n"
        "  AM_STARTUP_TIMER  If set to 1, a startup performance analysis will be printed\n"
        "                    on the console. Anything other than 1 will be interpreted\n"
        "                    as the name of a file that is used instead of the console.\n"
        "\n"
        "  AM_FORCE_COLOR_OUTPUT  Can be set to 'on' to force color output to the console\n"
        "                         and to 'off' to disable it. Any other value will result\n"
        "                         in the default, auto-detection behavior.\n";

    m_clp.setApplicationDescription(qSL("\n") + QCoreApplication::applicationName() + qSL("\n\n")
                                    + (additionalDescription ? (qL1S(additionalDescription) + qSL("\n\n")) : QString())
                                    + qL1S(description));

    m_clp.addOption({ { qSL("h"), qSL("help")
#if defined(Q_OS_WINDOWS)
                        , qSL("?")
#endif
                      },                           qSL("Displays this help.") });
    m_clp.addOption({ qSL("version"),              qSL("Displays version information.") });
    QCommandLineOption cf { { qSL("c"), qSL("config-file") },
                                                   qSL("Load configuration from file (can be given multiple times)."), qSL("files") };
    cf.setDefaultValues(m_defaultConfigFilePaths);
    m_clp.addOption(cf);
    m_clp.addOption({ { qSL("o"), qSL("option") }, qSL("Override a specific config option."), qSL("yaml-snippet") });
    m_clp.addOption({ { qSL("no-cache"), qSL("no-config-cache") },
                                                   qSL("Disable the use of the config and appdb file cache.") });
    m_clp.addOption({ { qSL("clear-cache"), qSL("clear-config-cache") },
                                                   qSL("Ignore an existing config and appdb file cache.") });
    m_clp.addOption({ { qSL("r"), qSL("recreate-database") },
                                                   qSL("Backwards compatibility: synonyms for --clear-cache.") });
    if (!buildConfigFilePath.isEmpty())
        m_clp.addOption({ qSL("build-config"),     qSL("Dumps the build configuration and exits.") });

    m_clp.addPositionalArgument(qSL("qml-file"),   qSL("The main QML file."));
    m_clp.addOption({ qSL("log-instant"),          qSL("Log instantly at start-up, neglect logging configuration.") });
    m_clp.addOption({ qSL("database"),             qSL("Deprecated (ignored)."), qSL("file") });
    m_clp.addOption({ qSL("builtin-apps-manifest-dir"), qSL("Base directory for built-in application manifests."), qSL("dir") });
    m_clp.addOption({ qSL("installation-dir"),     qSL("Base directory for package installations."), qSL("dir") });
    m_clp.addOption({ qSL("document-dir"),         qSL("Base directory for per-package document directories."), qSL("dir") });
    m_clp.addOption({ qSL("installed-apps-manifest-dir"), qSL("Deprecated (ignored)."), qSL("dir") });
    m_clp.addOption({ qSL("app-image-mount-dir"),  qSL("Deprecated (ignored)."), qSL("dir") });
    m_clp.addOption({ qSL("disable-installer"),    qSL("Disable the application installer sub-system.") });
    m_clp.addOption({ qSL("disable-intents"),      qSL("Disable the intents sub-system.") });
#if defined(QT_DBUS_LIB)
    m_clp.addOption({ qSL("dbus"),                 qSL("Register on the specified D-Bus."), qSL("<bus>|system|session|none|auto"), qSL("auto") });
    m_clp.addOption({ qSL("start-session-dbus"),   qSL("Deprecated (ignored).") });
#endif
    m_clp.addOption({ qSL("fullscreen"),           qSL("Display in full-screen.") });
    m_clp.addOption({ qSL("no-fullscreen"),        qSL("Do not display in full-screen.") });
    m_clp.addOption({ qSL("I"),                    qSL("Additional QML import path."), qSL("dir") });
    m_clp.addOption({ { qSL("v"), qSL("verbose") }, qSL("Verbose output.") });
    m_clp.addOption({ qSL("slow-animations"),      qSL("Run all animations in slow motion.") });
    m_clp.addOption({ qSL("load-dummydata"),       qSL("Loads QML dummy-data.") });
    m_clp.addOption({ qSL("no-security"),          qSL("Disables all security related checks (dev only!)") });
    m_clp.addOption({ qSL("development-mode"),     qSL("Enable development mode, allowing installation of dev-signed packages.") });
    m_clp.addOption({ qSL("no-ui-watchdog"),       qSL("Disables detecting hung UI applications (e.g. via Wayland's ping/pong).") });
    m_clp.addOption({ qSL("no-dlt-logging"),       qSL("Disables logging using automotive DLT.") });
    m_clp.addOption({ qSL("force-single-process"), qSL("Forces single-process mode even on a wayland enabled build.") });
    m_clp.addOption({ qSL("force-multi-process"),  qSL("Forces multi-process mode. Will exit immediately if this is not possible.") });
    m_clp.addOption({ qSL("wayland-socket-name"),  qSL("Use this file name to create the wayland socket."), qSL("socket") });
    m_clp.addOption({ qSL("single-app"),           qSL("Runs a single application only (ignores the database)"), qSL("info.yaml file") }); // rename single-package
    m_clp.addOption({ qSL("logging-rule"),         qSL("Adds a standard Qt logging rule."), qSL("rule") });
    m_clp.addOption({ qSL("qml-debug"),            qSL("Enables QML debugging and profiling.") });
    m_clp.addOption({ qSL("enable-touch-emulation"), qSL("Enables the touch emulation, converting mouse to touch events.") });
}

QVariant Configuration::buildConfig() const
{
    QFile f(m_buildConfigFilePath);
    if (f.open(QFile::ReadOnly)) {
        try {
            return YamlParser::parseAllDocuments(f.readAll()).toList();
        } catch (...) {
        }
    }
    return QVariant();
}

Configuration::~Configuration()
{ }

// vvvv copied from QCommandLineParser ... why is this not public API?

#if defined(Q_OS_ANDROID)
#  include <android/log.h>
#elif defined(Q_OS_WIN) && !defined(Q_OS_WINCE) && !defined(Q_OS_WINRT)
#  include <windows.h>

// Return whether to use a message box. Use handles if a console can be obtained
// or we are run with redirected handles (for example, by QProcess).
static inline bool displayMessageBox()
{
    if (GetConsoleWindow())
        return false;
    STARTUPINFO startupInfo;
    startupInfo.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&startupInfo);
    return !(startupInfo.dwFlags & STARTF_USESTDHANDLES);
}
#endif // Q_OS_WIN && !QT_BOOTSTRAPPED && !Q_OS_WIN && !Q_OS_WINRT

void Configuration::showParserMessage(const QString &message, MessageType type)
{
#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE) && !defined(Q_OS_WINRT)
    if (displayMessageBox()) {
        const UINT flags = MB_OK | MB_TOPMOST | MB_SETFOREGROUND
            | (type == UsageMessage ? MB_ICONINFORMATION : MB_ICONERROR);
        QString title;
        if (QCoreApplication::instance())
            title = QCoreApplication::instance()->property("applicationDisplayName").toString();
        if (title.isEmpty())
            title = QCoreApplication::applicationName();
        MessageBoxW(0, reinterpret_cast<const wchar_t *>(message.utf16()),
                    reinterpret_cast<const wchar_t *>(title.utf16()), flags);
        return;
    }
#elif defined(Q_OS_ANDROID)
    static QByteArray appName = QCoreApplication::applicationName().toLocal8Bit();

    __android_log_write(type == UsageMessage ? ANDROID_LOG_WARN : ANDROID_LOG_ERROR,
                        appName.constData(), qPrintable(message));
    return;

#endif // Q_OS_WIN && !QT_BOOTSTRAPPED && !Q_OS_WIN && !Q_OS_WINRT
    fputs(qPrintable(message), type == UsageMessage ? stdout : stderr);
}

// ^^^^ copied from QCommandLineParser ... why is this not public API?

void Configuration::parse(QStringList *deploymentWarnings)
{
    Q_UNUSED(deploymentWarnings)
    parseWithArguments(QCoreApplication::arguments());
}

void Configuration::parse()
{
    parseWithArguments(QCoreApplication::arguments());
}

void Configuration::parseWithArguments(const QStringList &arguments, QStringList *deploymentWarnings)
{
    Q_UNUSED(deploymentWarnings)
    parseWithArguments(arguments);
}

void Configuration::parseWithArguments(const QStringList &arguments)
{
    if (!m_clp.parse(arguments)) {
        showParserMessage(m_clp.errorText() + qL1C('\n'), ErrorMessage);
        exit(1);
    }

    if (m_clp.isSet(qSL("version")))
        m_clp.showVersion();

    if (m_clp.isSet(qSL("help")))
        m_clp.showHelp();

    if (!m_buildConfigFilePath.isEmpty() && m_clp.isSet(qSL("build-config"))) {
        QFile f(m_buildConfigFilePath);
        if (f.open(QFile::ReadOnly)) {
            showParserMessage(QString::fromLocal8Bit(f.readAll()), UsageMessage);
            exit(0);
        } else {
            showParserMessage(qL1S("Could not find the embedded build config.\n"), ErrorMessage);
            exit(1);
        }
    }

#if defined(AM_TIME_CONFIG_PARSING)
    QElapsedTimer timer;
    timer.start();
#endif

    QStringList configFilePaths = m_clp.values(qSL("config-file"));

    AbstractConfigCache::Options cacheOptions = AbstractConfigCache::MergedResult;
    if (noCache())
        cacheOptions |= AbstractConfigCache::NoCache;
    if (clearCache())
        cacheOptions |= AbstractConfigCache::ClearCache;

    if (configFilePaths.isEmpty()) {
        m_data.reset(new ConfigurationData());
    } else {
        ConfigCache<ConfigurationData> cache(configFilePaths, qSL("config"), "CFGD",
                                             ConfigurationData::DataStreamVersion, cacheOptions);

        try {
            cache.parse();
            m_data.reset(cache.takeMergedResult());
            if (m_data.isNull())
                m_data.reset(new ConfigurationData());
        } catch (const Exception &e) {
            showParserMessage(e.errorString() + qL1C('\n'), ErrorMessage);
            exit(1);
        }
    }

    const QStringList options = m_clp.values(qSL("o"));
    for (const QString &option : options) {
        QByteArray yaml("formatVersion: 1\nformatType: am-configuration\n---\n");
        yaml.append(option.toUtf8());
        QBuffer buffer(&yaml);
        buffer.open(QIODevice::ReadOnly);
        try {
            ConfigurationData *cd = ConfigCacheAdaptor<ConfigurationData>::loadFromSource(&buffer, qSL("command line"));
            if (cd) {
                ConfigCacheAdaptor<ConfigurationData>::merge(m_data.data(), cd);
                delete cd;
            }
        } catch (const Exception &e) {
            showParserMessage(QString::fromLatin1("Could not parse --option value: %1.\n")
                              .arg(e.errorString()),
                              ErrorMessage);
            exit(1);
        }
    }

    // early sanity checks
    if (m_onlyOnePositionalArgument && (m_clp.positionalArguments().size() > 1)) {
        showParserMessage(qL1S("Only one main qml file can be specified.\n"), ErrorMessage);
        exit(1);
    }

    if (installationDir().isEmpty()) {
        const auto ilocs = m_data->installationLocations;
        if (!ilocs.isEmpty()) {
            qCWarning(LogDeployment) << "Support for \"installationLocations\" in the main config file "
                                        "has been removed:";
        }

        for (const auto iloc : ilocs) {
            QVariantMap map = iloc.toMap();
            QString id = map.value(qSL("id")).toString();
            if (id == qSL("internal-0")) {
                m_installationDir = map.value(qSL("installationPath")).toString();
                m_documentDir = map.value(qSL("documentPath")).toString();
                qCWarning(LogDeployment) << " * still using installation location \"internal-0\" for backward "
                                            "compatibility";
            } else {
                qCWarning(LogDeployment) << " * ignoring installation location" << id;
            }
        }
    }

    if (installationDir().isEmpty()) {
        qCWarning(LogDeployment) << "No --installation-dir command line parameter or applications/installationDir "
                                    "configuration key specified. It won't be possible to install, remove or "
                                    "access installable packages.";
    }

    if (value<bool>("start-session-dbus"))
        qCDebug(LogDeployment) << "ignoring '--start-session-dbus'";
}


const quint32 ConfigurationData::DataStreamVersion = 4;


ConfigurationData *ConfigurationData::loadFromCache(QDataStream &ds)
{
    // NOTE: increment DataStreamVersion above, if you make any changes here

    // IMPORTANT: when doing changes to ConfigurationData, remember to adjust all of
    //            loadFromCache(), saveToCache() and mergeFrom() at the same time!

    ConfigurationData *cd = new ConfigurationData;
    ds >> cd->runtimes.configurations
       >> cd->containers.configurations
       >> cd->containers.selection
       >> cd->intents.disable
       >> cd->intents.timeouts.disambiguation
       >> cd->intents.timeouts.startApplication
       >> cd->intents.timeouts.replyFromApplication
       >> cd->intents.timeouts.replyFromSystem
       >> cd->plugins.startup
       >> cd->plugins.container
       >> cd->logging.dlt.id
       >> cd->logging.dlt.description
       >> cd->logging.rules
       >> cd->logging.messagePattern
       >> cd->logging.useAMConsoleLogger
       >> cd->installer.disable
       >> cd->installer.caCertificates
       >> cd->installer.applicationUserIdSeparation.maxUserId
       >> cd->installer.applicationUserIdSeparation.minUserId
       >> cd->installer.applicationUserIdSeparation.commonGroupId
       >> cd->dbus.policies
       >> cd->dbus.registrations
       >> cd->quicklaunch.idleLoad
       >> cd->quicklaunch.runtimesPerContainer
       >> cd->ui.style
       >> cd->ui.mainQml
       >> cd->ui.resources
       >> cd->ui.fullscreen
       >> cd->ui.windowIcon
       >> cd->ui.importPaths
       >> cd->ui.pluginPaths
       >> cd->ui.iconThemeName
       >> cd->ui.loadDummyData
       >> cd->ui.enableTouchEmulation
       >> cd->ui.iconThemeSearchPaths
       >> cd->ui.opengl
       >> cd->applications.builtinAppsManifestDir
       >> cd->applications.installationDir
       >> cd->applications.documentDir
       >> cd->installationLocations
       >> cd->crashAction
       >> cd->systemProperties
       >> cd->flags.noSecurity
       >> cd->flags.noUiWatchdog
       >> cd->flags.developmentMode
       >> cd->flags.forceMultiProcess
       >> cd->flags.forceSingleProcess
       >> cd->wayland.socketName
       >> cd->wayland.extraSockets
       >> cd->flags.allowUnsignedPackages;

    return cd;
}

void ConfigurationData::saveToCache(QDataStream &ds) const
{
    // NOTE: increment DataStreamVersion above, if you make any changes here

    // IMPORTANT: when doing changes to ConfigurationData, remember to adjust all of
    //            loadFromCache(), saveToCache() and mergeFrom() at the same time!

    ds << runtimes.configurations
       << containers.configurations
       << containers.selection
       << intents.disable
       << intents.timeouts.disambiguation
       << intents.timeouts.startApplication
       << intents.timeouts.replyFromApplication
       << intents.timeouts.replyFromSystem
       << plugins.startup
       << plugins.container
       << logging.dlt.id
       << logging.dlt.description
       << logging.rules
       << logging.messagePattern
       << logging.useAMConsoleLogger
       << installer.disable
       << installer.caCertificates
       << installer.applicationUserIdSeparation.maxUserId
       << installer.applicationUserIdSeparation.minUserId
       << installer.applicationUserIdSeparation.commonGroupId
       << dbus.policies
       << dbus.registrations
       << quicklaunch.idleLoad
       << quicklaunch.runtimesPerContainer
       << ui.style
       << ui.mainQml
       << ui.resources
       << ui.fullscreen
       << ui.windowIcon
       << ui.importPaths
       << ui.pluginPaths
       << ui.iconThemeName
       << ui.loadDummyData
       << ui.enableTouchEmulation
       << ui.iconThemeSearchPaths
       << ui.opengl
       << applications.builtinAppsManifestDir
       << applications.installationDir
       << applications.documentDir
       << installationLocations
       << crashAction
       << systemProperties
       << flags.noSecurity
       << flags.noUiWatchdog
       << flags.developmentMode
       << flags.forceMultiProcess
       << flags.forceSingleProcess
       << wayland.socketName
       << wayland.extraSockets
       << flags.allowUnsignedPackages;
}

template <typename T> void mergeField(T &into, const T &from, const T &def)
{
    if (from != def)
        into = from;
}

void mergeField(QVariantMap &into, const QVariantMap &from, const QVariantMap & /*def*/)
{
    recursiveMergeVariantMap(into, from);
}

void mergeField(QStringList &into, const QStringList &from, const QStringList & /*def*/)
{
    into.append(from);
}

template <typename T> void mergeField(QList<T> &into, const QList<T> &from, const QList<T> & /*def*/)
{
    into.append(from);
}

void mergeField(QList<QPair<QString, QString>> &into, const QList<QPair<QString, QString>> &from,
                const QList<QPair<QString, QString>> & /*def*/)
{
    for (auto &p : from) {
        auto it = std::find_if(into.begin(), into.end(), [p](const auto &fp) { return fp.first == p.first; });
        if (it != into.end())
            it->second = p.second;
        else
            into.append(p);
    }
}

// using templates only would be better, but we cannot get nice pointers-to-member-data for
// all elements in our sub-structs in a generic way without a lot of boilerplate code

#define MERGE_FIELD(x) mergeField(this->x, from->x, def.x)

void ConfigurationData::mergeFrom(const ConfigurationData *from)
{
    // IMPORTANT: when doing changes to ConfigurationData, remember to adjust all of
    //            loadFromCache(), saveToCache() and mergeFrom() at the same time!

    static const ConfigurationData def;

    MERGE_FIELD(runtimes.configurations);
    MERGE_FIELD(containers.configurations);
    MERGE_FIELD(containers.selection);

    MERGE_FIELD(intents.disable);
    MERGE_FIELD(intents.timeouts.disambiguation);
    MERGE_FIELD(intents.timeouts.startApplication);
    MERGE_FIELD(intents.timeouts.replyFromApplication);
    MERGE_FIELD(intents.timeouts.replyFromSystem);
    MERGE_FIELD(plugins.startup);
    MERGE_FIELD(plugins.container);
    MERGE_FIELD(logging.dlt.id);
    MERGE_FIELD(logging.dlt.description);
    MERGE_FIELD(logging.rules);
    MERGE_FIELD(logging.messagePattern);
    MERGE_FIELD(logging.useAMConsoleLogger);
    MERGE_FIELD(installer.disable);
    MERGE_FIELD(installer.caCertificates);
    MERGE_FIELD(installer.applicationUserIdSeparation.maxUserId);
    MERGE_FIELD(installer.applicationUserIdSeparation.minUserId);
    MERGE_FIELD(installer.applicationUserIdSeparation.commonGroupId);
    MERGE_FIELD(dbus.policies);
    MERGE_FIELD(dbus.registrations);
    MERGE_FIELD(quicklaunch.idleLoad);
    MERGE_FIELD(quicklaunch.runtimesPerContainer);
    MERGE_FIELD(ui.style);
    MERGE_FIELD(ui.mainQml);
    MERGE_FIELD(ui.resources);
    MERGE_FIELD(ui.fullscreen);
    MERGE_FIELD(ui.windowIcon);
    MERGE_FIELD(ui.importPaths);
    MERGE_FIELD(ui.pluginPaths);
    MERGE_FIELD(ui.iconThemeName);
    MERGE_FIELD(ui.loadDummyData);
    MERGE_FIELD(ui.enableTouchEmulation);
    MERGE_FIELD(ui.iconThemeSearchPaths);
    MERGE_FIELD(ui.opengl);
    MERGE_FIELD(applications.builtinAppsManifestDir);
    MERGE_FIELD(applications.installationDir);
    MERGE_FIELD(applications.documentDir);
    MERGE_FIELD(installationLocations);
    MERGE_FIELD(crashAction);
    MERGE_FIELD(systemProperties);
    MERGE_FIELD(flags.noSecurity);
    MERGE_FIELD(flags.noUiWatchdog);
    MERGE_FIELD(flags.developmentMode);
    MERGE_FIELD(flags.forceMultiProcess);
    MERGE_FIELD(flags.forceSingleProcess);
    MERGE_FIELD(wayland.socketName);
    MERGE_FIELD(wayland.extraSockets);
    MERGE_FIELD(flags.allowUnsignedPackages);
}

QByteArray ConfigurationData::substituteVars(const QByteArray &sourceContent, const QString &fileName)
{
    QByteArray string = sourceContent;
    int posBeg = -1;
    int posEnd = -1;
    while (true) {
        if ((posBeg = string.indexOf("${", posEnd + 1)) < 0)
            break;
        if ((posEnd = string.indexOf('}', posBeg + 2)) < 0)
            break;

        const QByteArray varName = string.mid(posBeg + 2, posEnd - posBeg - 2);

        QByteArray varValue;
        if (varName == "CONFIG_PWD") {
            static QByteArray path;
            if (path.isEmpty() && !fileName.isEmpty())
                path = QFileInfo(fileName).path().toUtf8();
            varValue = path;
        } else if (varName.startsWith("env:")) {
            varValue = qgetenv(varName.constData() + 4);
        } else if (varName.startsWith("stdpath:")) {
            bool exists;
            int loc = QMetaEnum::fromType<QStandardPaths::StandardLocation>().keyToValue(varName.constData() + 8, &exists);
            if (exists)
                varValue = QStandardPaths::writableLocation(static_cast<QStandardPaths::StandardLocation>(loc)).toUtf8();
        }

        if (varValue.isNull()) {
            qCWarning(LogDeployment).nospace() << "Could not replace variable ${" << varName << "} while parsing "
                                               << fileName;
            continue;
        }
        string.replace(posBeg, varName.length() + 3, varValue);
        // varName and varValue most likely have a different length, so we have to adjust
        posEnd = posEnd - 3 - varName.length() + varValue.length();
    }
    return string;
}

ConfigurationData *ConfigurationData::loadFromSource(QIODevice *source, const QString &fileName)
{
    try {
        YamlParser p(source->readAll(), fileName);
        auto header = p.parseHeader();
        if (!(header.first == qL1S("am-configuration") && header.second == 1))
            throw Exception("Unsupported format type and/or version");
        p.nextDocument();

        QString pwd;
        if (!fileName.isEmpty())
            pwd = QFileInfo(fileName).absoluteDir().path();

        QScopedPointer<ConfigurationData> cd(new ConfigurationData);

        YamlParser::Fields fields = {
            { "runtimes", false, YamlParser::Map, [&cd](YamlParser *p) {
                  cd->runtimes.configurations = p->parseMap(); } },
            { "containers", false, YamlParser::Map, [&cd](YamlParser *p) {
                  cd->containers.configurations = p->parseMap();

                  QVariant containerSelection = cd->containers.configurations.take(qSL("selection"));


                  QList<QPair<QString, QString>> config;

                  // this is easy to get wrong in the config file, so we do not just ignore a map here
                  // (this will in turn trigger the warning below)
                  if (containerSelection.type() == QVariant::Map)
                      containerSelection = QVariantList { containerSelection };

                  if (containerSelection.type() == QVariant::String) {
                      config.append(qMakePair(qSL("*"), containerSelection.toString()));
                  } else if (containerSelection.type() == QVariant::List) {
                      QVariantList list = containerSelection.toList();
                      for (const QVariant &v : list) {
                          if (v.type() == QVariant::Map) {
                              QVariantMap map = v.toMap();

                              if (map.size() != 1) {
                                  qCWarning(LogDeployment) << "The container selection configuration needs to be a "
                                                              "list of single mappings, in order to preserve the "
                                                              "evaluation order: found a mapping with"
                                                           << map.size() << "entries.";
                              }

                              for (auto it = map.cbegin(); it != map.cend(); ++it)
                                  config.append(qMakePair(it.key(), it.value().toString()));
                          }
                      }
                  }
                  cd->containers.selection = config;
              } },
            { "installationLocations", false, YamlParser::List, [&cd](YamlParser *p) {
                  cd->installationLocations = p->parseList(); } },
            { "plugins", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "startup", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->plugins.startup = p->parseStringOrStringList(); } },
                      { "container", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->plugins.container = p->parseStringOrStringList(); } },
                  }); } },
            { "logging", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "rules", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->logging.rules = p->parseStringOrStringList(); } },
                      { "messagePattern", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->logging.messagePattern = p->parseScalar().toString(); } },
                      { "useAMConsoleLogger", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->logging.useAMConsoleLogger = p->parseScalar(); } },
                      { "dlt", false, YamlParser::Map, [&cd](YamlParser *p) {
                            p->parseFields( {
                                { "id", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->logging.dlt.id = p->parseScalar().toString(); } },
                                { "description", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->logging.dlt.description = p->parseScalar().toString(); } }
                            }); } }
                  }); } },
            { "installer", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "disable", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->installer.disable = p->parseScalar().toBool(); } },
                      { "caCertificates", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->installer.caCertificates = p->parseStringOrStringList(); } },
                      { "applicationUserIdSeparation", false, YamlParser::Map, [&cd](YamlParser *p) {
                            p->parseFields({
                                { "minUserId", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->installer.applicationUserIdSeparation.minUserId = p->parseScalar().toInt(); } },
                                { "maxUserId", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->installer.applicationUserIdSeparation.maxUserId = p->parseScalar().toInt(); } },
                                { "commonGroupId", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->installer.applicationUserIdSeparation.commonGroupId = p->parseScalar().toInt(); } }
                            }); } }
                  }); } },
            { "quicklaunch", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "idleLoad", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->quicklaunch.idleLoad = p->parseScalar().toDouble(); } },
                      { "runtimesPerContainer", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->quicklaunch.runtimesPerContainer = p->parseScalar().toInt(); } },
                  }); } },
            { "ui", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "enableTouchEmulation", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->ui.enableTouchEmulation = p->parseScalar().toBool(); } },
                      { "iconThemeSearchPaths", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->ui.iconThemeSearchPaths = p->parseStringOrStringList(); } },
                      { "iconThemeName", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->ui.iconThemeName = p->parseScalar().toString(); } },
                      { "style", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->ui.style = p->parseScalar().toString(); } },
                      { "loadDummyData", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->ui.loadDummyData = p->parseScalar().toBool(); } },
                      { "importPaths", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->ui.importPaths = p->parseStringOrStringList(); } },
                      { "pluginPaths", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->ui.pluginPaths = p->parseStringOrStringList(); } },
                      { "windowIcon", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->ui.windowIcon = p->parseScalar().toString(); } },
                      { "fullscreen", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->ui.fullscreen = p->parseScalar().toBool(); } },
                      { "mainQml", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->ui.mainQml = p->parseScalar().toString(); } },
                      { "resources", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->ui.resources = p->parseStringOrStringList(); } },
                      { "opengl", false, YamlParser::Map, [&cd](YamlParser *p) {
                            p->parseFields({
                                { "desktopProfile", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->ui.opengl.insert(qSL("desktopProfile"), p->parseScalar().toString()); } },
                                { "esMajorVersion", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->ui.opengl.insert(qSL("esMajorVersion"), p->parseScalar().toInt()); } },
                                { "esMinorVersion", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->ui.opengl.insert(qSL("esMinorVersion"), p->parseScalar().toInt()); } }
                            });
                        } },
                  }); } },
            { "applications", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "builtinAppsManifestDir", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->applications.builtinAppsManifestDir = p->parseStringOrStringList(); } },
                      { "installationDir", false, YamlParser::Scalar | YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->applications.installationDir = p->parseScalar().toString(); } },
                      { "documentDir", false, YamlParser::Scalar | YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->applications.documentDir = p->parseScalar().toString(); } },
                      { "installedAppsManifestDir", false, YamlParser::Scalar, [](YamlParser *p) {
                            qCDebug(LogDeployment) << "ignoring 'installedAppsManifestDir'";
                            (void) p->parseScalar(); } },
                      { "database", false, YamlParser::Scalar | YamlParser::Scalar, [](YamlParser *p) {
                            qCDebug(LogDeployment) << "ignoring 'database'";
                            (void) p->parseScalar(); } },
                      { "appImageMountDir", false, YamlParser::Scalar | YamlParser::Scalar, [](YamlParser *p) {
                            qCDebug(LogDeployment) << "ignoring 'appImageMountDir'";
                            (void) p->parseScalar(); } }
                  }); } },
            { "flags", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "forceSingleProcess", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->flags.forceSingleProcess = p->parseScalar().toBool(); } },
                      { "forceMultiProcess", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->flags.forceMultiProcess = p->parseScalar().toBool(); } },
                      { "noSecurity", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->flags.noSecurity = p->parseScalar().toBool(); } },
                      { "developmentMode", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->flags.developmentMode = p->parseScalar().toBool(); } },
                      { "noUiWatchdog", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->flags.noUiWatchdog = p->parseScalar().toBool(); } },
                      { "allowUnsignedPackages", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->flags.allowUnsignedPackages = p->parseScalar().toBool(); } },
                  }); } },
            { "wayland", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "socketName", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->wayland.socketName = p->parseScalar().toString(); } },
                      { "extraSockets", false, YamlParser::List, [&cd](YamlParser *p) {
                            p->parseList([&cd](YamlParser *p) {
                                QVariantMap wes;
                                p->parseFields({
                                    { "path", true, YamlParser::Scalar, [&wes](YamlParser *p) {
                                          wes.insert(qSL("path"), p->parseScalar().toString()); } },
                                    { "permissions", false, YamlParser::Scalar, [&wes](YamlParser *p) {
                                          wes.insert(qSL("permissions"), p->parseScalar().toInt()); } },
                                    { "userId", false, YamlParser::Scalar, [&wes](YamlParser *p) {
                                          wes.insert(qSL("userId"), p->parseScalar().toInt()); } },
                                    { "groupId", false, YamlParser::Scalar, [&wes](YamlParser *p) {
                                          wes.insert(qSL("groupId"), p->parseScalar().toInt()); } }
                                });
                                cd->wayland.extraSockets.append(wes);
                            }); } }
                  }); } },
            { "systemProperties", false, YamlParser::Map, [&cd](YamlParser *p) {
                  cd->systemProperties = p->parseMap(); } },
            { "crashAction", false, YamlParser::Map, [&cd](YamlParser *p) {
                  cd->crashAction = p->parseMap(); } },
            { "intents", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "disable", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->intents.disable = p->parseScalar().toBool(); } },
                      { "timeouts", false, YamlParser::Map, [&cd](YamlParser *p) {
                            p->parseFields({
                                { "disambiguation", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->intents.timeouts.disambiguation = p->parseScalar().toInt(); } },
                                { "startApplication", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->intents.timeouts.startApplication = p->parseScalar().toInt(); } },
                                { "replyFromApplication", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->intents.timeouts.replyFromApplication = p->parseScalar().toInt(); } },
                                { "replyFromSystem", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->intents.timeouts.replyFromSystem = p->parseScalar().toInt(); } },
                            }); } }
                  }); } },
            { "dbus", false, YamlParser::Map, [&cd](YamlParser *p) {
                  const QVariantMap dbus = p->parseMap();
                  for (auto it = dbus.cbegin(); it != dbus.cend(); ++it) {
                      const QString &ifaceName = it.key();
                      const QVariantMap &ifaceData = it.value().toMap();

                      auto rit = ifaceData.constFind(qSL("register"));
                      if (rit != ifaceData.cend())
                          cd->dbus.registrations.insert(ifaceName, rit->toString());

                      auto pit = ifaceData.constFind(qSL("policy"));
                      if (pit != ifaceData.cend())
                          cd->dbus.policies.insert(ifaceName, pit->toMap());
                  }
              } }
        };

        p.parseFields(fields);
        return cd.take();
    } catch (const Exception &e) {
        throw Exception(e.errorCode(), "Failed to parse config file %1: %2")
                .arg(!fileName.isEmpty() ? QDir().relativeFilePath(fileName) : qSL("<stream>"), e.errorString());
    }
}


// getters

static QString replaceEnvVars(QString string)
{
    // note: we cannot replace ${CONFIG_PWD} here, since we have lost that information during
    //       the config file merge!

    static QHash<QString, QString> replacement;
    if (replacement.isEmpty()) {
        QMetaEnum locations = QMetaEnum::fromType<QStandardPaths::StandardLocation>();
        for (int i = 0; i < locations.keyCount(); ++i) {
            replacement.insert(qSL("stdpath:") + qL1S(locations.key(i)),
                               QStandardPaths::writableLocation(static_cast<QStandardPaths::StandardLocation>(locations.value(i))));
        }
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QStringList envNames = env.keys();
        for (const auto &envName : qAsConst(envNames))
            replacement.insert(qSL("env:") + envName, env.value(envName));
    }

    // this will return immediately, if no vars are referenced
    int posBeg = -1;
    int posEnd = -1;
    while (true) {
        if ((posBeg = string.indexOf(qL1S("${"), posEnd + 1)) < 0)
            break;
        if ((posEnd = string.indexOf(qL1C('}'), posBeg + 2)) < 0)
            break;

        const QString varName = string.mid(posBeg + 2, posEnd - posBeg - 2);
        const QString varValue = replacement.value(varName);
        string.replace(posBeg, varName.length() + 3, varValue);

        // varName and varValue most likely have a different length, so we have to adjust
        posEnd = posEnd - 3 - varName.length() + varValue.length();
    }
    return string;
}


QString Configuration::mainQmlFile() const
{
    if (!m_clp.positionalArguments().isEmpty())
        return m_clp.positionalArguments().at(0);
    else
        return replaceEnvVars(m_data->ui.mainQml);
}

bool Configuration::noCache() const
{
    return value<bool>("no-cache");
}

bool Configuration::clearCache() const
{
    return value<bool>("clear-cache");
}


QStringList Configuration::builtinAppsManifestDirs() const
{
    return value<QStringList>("builtin-apps-manifest-dir", m_data->applications.builtinAppsManifestDir);
}

QString Configuration::installationDir() const
{
    if (m_installationDir.isEmpty())
        m_installationDir = value<QString>("installation-dir", m_data->applications.installationDir);
    return m_installationDir;
}

QString Configuration::documentDir() const
{
    if (m_documentDir.isEmpty())
        m_documentDir = value<QString>("document-dir", m_data->applications.documentDir);
    return m_documentDir;
}

bool Configuration::disableInstaller() const
{
    return value<bool>("disable-installer", m_data->installer.disable);
}

bool Configuration::disableIntents() const
{
    return value<bool>("disable-intents", m_data->intents.disable);
}

int Configuration::intentTimeoutForDisambiguation() const
{
    return m_data->intents.timeouts.disambiguation;
}

int Configuration::intentTimeoutForStartApplication() const
{
    return m_data->intents.timeouts.startApplication;
}

int Configuration::intentTimeoutForReplyFromApplication() const
{
    return m_data->intents.timeouts.replyFromApplication;
}

int Configuration::intentTimeoutForReplyFromSystem() const
{
    return m_data->intents.timeouts.replyFromSystem;
}

bool Configuration::fullscreen() const
{
    return value<bool>("fullscreen", m_data->ui.fullscreen);
}

bool Configuration::noFullscreen() const
{
    return value<bool>("no-fullscreen");
}

QString Configuration::windowIcon() const
{
    return m_data->ui.windowIcon;
}

QStringList Configuration::importPaths() const
{
    QStringList importPaths = value<QStringList>("I", m_data->ui.importPaths);

    for (int i = 0; i < importPaths.size(); ++i)
        importPaths[i] = toAbsoluteFilePath(importPaths.at(i));

    return importPaths;
}

QStringList Configuration::pluginPaths() const
{
    return m_data->ui.pluginPaths;
}

bool Configuration::verbose() const
{
    return value<bool>("verbose") || m_forceVerbose;
}

void Configuration::setForceVerbose(bool forceVerbose)
{
    m_forceVerbose = forceVerbose;
}

bool Configuration::slowAnimations() const
{
    return value<bool>("slow-animations");
}

bool Configuration::loadDummyData() const
{
    return value<bool>("load-dummydata", m_data->ui.loadDummyData);
}

bool Configuration::noSecurity() const
{
    return value<bool>("no-security", m_data->flags.noSecurity);
}

bool Configuration::developmentMode() const
{
    return value<bool>("development-mode", m_data->flags.developmentMode);
}

bool Configuration::allowUnsignedPackages() const
{
    return m_data->flags.allowUnsignedPackages;
}

bool Configuration::noUiWatchdog() const
{
    return value<bool>("no-ui-watchdog", m_data->flags.noUiWatchdog);
}

bool Configuration::noDltLogging() const
{
    return value<bool>("no-dlt-logging");
}

bool Configuration::forceSingleProcess() const
{
    return value<bool>("force-single-process", m_data->flags.forceSingleProcess);
}

bool Configuration::forceMultiProcess() const
{
    return value<bool>("force-multi-process", m_data->flags.forceMultiProcess);
}

bool Configuration::qmlDebugging() const
{
    return value<bool>("qml-debug");
}

QString Configuration::singleApp() const
{
    //TODO: single-package
    return value<QString>("single-app");
}

QStringList Configuration::loggingRules() const
{
    return value<QStringList>("logging-rule", m_data->logging.rules);
}

QString Configuration::messagePattern() const
{
    return m_data->logging.messagePattern;
}

QVariant Configuration::useAMConsoleLogger() const
{
    // true = use the am logger
    // false = don't use the am logger
    // invalid = don't use the am logger when QT_MESSAGE_PATTERN is set
    const QVariant &val = m_data->logging.useAMConsoleLogger;
    if (val.type() == QVariant::Bool)
        return val;
    else
        return QVariant();
}

QString Configuration::style() const
{
    return m_data->ui.style;
}

QString Configuration::iconThemeName() const
{
    return m_data->ui.iconThemeName;
}

QStringList Configuration::iconThemeSearchPaths() const
{
    return m_data->ui.iconThemeSearchPaths;
}

bool Configuration::enableTouchEmulation() const
{
    return value("enable-touch-emulation", m_data->ui.enableTouchEmulation);
}

QString Configuration::dltId() const
{
    return value<QString>(nullptr, m_data->logging.dlt.id);
}

QString Configuration::dltDescription() const
{
    return value<QString>(nullptr, m_data->logging.dlt.description);
}

QStringList Configuration::resources() const
{
    return m_data->ui.resources;
}

QVariantMap Configuration::openGLConfiguration() const
{
    return m_data->ui.opengl;
}

QVariantList Configuration::installationLocations() const
{
    return m_data->installationLocations;
}

QList<QPair<QString, QString>> Configuration::containerSelectionConfiguration() const
{
    return m_data->containers.selection;
}

QVariantMap Configuration::containerConfigurations() const
{
    return m_data->containers.configurations;
}

QVariantMap Configuration::runtimeConfigurations() const
{
    return m_data->runtimes.configurations;
}

QVariantMap Configuration::dbusPolicy(const char *interfaceName) const
{
    return m_data->dbus.policies.value(qL1S(interfaceName)).toMap();
}

QString Configuration::dbusRegistration(const char *interfaceName) const
{
    auto hasConfig = m_data->dbus.registrations.constFind(qL1S(interfaceName));

    if (hasConfig != m_data->dbus.registrations.cend())
        return hasConfig->toString();
    else
        return m_clp.value(qSL("dbus"));
}

QVariantMap Configuration::rawSystemProperties() const
{
    return m_data->systemProperties;
}

bool Configuration::applicationUserIdSeparation(uint *minUserId, uint *maxUserId, uint *commonGroupId) const
{
    const auto &sep = m_data->installer.applicationUserIdSeparation;
    if (sep.minUserId >= 0 && sep.maxUserId >= 0 && sep.commonGroupId >= 0) {
        if (minUserId)
            *minUserId = sep.minUserId;
        if (maxUserId)
            *maxUserId = sep.maxUserId;
        if (commonGroupId)
            *commonGroupId = sep.commonGroupId;
        return true;
    }
    return false;
}

qreal Configuration::quickLaunchIdleLoad() const
{
    return m_data->quicklaunch.idleLoad;
}

int Configuration::quickLaunchRuntimesPerContainer() const
{
    // if you need more than 10 quicklaunchers per runtime, you're probably doing something wrong
    // or you have a typo in your YAML, which could potentially freeze your target (container
    // construction can be expensive)
    return qBound(0, m_data->quicklaunch.runtimesPerContainer, 10);
}

QString Configuration::waylandSocketName() const
{
#if !defined(AM_HEADLESS)
    QString socketName = m_clp.value(qSL("wayland-socket-name")); // get the default value
    if (!socketName.isEmpty())
        return socketName;

    const char *envName = "WAYLAND_DISPLAY";
    if (qEnvironmentVariableIsSet(envName)) {
        socketName = qEnvironmentVariable(envName);
        if (!QGuiApplication::platformName().startsWith(qSL("wayland")) || (socketName != qSL("wayland-0")))
            return socketName;
    }

    if (!m_data->wayland.socketName.isEmpty())
        return m_data->wayland.socketName;

#  if defined(Q_OS_LINUX)
    // modelled after wl_socket_lock() in wayland_server.c
    const QString xdgDir = qEnvironmentVariable("XDG_RUNTIME_DIR") + qSL("/");
    const QString pattern = qSL("qtam-wayland-%1");
    const QString lockSuffix = qSL(".lock");

    for (int i = 0; i < 32; ++i) {
        socketName = pattern.arg(i);
        QFile lock(xdgDir + socketName + lockSuffix);
        if (lock.open(QIODevice::ReadWrite)) {
            if (::flock(lock.handle(), LOCK_EX | LOCK_NB) == 0) {
                QFile socket(xdgDir + socketName);
                if (!socket.exists() || socket.remove())
                    return socketName;
            }
        }
    }
#  endif
#endif
    return QString();

}

QVariantList Configuration::waylandExtraSockets() const
{
    return m_data->wayland.extraSockets;
}

QVariantMap Configuration::managerCrashAction() const
{
    return m_data->crashAction;
}

QStringList Configuration::caCertificates() const
{
    return m_data->installer.caCertificates;
}

QStringList Configuration::pluginFilePaths(const char *type) const
{
    if (qstrcmp(type, "startup") == 0)
        return m_data->plugins.startup;
    else if (qstrcmp(type, "container") == 0)
        return m_data->plugins.container;
    else
        return QStringList();
}

QStringList Configuration::testRunnerArguments() const
{
    QStringList targs = m_clp.positionalArguments();
    if (!targs.isEmpty() && targs.constFirst().endsWith(qL1S(".qml")))
        targs.removeFirst();
    targs.prepend(QCoreApplication::arguments().constFirst());
    return targs;
}

QT_END_NAMESPACE_AM
