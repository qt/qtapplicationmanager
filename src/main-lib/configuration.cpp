// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QMetaEnum>
#include <QProcessEnvironment>

#include <QDataStream>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QGuiApplication>

#include <cstdlib>
#include <cstdio>
#include <functional>
#include <memory>

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

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM


template<> bool Configuration::value(const char *clname, const bool &cfvalue) const
{
    return (clname && m_clp.isSet(QString::fromLatin1(clname))) || cfvalue;
}

template<> QString Configuration::value(const char *clname, const QString &cfvalue) const
{
    return (clname && m_clp.isSet(QString::fromLatin1(clname))) ? m_clp.value(QString::fromLatin1(clname)) : cfvalue;
}

template<> QStringList Configuration::value(const char *clname, const QStringList &cfvalue) const
{
    QStringList result;
    if (clname)
        result = m_clp.values(QString::fromLatin1(clname));
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
    : Configuration(QStringList(), u":/build-config.yaml"_s,
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
        "  AM_STARTUP_TIMER       If set to '1', a startup performance analysis is\n"
        "                         printed to the console. Anything other than '1' is\n"
        "                         interpreted as the name of a file to use, instead\n"
        "                         of the console.\n"
        "\n"
        "  AM_FORCE_COLOR_OUTPUT  Can be set to 'on' to force color output to the\n"
        "                         console and to 'off' to disable it. Any other value\n"
        "                         enables the default, auto-detection behavior.\n"
        "\n"
        "  AM_NO_CUSTOM_LOGGING   If set to '1', debug output is not redirected to DLT,\n"
        "                         colorized or nicely formatted.\n"
        "\n"
        "  AM_NO_DLT_LOGGING      If set to '1', debug output is not redirected to DLT.\n"
        "\n"
        "  AM_NO_CRASH_HANDLER    If set to '1', no crash handler is installed. Use\n"
        "                         this, if the application manager's crash handler is\n"
        "                         interfering with other debugging tools you are using.\n";

    m_clp.setApplicationDescription(u"\n"_s + QCoreApplication::applicationName() + u"\n\n"_s
                                    + (additionalDescription ? (QString::fromLatin1(additionalDescription) + u"\n\n"_s) : QString())
                                    + QString::fromLatin1(description));

    m_clp.addOption({ { u"h"_s, u"help"_s
#if defined(Q_OS_WINDOWS)
                        , u"?"_s
#endif
                      },                         u"Displays this help."_s });
    m_clp.addOption({ u"version"_s,              u"Displays version information."_s });
    QCommandLineOption cf { { u"c"_s, u"config-file"_s },
                                                 u"Load configuration from file (can be given multiple times)."_s, u"files"_s };
    cf.setDefaultValues(m_defaultConfigFilePaths);
    m_clp.addOption(cf);
    m_clp.addOption({ { u"o"_s, u"option"_s },   u"Override a specific config option."_s, u"yaml-snippet"_s });
    m_clp.addOption({ { u"no-cache"_s, u"no-config-cache"_s },
                                                 u"Disable the use of the config and appdb file cache."_s });
    m_clp.addOption({ { u"clear-cache"_s, u"clear-config-cache"_s },
                                                 u"Ignore an existing config and appdb file cache."_s });
    m_clp.addOption({ { u"r"_s, u"recreate-database"_s },
                                                 u"Backwards compatibility: synonyms for --clear-cache."_s });
    if (!buildConfigFilePath.isEmpty())
        m_clp.addOption({ u"build-config"_s,     u"Dumps the build configuration and exits."_s });

    m_clp.addPositionalArgument(u"qml-file"_s,   u"The main QML file."_s);
    m_clp.addOption({ u"log-instant"_s,          u"Log instantly at start-up, neglect logging configuration."_s });
    m_clp.addOption({ u"database"_s,             u"Deprecated (ignored)."_s, u"file"_s });
    m_clp.addOption({ u"builtin-apps-manifest-dir"_s, u"Base directory for built-in application manifests."_s, u"dir"_s });
    m_clp.addOption({ u"installation-dir"_s,     u"Base directory for package installations."_s, u"dir"_s });
    m_clp.addOption({ u"document-dir"_s,         u"Base directory for per-package document directories."_s, u"dir"_s });
    m_clp.addOption({ u"installed-apps-manifest-dir"_s, u"Deprecated (ignored)."_s, u"dir"_s });
    m_clp.addOption({ u"app-image-mount-dir"_s,  u"Deprecated (ignored)."_s, u"dir"_s });
    m_clp.addOption({ u"disable-installer"_s,    u"Disable the application installer sub-system."_s });
    m_clp.addOption({ u"disable-intents"_s,      u"Disable the intents sub-system."_s });
    m_clp.addOption({ u"dbus"_s,                 u"Register on the specified D-Bus."_s, u"<bus>|system|session|none|auto"_s, u"auto"_s });
    m_clp.addOption({ u"fullscreen"_s,           u"Display in full-screen."_s });
    m_clp.addOption({ u"no-fullscreen"_s,        u"Do not display in full-screen."_s });
    m_clp.addOption({ u"I"_s,                    u"Additional QML import path."_s, u"dir"_s });
    m_clp.addOption({ { u"v"_s, u"verbose"_s }, u"Verbose output."_s });
    m_clp.addOption({ u"slow-animations"_s,      u"Run all animations in slow motion."_s });
    m_clp.addOption({ u"load-dummydata"_s,       u"Deprecated. Loads QML dummy-data."_s });
    m_clp.addOption({ u"no-security"_s,          u"Disables all security related checks (dev only!)"_s });
    m_clp.addOption({ u"development-mode"_s,     u"Enable development mode, allowing installation of dev-signed packages."_s });
    m_clp.addOption({ u"no-ui-watchdog"_s,       u"Disables detecting hung UI applications (e.g. via Wayland's ping/pong)."_s });
    m_clp.addOption({ u"no-dlt-logging"_s,       u"Disables logging using automotive DLT."_s });
    m_clp.addOption({ u"force-single-process"_s, u"Forces single-process mode even on a wayland enabled build."_s });
    m_clp.addOption({ u"force-multi-process"_s,  u"Forces multi-process mode. Will exit immediately if this is not possible."_s });
    m_clp.addOption({ u"wayland-socket-name"_s,  u"Use this file name to create the wayland socket."_s, u"socket"_s });
    m_clp.addOption({ u"single-app"_s,           u"Runs a single application only (ignores the database)"_s, u"info.yaml file"_s }); // rename single-package
    m_clp.addOption({ u"logging-rule"_s,         u"Adds a standard Qt logging rule."_s, u"rule"_s });
    m_clp.addOption({ u"qml-debug"_s,            u"Enables QML debugging and profiling."_s });
    m_clp.addOption({ u"enable-touch-emulation"_s, u"Deprecated (ignored)."_s });
    m_clp.addOption({ u"instance-id"_s,          u"Use this id to distinguish between multiple instances."_s, u"id"_s });

    { // qmltestrunner specific, necessary for CI blacklisting
        QCommandLineOption qtrsf { u"qmltestrunner-source-file"_s, u"appman-qmltestrunner only: set the source file path of the test."_s, u"file"_s };
        qtrsf.setFlags(QCommandLineOption::HiddenFromHelp);
        m_clp.addOption(qtrsf);
    }
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

void Configuration::parseWithArguments(const QStringList &arguments)
{
    if (!m_clp.parse(arguments))
        throw Exception(m_clp.errorText());

    if (m_clp.isSet(u"version"_s))
        m_clp.showVersion();

    if (m_clp.isSet(u"help"_s))
        m_clp.showHelp();

    if (!m_buildConfigFilePath.isEmpty() && m_clp.isSet(u"build-config"_s)) {
        QFile f(m_buildConfigFilePath);
        if (f.open(QFile::ReadOnly)) {
            ::fprintf(stdout, "%s\n", f.readAll().constData());
            ::exit(0);
        } else {
            throw Exception("Could not find the embedded build config.");
        }
    }

    if (m_clp.isSet(u"instance-id"_s)) {
        auto id = m_clp.value(u"instance-id"_s);
        try {
            validateIdForFilesystemUsage(id);
        } catch (const Exception &e) {
            throw Exception("Invalid instance-id (%1): %2\n").arg(id, e.errorString());
        }
    }

#if defined(AM_TIME_CONFIG_PARSING)
    QElapsedTimer timer;
    timer.start();
#endif

    const QStringList rawConfigFilePaths = m_clp.values(u"config-file"_s);
    QStringList configFilePaths;
    configFilePaths.reserve(rawConfigFilePaths.size());
    for (const auto &path : rawConfigFilePaths) {
        if (QFileInfo(path).isDir()) {
            const auto entries = QDir(path).entryInfoList({ u"*.yaml"_s }, QDir::Files, QDir::Name);
            for (const auto &entry : entries)
                configFilePaths << entry.filePath();
        } else {
            configFilePaths << path;
        }
    }

    AbstractConfigCache::Options cacheOptions = AbstractConfigCache::MergedResult;
    if (noCache())
        cacheOptions |= AbstractConfigCache::NoCache;
    if (clearCache())
        cacheOptions |= AbstractConfigCache::ClearCache;

    if (configFilePaths.isEmpty()) {
        m_data.reset(new ConfigurationData());
    } else {
        ConfigCache<ConfigurationData> cache(configFilePaths, u"config"_s, { 'C','F','G','D' },
                                             ConfigurationData::dataStreamVersion(), cacheOptions);

        cache.parse();
        m_data.reset(cache.takeMergedResult());
        if (!m_data)
            m_data.reset(new ConfigurationData());
    }

    const QStringList options = m_clp.values(u"o"_s);
    for (const QString &option : options) {
        QByteArray yaml("formatVersion: 1\nformatType: am-configuration\n---\n");
        yaml.append(option.toUtf8());
        QBuffer buffer(&yaml);
        buffer.open(QIODevice::ReadOnly);
        try {
            ConfigurationData *cd = ConfigCacheAdaptor<ConfigurationData>::loadFromSource(&buffer, u"command line"_s);
            if (cd) {
                ConfigCacheAdaptor<ConfigurationData>::merge(m_data.get(), cd);
                delete cd;
            }
        } catch (const Exception &e) {
            throw Exception("Could not parse --option value: %1").arg(e.errorString());
        }
    }

    // early sanity checks
    if (m_onlyOnePositionalArgument && (m_clp.positionalArguments().size() > 1))
        throw Exception("Only one main qml file can be specified.");

    if (installationDir().isEmpty()) {
        const auto ilocs = m_data->installationLocations;
        if (!ilocs.isEmpty()) {
            qCWarning(LogDeployment) << "Support for \"installationLocations\" in the main config file "
                                        "has been removed:";
        }

        for (const auto &iloc : ilocs) {
            QVariantMap map = iloc.toMap();
            QString id = map.value(u"id"_s).toString();
            if (id == u"internal-0") {
                m_installationDir = map.value(u"installationPath"_s).toString();
                m_documentDir = map.value(u"documentPath"_s).toString();
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
}

quint32 ConfigurationData::dataStreamVersion()
{
    return 13;
}

ConfigurationData *ConfigurationData::loadFromCache(QDataStream &ds)
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    // IMPORTANT: when doing changes to ConfigurationData, remember to adjust all of
    //            loadFromCache(), saveToCache() and mergeFrom() at the same time!

    ConfigurationData *cd = new ConfigurationData;
    ds >> cd->runtimes.configurations
       >> cd->runtimes.additionalLaunchers
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
       >> cd->logging.dlt.longMessageBehavior
       >> cd->logging.rules
       >> cd->logging.messagePattern
       >> cd->logging.useAMConsoleLogger
       >> cd->installer.disable
       >> cd->installer.caCertificates
       >> cd->dbus.policies
       >> cd->dbus.registrations
       >> cd->quicklaunch.idleLoad
       >> cd->quicklaunch.runtimesPerContainer
       >> cd->quicklaunch.failedStartLimit
       >> cd->quicklaunch.failedStartLimitIntervalSec
       >> cd->ui.style
       >> cd->ui.mainQml
       >> cd->ui.resources
       >> cd->ui.fullscreen
       >> cd->ui.windowIcon
       >> cd->ui.importPaths
       >> cd->ui.pluginPaths
       >> cd->ui.iconThemeName
       >> cd->ui.loadDummyData
       >> cd->ui.iconThemeSearchPaths
       >> cd->ui.opengl
       >> cd->applications.builtinAppsManifestDir
       >> cd->applications.installationDir
       >> cd->applications.documentDir
       >> cd->applications.installationDirMountPoint
       >> cd->installationLocations
       >> cd->crashAction
       >> cd->systemProperties
       >> cd->flags.noSecurity
       >> cd->flags.noUiWatchdog
       >> cd->flags.developmentMode
       >> cd->flags.forceMultiProcess
       >> cd->flags.forceSingleProcess
       >> cd->flags.allowUnsignedPackages
       >> cd->flags.allowUnknownUiClients
       >> cd->wayland.socketName
       >> cd->wayland.extraSockets
       >> cd->instanceId;

    return cd;
}

void ConfigurationData::saveToCache(QDataStream &ds) const
{
    //NOTE: increment dataStreamVersion() above, if you make any changes here

    // IMPORTANT: when doing changes to ConfigurationData, remember to adjust all of
    //            loadFromCache(), saveToCache() and mergeFrom() at the same time!

    ds << runtimes.configurations
       << runtimes.additionalLaunchers
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
       << logging.dlt.longMessageBehavior
       << logging.rules
       << logging.messagePattern
       << logging.useAMConsoleLogger
       << installer.disable
       << installer.caCertificates
       << dbus.policies
       << dbus.registrations
       << quicklaunch.idleLoad
       << quicklaunch.runtimesPerContainer
       << quicklaunch.failedStartLimit
       << quicklaunch.failedStartLimitIntervalSec
       << ui.style
       << ui.mainQml
       << ui.resources
       << ui.fullscreen
       << ui.windowIcon
       << ui.importPaths
       << ui.pluginPaths
       << ui.iconThemeName
       << ui.loadDummyData
       << ui.iconThemeSearchPaths
       << ui.opengl
       << applications.builtinAppsManifestDir
       << applications.installationDir
       << applications.documentDir
       << applications.installationDirMountPoint
       << installationLocations
       << crashAction
       << systemProperties
       << flags.noSecurity
       << flags.noUiWatchdog
       << flags.developmentMode
       << flags.forceMultiProcess
       << flags.forceSingleProcess
       << flags.allowUnsignedPackages
       << flags.allowUnknownUiClients
       << wayland.socketName
       << wayland.extraSockets
       << instanceId;
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

template <typename T, typename U> void mergeField(QHash<T, U> &into, const QHash<T, U> &from, const QHash<T, U> & /*def*/)
{
    into.insert(from);
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
    MERGE_FIELD(runtimes.additionalLaunchers);
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
    MERGE_FIELD(logging.dlt.longMessageBehavior);
    MERGE_FIELD(logging.rules);
    MERGE_FIELD(logging.messagePattern);
    MERGE_FIELD(logging.useAMConsoleLogger);
    MERGE_FIELD(installer.disable);
    MERGE_FIELD(installer.caCertificates);
    MERGE_FIELD(dbus.policies);
    MERGE_FIELD(dbus.registrations);
    MERGE_FIELD(quicklaunch.idleLoad);
    MERGE_FIELD(quicklaunch.runtimesPerContainer);
    MERGE_FIELD(quicklaunch.failedStartLimit);
    MERGE_FIELD(quicklaunch.failedStartLimitIntervalSec);
    MERGE_FIELD(ui.style);
    MERGE_FIELD(ui.mainQml);
    MERGE_FIELD(ui.resources);
    MERGE_FIELD(ui.fullscreen);
    MERGE_FIELD(ui.windowIcon);
    MERGE_FIELD(ui.importPaths);
    MERGE_FIELD(ui.pluginPaths);
    MERGE_FIELD(ui.iconThemeName);
    MERGE_FIELD(ui.loadDummyData);
    MERGE_FIELD(ui.iconThemeSearchPaths);
    MERGE_FIELD(ui.opengl);
    MERGE_FIELD(applications.builtinAppsManifestDir);
    MERGE_FIELD(applications.installationDir);
    MERGE_FIELD(applications.documentDir);
    MERGE_FIELD(applications.installationDirMountPoint);
    MERGE_FIELD(installationLocations);
    MERGE_FIELD(crashAction);
    MERGE_FIELD(systemProperties);
    MERGE_FIELD(flags.noSecurity);
    MERGE_FIELD(flags.noUiWatchdog);
    MERGE_FIELD(flags.developmentMode);
    MERGE_FIELD(flags.forceMultiProcess);
    MERGE_FIELD(flags.forceSingleProcess);
    MERGE_FIELD(flags.allowUnsignedPackages);
    MERGE_FIELD(flags.allowUnknownUiClients);
    MERGE_FIELD(wayland.socketName);
    MERGE_FIELD(wayland.extraSockets);
    MERGE_FIELD(instanceId);
}

QByteArray ConfigurationData::substituteVars(const QByteArray &sourceContent, const QString &fileName)
{
    QByteArray string = sourceContent;
    QByteArray path;
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
        if (!(header.first == u"am-configuration" && header.second == 1))
            throw Exception("Unsupported format type and/or version");
        p.nextDocument();

        QString pwd;
        if (!fileName.isEmpty())
            pwd = QFileInfo(fileName).absoluteDir().path();

        auto cd = std::make_unique<ConfigurationData>();

        YamlParser::Fields fields = {
            { "instanceId", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                  auto id = p->parseString();
                  try {
                      validateIdForFilesystemUsage(id);
                      cd->instanceId = id;
                  } catch (const Exception &e) {
                      throw YamlParserException(p, "invalid instanceId: %1").arg(e.errorString());
                  }
              } },
            { "runtimes", false, YamlParser::Map, [&cd](YamlParser *p) {
                  cd->runtimes.configurations = p->parseMap();
                  QVariant additionalLaunchers = cd->runtimes.configurations.take(u"additionalLaunchers"_s);
                  cd->runtimes.additionalLaunchers = variantToStringList(additionalLaunchers);
              } },
            { "containers", false, YamlParser::Map, [&cd](YamlParser *p) {
                  cd->containers.configurations = p->parseMap();

                  QVariant containerSelection = cd->containers.configurations.take(u"selection"_s);


                  QList<QPair<QString, QString>> config;

                  // this is easy to get wrong in the config file, so we do not just ignore a map here
                  // (this will in turn trigger the warning below)
                  if (containerSelection.metaType() == QMetaType::fromType<QVariantMap>())
                      containerSelection = QVariantList { containerSelection };

                  if (containerSelection.metaType() == QMetaType::fromType<QString>()) {
                      config.append(qMakePair(u"*"_s, containerSelection.toString()));
                  } else if (containerSelection.metaType() == QMetaType::fromType<QVariantList>()) {
                      QVariantList list = containerSelection.toList();
                      for (const QVariant &v : list) {
                          if (v.metaType() == QMetaType::fromType<QVariantMap>()) {
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
                                      cd->logging.dlt.description = p->parseScalar().toString(); } },
                                { "longMessageBehavior", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      static const QStringList validValues {
                                          u"split"_s, u"truncate"_s, u"pass"_s
                                      };
                                      QString s = p->parseScalar().toString().trimmed();
                                      if (!s.isEmpty() && !validValues.contains(s)) {
                                          throw YamlParserException(p, "dlt.longMessageBehavior needs to be one of %1").arg(validValues);
                                      }
                                      cd->logging.dlt.longMessageBehavior = s;
                                  } }
                            }); } }
                  }); } },
            { "installer", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "disable", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->installer.disable = p->parseScalar().toBool(); } },
                      { "caCertificates", false, YamlParser::Scalar | YamlParser::List, [&cd](YamlParser *p) {
                            cd->installer.caCertificates = p->parseStringOrStringList(); } },
                  }); } },
            { "quicklaunch", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "idleLoad", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->quicklaunch.idleLoad = p->parseScalar().toDouble(); } },
                      { "runtimesPerContainer", false, YamlParser::Scalar | YamlParser::Map, [&cd](YamlParser *p) {
                            // this can be set to different things:
                            //  - just a number -> the same count for any container/runtime combo (the only option pre 6.7)
                            //  - a "<container-id>": mapping -> you can map to
                            //    - just a number -> the same count for any runtime in these containers
                            //    - a "<runtime-id>": mapping -> a specific count for this container/runtime combo
                            static const QString anyId = u"*"_s;

                            if (p->isScalar()) {
                                bool ok;
                                int rpc = p->parseScalar().toInt(&ok);
                                if (!ok || (rpc < 0) || (rpc > 10))
                                    throw YamlParserException(p, "quicklaunch.runtimesPerContainer count needs to be between 0 and 10");

                                cd->quicklaunch.runtimesPerContainer[{ anyId, anyId }] = rpc;
                            } else {
                                const QVariantMap containerIdMap = p->parseMap();
                                for (auto cit = containerIdMap.cbegin(); cit != containerIdMap.cend(); ++cit) {
                                    const QString &containerId = cit.key();
                                    const QVariant &value = cit.value();

                                    switch (value.metaType().id()) {
                                    case QMetaType::Int:
                                        cd->quicklaunch.runtimesPerContainer[{ containerId, anyId }] = value.toInt();
                                        break;
                                    case QMetaType::QVariantMap: {
                                        const QVariantMap runtimeIdMap = value.toMap();
                                        for (auto rit = runtimeIdMap.cbegin(); rit != runtimeIdMap.cend(); ++rit) {
                                            const QString &runtimeId = rit.key();
                                            bool ok;
                                            int rpc = rit.value().toInt(&ok);
                                            if (!ok || (rpc < 0) || (rpc > 10))
                                                throw YamlParserException(p, "quicklaunch.runtimesPerContainer count needs to be between 0 and 10");

                                            cd->quicklaunch.runtimesPerContainer[{ containerId, runtimeId }] = rpc;
                                        }
                                        break;
                                    }
                                    default:
                                        throw YamlParserException(p, "quicklaunch.runtimesPerContainer is invalid");
                                    }
                                }
                            } } },
                      { "failedStartLimit", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->quicklaunch.failedStartLimit = p->parseScalar().toInt(); } },
                      { "failedStartLimitIntervalSec", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->quicklaunch.failedStartLimitIntervalSec = p->parseScalar().toInt(); } },
                  }); } },
            { "ui", false, YamlParser::Map, [&cd](YamlParser *p) {
                  p->parseFields({
                      { "enableTouchEmulation", false, YamlParser::Scalar, [](YamlParser *p) {
                            qCDebug(LogDeployment) << "ignoring 'enableTouchEmulation'";
                            (void) p->parseScalar(); } },
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
                                      cd->ui.opengl.insert(u"desktopProfile"_s, p->parseScalar().toString()); } },
                                { "esMajorVersion", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->ui.opengl.insert(u"esMajorVersion"_s, p->parseScalar().toInt()); } },
                                { "esMinorVersion", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                                      cd->ui.opengl.insert(u"esMinorVersion"_s, p->parseScalar().toInt()); } }
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
                      { "installationDirMountPoint", false, YamlParser::Scalar | YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->applications.installationDirMountPoint = p->parseScalar().toString(); } },
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
                      { "allowUnknownUiClients", false, YamlParser::Scalar, [&cd](YamlParser *p) {
                            cd->flags.allowUnknownUiClients = p->parseScalar().toBool(); } },
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
                                          wes.insert(u"path"_s, p->parseScalar().toString()); } },
                                    { "permissions", false, YamlParser::Scalar, [&wes](YamlParser *p) {
                                          wes.insert(u"permissions"_s, p->parseScalar().toInt()); } },
                                    { "userId", false, YamlParser::Scalar, [&wes](YamlParser *p) {
                                          wes.insert(u"userId"_s, p->parseScalar().toInt()); } },
                                    { "groupId", false, YamlParser::Scalar, [&wes](YamlParser *p) {
                                          wes.insert(u"groupId"_s, p->parseScalar().toInt()); } }
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

                      auto rit = ifaceData.constFind(u"register"_s);
                      if (rit != ifaceData.cend())
                          cd->dbus.registrations.insert(ifaceName, rit->toString());

                      auto pit = ifaceData.constFind(u"policy"_s);
                      if (pit != ifaceData.cend())
                          cd->dbus.policies.insert(ifaceName, pit->toMap());
                  }
              } }
        };

        p.parseFields(fields);
        return cd.release();
    } catch (const Exception &e) {
        throw Exception(e.errorCode(), "Failed to parse config file %1: %2")
                .arg(!fileName.isEmpty() ? QDir().relativeFilePath(fileName) : u"<stream>"_s, e.errorString());
    }
}

QString Configuration::instanceId() const
{
    return value<QString>("instance-id", m_data->instanceId);
}

QString Configuration::mainQmlFile() const
{
    if (!m_clp.positionalArguments().isEmpty())
        return m_clp.positionalArguments().at(0);
    else
        return m_data->ui.mainQml;
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

QString Configuration::installationDirMountPoint() const
{
    return m_data->applications.installationDirMountPoint;
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

bool Configuration::allowUnknownUiClients() const
{
    return m_data->flags.allowUnknownUiClients;
}

bool Configuration::noUiWatchdog() const
{
    return value<bool>("no-ui-watchdog", m_data->flags.noUiWatchdog) || m_forceNoUiWatchdog;
}

void Configuration::setForceNoUiWatchdog(bool noUiWatchdog)
{
    m_forceNoUiWatchdog = noUiWatchdog;
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
    if (val.metaType() == QMetaType::fromType<bool>())
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

QString Configuration::dltId() const
{
    return value<QString>(nullptr, m_data->logging.dlt.id);
}

QString Configuration::dltDescription() const
{
    return value<QString>(nullptr, m_data->logging.dlt.description);
}

QString Configuration::dltLongMessageBehavior() const
{
    return value<QString>(nullptr, m_data->logging.dlt.longMessageBehavior);
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

QStringList Configuration::runtimeAdditionalLaunchers() const
{
    return m_data->runtimes.additionalLaunchers;
}

QVariantMap Configuration::runtimeConfigurations() const
{
    return m_data->runtimes.configurations;
}

QVariantMap Configuration::dbusPolicy(const char *interfaceName) const
{
    return m_data->dbus.policies.value(QString::fromLatin1(interfaceName)).toMap();
}

QString Configuration::dbusRegistration(const char *interfaceName) const
{
    auto hasConfig = m_data->dbus.registrations.constFind(QString::fromLatin1(interfaceName));

    if (hasConfig != m_data->dbus.registrations.cend())
        return hasConfig->toString();
    else
        return m_clp.value(u"dbus"_s);
}

QVariantMap Configuration::rawSystemProperties() const
{
    return m_data->systemProperties;
}

qreal Configuration::quickLaunchIdleLoad() const
{
    return m_data->quicklaunch.idleLoad;
}

QHash<std::pair<QString, QString>, int> Configuration::quickLaunchRuntimesPerContainer() const
{
    return m_data->quicklaunch.runtimesPerContainer;
}

int Configuration::quickLaunchFailedStartLimit() const
{
    return m_data->quicklaunch.failedStartLimit;
}

int Configuration::quickLaunchFailedStartLimitIntervalSec() const
{
    return m_data->quicklaunch.failedStartLimitIntervalSec;
}

QString Configuration::waylandSocketName() const
{
    QString socketName = m_clp.value(u"wayland-socket-name"_s); // get the default value
    if (!socketName.isEmpty())
        return socketName;

    if (!m_data->wayland.socketName.isEmpty())
        return m_data->wayland.socketName;

#if defined(Q_OS_LINUX)
    // modelled after wl_socket_lock() in wayland_server.c
    const QString xdgDir = qEnvironmentVariable("XDG_RUNTIME_DIR") + u"/"_s;
    const QString pattern = u"qtam-wayland-%1"_s;
    const QString lockSuffix = u".lock"_s;

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
    if (!targs.isEmpty() && targs.constFirst().endsWith(u".qml"))
        targs.removeFirst();
    targs.prepend(QCoreApplication::arguments().constFirst());
    return targs;
}

QString Configuration::testRunnerSourceFile() const
{
    return m_clp.value(u"qmltestrunner-source-file"_s);
}

QT_END_NAMESPACE_AM
